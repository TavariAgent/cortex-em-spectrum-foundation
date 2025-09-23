#pragma once
#include <vector>
#include <cmath>
#include <cstdint>
#include <memory>
#include <algorithm>
#include "static_frame_generator.hpp"
#include "frame_cache.hpp"

namespace cortex {

// Tile grid that tracks subpixel accumulation and deviation.
// Policy: if any channel delta > threshold (fraction of full scale) in a tile, mark deviated.
// Accumulation cap: up to 4x subpixel passes per tile before forced release.
class DeviationRouter {
public:
    struct Config {
        size_t tile_w{64};
        size_t tile_h{32};
        double threshold{0.05}; // 5% (any channel)
        int    subpixel_cap{4}; // up to 4x accumulation per tile
    };

    explicit DeviationRouter(const Config& cfg = {}) : cfg_(cfg) {}

    void set_resolution(size_t w, size_t h) {
        W_ = w; H_ = h;
        gx_ = (W_ + cfg_.tile_w - 1) / cfg_.tile_w;
        gy_ = (H_ + cfg_.tile_h - 1) / cfg_.tile_h;
        state_.assign(gx_ * gy_, TileState{});
    }

    // Analyze current vs previous frame and emit ROIs to cache.
    // If prev == nullptr, only accumulate.
    void analyze_and_route(const ElectromagneticFrame& curr,
                           const ElectromagneticFrame* prev,
                           FrameCache& cache) {
        if (W_ != curr.width || H_ != curr.height) set_resolution(curr.width, curr.height);

        for (size_t ty = 0; ty < gy_; ++ty) {
            for (size_t tx = 0; tx < gx_; ++tx) {
                const size_t idx = ty*gx_ + tx;
                auto& st = state_[idx];
                const TileRect r = rect_of_tile(tx, ty);

                const bool deviated = prev ? tile_deviated(*prev, curr, r, cfg_.threshold) : false;

                if (deviated) {
                    // Release accumulated subpixels as a chunk and reset
                    st.spp_accum = std::min(st.spp_accum, cfg_.subpixel_cap);
                    SubpixelChunk c{r.x, r.y, r.w, r.h, st.spp_accum};
                    cache.roi_chunks.push(std::make_shared<SubpixelChunk>(c));
                    st.spp_accum = 0;
                    st.deviated_last = true;
                } else {
                    // No deviation: accumulate subpixel passes up to cap
                    st.spp_accum = std::min(st.spp_accum + 1, cfg_.subpixel_cap);
                    st.deviated_last = false;

                    // Optional: when cap reached and still stable, consider emitting
                    // a static-frame contribution or keep it local until next deviation.
                    // For now we do nothing (keeps CPU priority on full frames).
                }
            }
        }
    }

private:
    struct TileRect { size_t x,y,w,h; };
    struct TileState {
        int  spp_accum{0};
        bool deviated_last{false};
    };

    Config cfg_;
    size_t W_{0}, H_{0}, gx_{0}, gy_{0};
    std::vector<TileState> state_;

    TileRect rect_of_tile(size_t tx, size_t ty) const {
        const size_t x = tx * cfg_.tile_w;
        const size_t y = ty * cfg_.tile_h;
        const size_t w = std::min(cfg_.tile_w, W_ - x);
        const size_t h = std::min(cfg_.tile_h, H_ - y);
        return {x,y,w,h};
    }

    // 5% rule on any channel (CosmicPrecision -> double). Early exits on hit.
    static bool tile_deviated(const ElectromagneticFrame& prev,
                              const ElectromagneticFrame& curr,
                              const TileRect& r,
                              double thr) {
        const double t = thr;
        for (size_t yy = 0; yy < r.h; ++yy) {
            const size_t row = (r.y + yy) * curr.width;
            for (size_t xx = 0; xx < r.w; ++xx) {
                const size_t i = row + (r.x + xx);
                const auto& cp = curr.pixels[i];
                const auto& pp = prev.pixels[i];
                const double dr = std::abs(static_cast<double>(cp.red   - pp.red));
                const double dg = std::abs(static_cast<double>(cp.green - pp.green));
                const double db = std::abs(static_cast<double>(cp.blue  - pp.blue));
                if (dr > t || dg > t || db > t) return true;
            }
        }
        return false;
    }
};

} // namespace cortex