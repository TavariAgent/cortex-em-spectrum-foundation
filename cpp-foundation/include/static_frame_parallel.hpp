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
#include "color_math_fast.hpp"

namespace cortex {

struct RGBWAccumulator {
    CosmicPrecision r{0}, g{0}, b{0}, w{0};
    void add_with_cap(const CosmicPixel& p, const CosmicPrecision& weight, double max_w_cap) {
        r += p.red   * weight;
        g += p.green * weight;
        b += p.blue  * weight;
        w += weight;
        if (const double wd = static_cast<double>(w); wd > max_w_cap) {
            const CosmicPrecision cap(max_w_cap);
            const CosmicPixel avg = to_pixel();
            r = avg.red   * cap;
            g = avg.green * cap;
            b = avg.blue  * cap;
            w = cap;
        }
    }

    CosmicPixel to_pixel() const {
        if (w == 0) return CosmicPixel(0,0,0,1);
        return CosmicPixel(r / w, g / w, b / w, 1);
    }

    void clear() { r = g = b = w = CosmicPrecision(0); }
};

struct OperandMap {
    std::vector<CosmicPrecision> amplitude;
    size_t width{0}, height{0};
    int frames_accumulated{0};
    void resize(const size_t w, const size_t h) {
        width = w; height = h;
        amplitude.assign(w * h, CosmicPrecision(0));
        frames_accumulated = 0;
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
        int threads = 0;                // 0 => auto (min 4, keep 2 free)
        RouterConfig router{};
        size_t spp_x = 2;
        size_t spp_y = 2;
        bool jitter = false;
        double max_accum_weight = 4.0;
        bool fast_color_math = true;    // use double + gamma LUT in inner loop
        int gamma_lut_size = 4096;
        double gamma_value = 2.2;       // for fast path only
    };

    explicit StaticFrameParallel(const Config& cfg = {})
        : cfg_(cfg), router_(cfg.router),
          gamma_lut_(1.0 / cfg.gamma_value, cfg.gamma_lut_size) {}

    void set_resolution(size_t w, size_t h) {
        width_ = w; height_ = h;
        tiles_.configure(w, h, cfg_.tile_w, cfg_.tile_h);
        accum_.assign(w * h, RGBWAccumulator{});
        prev_amp_.assign(w * h, CosmicPrecision(0));
        curr_amp_.assign(w * h, CosmicPrecision(0));
        tile_dirty_.assign(tiles_.tiles().size(), 1);
        op_map_.resize(w, h);
        router_.reset_calibration();
        initialized_ = true;
    }

    FrameParallelResult render_next_frame(StaticFrameGenerator& /*gen*/) {
        ensure_initialized();
        router_.begin_frame();

        const auto& T = tiles_.tiles();
        std::fill(curr_amp_.begin(), curr_amp_.end(), CosmicPrecision(0));

        // Parallel tile workers
        std::atomic<size_t> next_tile{0};
        const int hw = std::max(1u, std::thread::hardware_concurrency());
        const int threads = cfg_.threads > 0 ? cfg_.threads: std::max(4, static_cast<int>(std::max(1, hw - 2)));
        std::vector<std::thread> pool;
        pool.reserve(threads);

        std::vector<uint8_t> local_dirty(T.size(), 0);

        auto worker = [&]() {
            size_t ti;
            while ((ti = next_tile.fetch_add(1)) < T.size()) {
                process_tile_fast(T[ti], ti, local_dirty);
            }
        };

        for (int i = 0; i < threads; ++i) pool.emplace_back(worker);
        for (auto& th : pool) th.join();

        // Compose frame
        ElectromagneticFrame frame(width_, height_);
        frame.pixels.resize(width_ * height_);
        for (size_t i = 0; i < accum_.size(); ++i) {
            frame.pixels[i] = accum_[i].to_pixel();
            frame.total_energy += frame.pixels[i].red + frame.pixels[i].green + frame.pixels[i].blue;
        }
        frame.spectrum_range = RED_MAX_WAVELENGTH - VIOLET_MIN_WAVELENGTH;

        // Update amplitude history + (optional) calibration operands
        prev_amp_ = curr_amp_;
        if (!router_.is_calibrated()) {
            accumulate_operands_from_frame(frame);
        }

        tile_dirty_ = std::move(local_dirty);
        clear_dynamic_tiles_for_next_frame();

        return FrameParallelResult{ std::move(frame), tile_dirty_, router_.is_calibrated() };
    }

private:
    Config cfg_;
    size_t width_{0}, height_{0};
    bool initialized_{false};

    QuadArrayManager tiles_;
    OrSwitch router_;
    GammaLUT gamma_lut_;

    std::vector<RGBWAccumulator> accum_;
    std::vector<CosmicPrecision> prev_amp_;
    std::vector<CosmicPrecision> curr_amp_;
    std::vector<uint8_t> tile_dirty_;
    OperandMap op_map_;

    void ensure_initialized() { if (!initialized_) set_resolution(256, 256); }

    static CosmicPrecision amplitude_of(const CosmicPixel& p) {
        using boost::multiprecision::abs;
        return (abs(p.red) + abs(p.green) + abs(p.blue)) / CosmicPrecision(3);
    }

    // Double-based sampling inner loop; convert to CosmicPrecision once per pixel
    void process_tile_fast(const TileRect& R, size_t tile_index, std::vector<uint8_t>& local_dirty) {
        size_t changed = 0;
        const double VMIN = 380.0;
        const double RMAX = 750.0;

        for (size_t y = R.y0; y < R.y1; ++y) {
            for (size_t x = R.x0; x < R.x1; ++x) {
                double acc_r = 0.0, acc_g = 0.0, acc_b = 0.0;

                for (size_t sy = 0; sy < cfg_.spp_y; ++sy) {
                    for (size_t sx = 0; sx < cfg_.spp_x; ++sx) {
                        const double jx = cfg_.jitter ? rand01() : 0.5;
                        const double fx = (static_cast<double>(sx) + jx) / static_cast<double>(cfg_.spp_x);
                        const double xN = (static_cast<double>(x) + fx) / static_cast<double>(width_);
                        const double wl = VMIN + (RMAX - VMIN) * xN;

                        RGBd c = ColorMathFast::shade(wl, gamma_lut_);
                        acc_r += c.r; acc_g += c.g; acc_b += c.b;
                    }
                }

                const double inv_spp = 1.0 / static_cast<double>(cfg_.spp_x * cfg_.spp_y);
                const size_t idx = y * width_ + x;

                // Convert once to CosmicPrecision for pipeline semantics
                CosmicPixel p(
                    CosmicPrecision(acc_r * inv_spp),
                    CosmicPrecision(acc_g * inv_spp),
                    CosmicPrecision(acc_b * inv_spp),
                    CosmicPrecision(1)
                );

                curr_amp_[idx] = amplitude_of(p);

                const double eps = router_.is_calibrated() ? cfg_.router.epsilon : 0.0;
                if (gt_eps(prev_amp_[idx], curr_amp_[idx], eps)) ++changed;

                accum_[idx].add_with_cap(p, CosmicPrecision(1), cfg_.max_accum_weight);
            }
        }

        const size_t tile_pixels = R.width() * R.height();
        const double percent_changed = tile_pixels ? (100.0 * static_cast<double>(changed) / tile_pixels) : 0.0;

        router_.update_tile_change(tile_index, percent_changed);
        const Route route = router_.decide(tile_index);
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
            auto n = CosmicPrecision(op_map_.frames_accumulated);
            op_map_.amplitude[i] = (op_map_.frames_accumulated == 0)
                ? a
                : (op_map_.amplitude[i] * (n / (n + 1)) + a / (n + 1));
        }
        op_map_.frames_accumulated++;
    }

    static inline bool gt_eps(const CosmicPrecision& a, const CosmicPrecision& b, double eps) {
        const double ad = static_cast<double>(a);
        const double bd = static_cast<double>(b);
        return std::fabs(ad - bd) > eps;
    }

    // Minimal xorshift64* RNG -> [0,1)
    static inline double rand01() {
        static thread_local uint64_t s = 0x9E3779B97F4A7C15ull;
        s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
        const uint64_t z = s * 0x2545F4914F6CDD1Dull;
        return (z >> 11) * (1.0 / 9007199254740992.0);
    }
};

} // namespace cortex