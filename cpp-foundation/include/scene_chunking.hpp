#pragma once
#include <vector>
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <iostream>
#include "static_frame_generator.hpp"
#include "adaptive_gpu_delegation.hpp"

namespace cortex {

// Core flags the orchestrator maintains
struct SceneState {
    bool changing_pixel_data{false};     // True when current frame has dynamic regions
    bool requires_static_frame{true};    // Calibration waits while true
    bool chunk_divide{false};            // Enable chunking of dynamic areas
};

// Simple rectangle
struct Rect {
    size_t x{0}, y{0}, w{0}, h{0};
    size_t x1() const { return x + w; }
    size_t y1() const { return y + h; }
    bool empty() const { return (w == 0 || h == 0); }
};

// A contained, summable chunk of pixels in the scene
struct ChunkObject {
    Rect area;
    bool on_gpu{false};          // delegation decision
    bool use_sum_mode{false};    // if true, add into destination (with normalization); else overwrite

    // Compose source into destination frame in the chunk rect
    // If use_sum_mode is true, sum and clamp to [0,1] to retain normalization.
    static void compose(const ElectromagneticFrame& src, ElectromagneticFrame& dst,
                        const Rect& r, bool sum_mode) {
        if (r.empty()) return;
        const size_t W = dst.width;
        const size_t H = dst.height;
        const size_t x0 = std::min(r.x, W);
        const size_t y0 = std::min(r.y, H);
        const size_t x1 = std::min(r.x1(), W);
        const size_t y1 = std::min(r.y1(), H);

        auto clamp01 = [](double v){ return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); };

        for (size_t y = y0; y < y1; ++y) {
            for (size_t x = x0; x < x1; ++x) {
                const size_t idx = y * W + x;
                if (!sum_mode) {
                    dst.pixels[idx] = src.pixels[idx];
                } else {
                    // Sum and renormalize to stay in displayable range
                    double rsum = static_cast<double>(dst.pixels[idx].red)   + static_cast<double>(src.pixels[idx].red);
                    double gsum = static_cast<double>(dst.pixels[idx].green) + static_cast<double>(src.pixels[idx].green);
                    double bsum = static_cast<double>(dst.pixels[idx].blue)  + static_cast<double>(src.pixels[idx].blue);
                    dst.pixels[idx].red   = CosmicPrecision(clamp01(rsum));
                    dst.pixels[idx].green = CosmicPrecision(clamp01(gsum));
                    dst.pixels[idx].blue  = CosmicPrecision(clamp01(bsum));
                }
            }
        }
    }
};

// Build chunk objects from a tile dirty mask (1 = dynamic) and tile geometry
inline std::vector<ChunkObject> build_chunks_from_tile_mask(
    const std::vector<uint8_t>& tile_dirty_mask,
    size_t frame_w, size_t frame_h,
    size_t tile_w, size_t tile_h,
    bool prefer_gpu = true
) {
    std::vector<ChunkObject> chunks;
    if (tile_dirty_mask.empty() || tile_w == 0 || tile_h == 0) return chunks;

    const size_t tiles_x = (frame_w + tile_w - 1) / tile_w;
    const size_t tiles_y = (frame_h + tile_h - 1) / tile_h;

    // Create one chunk per dirty tile (simple strategy; can be coalesced later)
    for (size_t ty = 0; ty < tiles_y; ++ty) {
        for (size_t tx = 0; tx < tiles_x; ++tx) {
            const size_t tid = ty * tiles_x + tx;
            if (tid >= tile_dirty_mask.size()) break;
            if (tile_dirty_mask[tid] == 0) continue;

            Rect r;
            r.x = tx * tile_w;
            r.y = ty * tile_h;
            r.w = std::min(tile_w, frame_w - r.x);
            r.h = std::min(tile_h, frame_h - r.y);

            chunks.push_back(ChunkObject{r, prefer_gpu, /*use_sum_mode*/false});
        }
    }
    return chunks;
}

// CPU shader for a rectangular region into dst frame (overwrites dst pixels in rect)
inline void shade_rect_cpu(StaticFrameGenerator& gen, ElectromagneticFrame& dst, const Rect& r) {
    if (r.empty()) return;
    const size_t W = dst.width;
    const size_t H = dst.height;
    const CosmicPrecision vmin = VIOLET_MIN_WAVELENGTH;
    const CosmicPrecision vmax = RED_MAX_WAVELENGTH;
    const CosmicPrecision invW = CosmicPrecision(1) / CosmicPrecision(W);

    for (size_t y = r.y; y < std::min(r.y1(), H); ++y) {
        for (size_t x = r.x; x < std::min(r.x1(), W); ++x) {
            CosmicPrecision xN = (CosmicPrecision(x) + CosmicPrecision("0.5")) * invW;
            CosmicPrecision wl = vmin + (vmax - vmin) * xN;
            CosmicPixel p = gen.wavelength_to_rgb_pixel(wl);
            dst.pixels[y * W + x] = p;
        }
    }
}

// Decide CPU vs GPU per chunk using AdaptiveGPUDelegator
inline void render_chunk(AdaptiveGPUDelegator& delegator,
                         StaticFrameGenerator& gen,
                         ElectromagneticFrame& dst,
                         const Rect& r)
{
    const size_t area = r.w * r.h;
    const bool use_gpu = delegator.is_cuda_initialized()
        && delegator.should_use_gpu("advanced_pixel_processing", area, /*complexity*/ 5000.0);

    if (use_gpu) {
        // Placeholder: GPU render path would go here.
        // For now, CPU fallback does the work to keep flow unblocked.
        // You’ll replace this with a CUDA kernel that writes into dst.pixels[r].
        shade_rect_cpu(gen, dst, r);
    } else {
        shade_rect_cpu(gen, dst, r);
    }
}

} // namespace cortex