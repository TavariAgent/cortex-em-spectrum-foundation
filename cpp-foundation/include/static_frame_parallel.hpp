#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>
#include <functional>
#include <cmath>
#include <algorithm>
#include "static_frame_generator.hpp"
#include "quad_array_manager.hpp"
#include "or_switch.hpp"

namespace cortex {

// Minimal accumulator for sharpening with cap
struct RGBWAccumulator {
    CosmicPrecision r{0}, g{0}, b{0}, w{0};

    inline void add_with_cap(const CosmicPixel& p, const CosmicPrecision& weight, double max_w_cap) {
        r += p.red   * weight;
        g += p.green * weight;
        b += p.blue  * weight;
        w += weight;

        double wd = static_cast<double>(w);
        if (wd > max_w_cap) {
            CosmicPrecision cap(max_w_cap);
            CosmicPixel avg = to_pixel();
            r = avg.red   * cap;
            g = avg.green * cap;
            b = avg.blue  * cap;
            w = cap;
        }
    }

    inline CosmicPixel to_pixel() const {
        if (w == 0) return CosmicPixel(0,0,0,1);
        return CosmicPixel(r / w, g / w, b / w, 1);
    }
    inline void clear() { r = g = b = w = CosmicPrecision(0); }
};

// Operand map (amplitude-first)
struct OperandMap {
    std::vector<CosmicPrecision> amplitude;
    size_t width{0}, height{0};
    int frames_accumulated{0};

    void resize(size_t w, size_t h) {
        width = w; height = h;
        amplitude.assign(w * h, CosmicPrecision(0));
        frames_accumulated = 0;
    }
};

struct TileChangeStats {
    size_t changed_pixels{0};
    size_t total_pixels{0};
    double percent_changed() const {
        return total_pixels ? (100.0 * static_cast<double>(changed_pixels) / static_cast<double>(total_pixels)) : 100.0;
    }
};

struct FrameParallelResult {
    ElectromagneticFrame frame;
    std::vector<uint8_t> tile_dirty_mask; // 1 = dynamic (>K), 0 = static
    bool calibration_complete{false};
};

class StaticFrameParallel {
public:
    struct Config {
        size_t tile_w = 32;
        size_t tile_h = 32;
        int threads = 4;              // enforce minimum 4 tile workers (can raise later)
        RouterConfig router{};
        size_t spp_x = 2;
        size_t spp_y = 2;
        bool jitter = true;
        double max_accum_weight = 4.0;  // clamp sharpening weight per pixel
    };

    explicit StaticFrameParallel(const Config& cfg = {}) : cfg_(cfg), router_(cfg.router) {}

    void set_resolution(size_t w, size_t h) {
        width_ = w; height_ = h;
        tiles_.configure(w, h, cfg_.tile_w, cfg_.tile_h);
        accum_.assign(w * h, RGBWAccumulator{});
        prev_amplitude_.assign(w * h, CosmicPrecision(0));
        curr_amplitude_.assign(w * h, CosmicPrecision(0));
        tile_dirty_.assign(tiles_.tiles().size(), 1);
        op_map_.resize(w, h);
        router_.reset_calibration();
        initialized_ = true;
    }

    // Render next frame (CPU), update operands and tile mask.
    FrameParallelResult render_next_frame(StaticFrameGenerator& gen) {
        ensure_initialized();
        router_.begin_frame(); // advance calibration once per frame

        const auto& T = tiles_.tiles();
        std::fill(curr_amplitude_.begin(), curr_amplitude_.end(), CosmicPrecision(0));

        // Parallel over tiles
        std::atomic<size_t> next_tile{0};
        std::vector<std::thread> pool;
        pool.reserve(std::max(1, cfg_.threads));

        std::vector<uint8_t> local_dirty(T.size(), 0);

        auto worker = [&]() {
            size_t ti;
            while ((ti = next_tile.fetch_add(1)) < T.size()) {
                process_tile(gen, T[ti], ti, local_dirty);
            }
        };

        const int thread_count = std::max(1, cfg_.threads);
        for (int i = 0; i < thread_count; ++i) pool.emplace_back(worker);
        for (auto& th : pool) th.join();

        // Compose frame from accumulators
        ElectromagneticFrame frame(width_, height_);
        frame.pixels.resize(width_ * height_);
        for (size_t i = 0; i < accum_.size(); ++i) {
            frame.pixels[i] = accum_[i].to_pixel();
            frame.total_energy += frame.pixels[i].red + frame.pixels[i].green + frame.pixels[i].blue;
        }
        frame.spectrum_range = RED_MAX_WAVELENGTH - VIOLET_MIN_WAVELENGTH;

        // Update prev amplitude and operand map (during calibration)
        prev_amplitude_ = curr_amplitude_;
        if (!router_.is_calibrated()) {
            accumulate_operands_from_frame(frame);
        }

        // Publish mask for this frame and clear sharpening for dynamic tiles for next frame
        tile_dirty_ = std::move(local_dirty);
        clear_dynamic_tiles_for_next_frame();

        FrameParallelResult out{ std::move(frame), tile_dirty_, router_.is_calibrated() };
        return out;
    }

private:
    Config cfg_;
    size_t width_{0}, height_{0};
    bool initialized_{false};

    QuadArrayManager tiles_;
    OrSwitch router_;

    std::vector<RGBWAccumulator> accum_;
    std::vector<CosmicPrecision> prev_amplitude_;
    std::vector<CosmicPrecision> curr_amplitude_;
    std::vector<uint8_t> tile_dirty_;
    OperandMap op_map_;

    void ensure_initialized() {
        if (!initialized_) set_resolution(256, 256);
    }

    static inline CosmicPrecision amplitude_of(const CosmicPixel& p) {
        using boost::multiprecision::abs;
        return (abs(p.red) + abs(p.green) + abs(p.blue)) / CosmicPrecision(3);
    }

    void process_tile(StaticFrameGenerator& gen, const TileRect& R, size_t tile_index, std::vector<uint8_t>& local_dirty) {
        TileChangeStats stats{0, R.width() * R.height()};

        for (size_t y = R.y0; y < R.y1; ++y) {
            for (size_t x = R.x0; x < R.x1; ++x) {
                CosmicPixel sum(0,0,0,1);
                for (size_t sy = 0; sy < cfg_.spp_y; ++sy) {
                    for (size_t sx = 0; sx < cfg_.spp_x; ++sx) {
                        double jx = cfg_.jitter ? rand01() : 0.5;
                        double jy = cfg_.jitter ? rand01() : 0.5;
                        double fx = (static_cast<double>(sx) + jx) / static_cast<double>(cfg_.spp_x);
                        double fy = (static_cast<double>(sy) + jy) / static_cast<double>(cfg_.spp_y);
                        (void)fy;

                        CosmicPrecision xN = (CosmicPrecision(x) + CosmicPrecision(fx)) / CosmicPrecision(width_);
                        CosmicPrecision wavelength = VIOLET_MIN_WAVELENGTH +
                            (RED_MAX_WAVELENGTH - VIOLET_MIN_WAVELENGTH) * xN;

                        CosmicPixel sp = gen.wavelength_to_rgb_pixel(wavelength);
                        sum.red   += sp.red;
                        sum.green += sp.green;
                        sum.blue  += sp.blue;
                    }
                }

                const CosmicPrecision spp = CosmicPrecision(cfg_.spp_x * cfg_.spp_y);
                sum.red   /= spp;
                sum.green /= spp;
                sum.blue  /= spp;

                size_t idx = y * width_ + x;
                curr_amplitude_[idx] = amplitude_of(sum);

                const double eps = router_.is_calibrated() ? cfg_.router.epsilon : 0.0;
                if (gt_eps(prev_amplitude_[idx], curr_amplitude_[idx], eps)) stats.changed_pixels++;

                accum_[idx].add_with_cap(sum, CosmicPrecision(1), cfg_.max_accum_weight);
            }
        }

        double percent_changed = stats.percent_changed();
        router_.update_tile_change(tile_index, percent_changed);
        Route route = router_.decide(tile_index);

        local_dirty[tile_index] = (route == Route::GPU) ? 1u : 0u;
    }

    void clear_dynamic_tiles_for_next_frame() {
        const auto& T = tiles_.tiles();
        for (size_t ti = 0; ti < T.size(); ++ti) {
            if (tile_dirty_[ti]) {
                const auto& R = T[ti];
                for (size_t y = R.y0; y < R.y1; ++y) {
                    for (size_t x = R.x0; x < R.x1; ++x) {
                        accum_[y * width_ + x].clear();
                    }
                }
            }
        }
    }

    void accumulate_operands_from_frame(const ElectromagneticFrame& frame) {
        if (op_map_.amplitude.size() != frame.pixels.size()) op_map_.resize(frame.width, frame.height);

        for (size_t i = 0; i < frame.pixels.size(); ++i) {
            CosmicPrecision a = amplitude_of(frame.pixels[i]);
            CosmicPrecision n = CosmicPrecision(op_map_.frames_accumulated);
            op_map_.amplitude[i] = (op_map_.frames_accumulated == 0)
                ? a
                : (op_map_.amplitude[i] * (n / (n + 1)) + a / (n + 1));
        }
        op_map_.frames_accumulated++;
    }

    static inline bool gt_eps(const CosmicPrecision& a, const CosmicPrecision& b, double eps) {
        double ad = static_cast<double>(a);
        double bd = static_cast<double>(b);
        return std::fabs(ad - bd) > eps;
    }

    // Minimal xorshift64* RNG -> [0,1)
    static inline double rand01() {
        static thread_local uint64_t s = 0x9E3779B97F4A7C15ull;
        s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
        uint64_t z = s * 0x2545F4914F6CDD1Dull;
        return (z >> 11) * (1.0 / 9007199254740992.0);
    }
};

} // namespace cortex