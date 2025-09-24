#pragma once
#include <memory>
#include <vector>
#include "static_frame_generator.hpp"
#include "spsc_ring.hpp"

namespace cortex {

    // Lightweight unit for offload
    struct SubpixelChunk {
        size_t x, y, w, h;    // ROI in pixels
        int    spp_accum;     // accumulated subpixel passes (up to cap)
    };

    // Independent caches
    class FrameCache {
    public:
        // Static frames (non-deviated tiles resolved into full frames if you want)
        SpscRing<ElectromagneticFrame> static_frames{256};

        // ROI chunks produced when tiles deviate
        SpscRing<SubpixelChunk> roi_chunks{4096};
    };

} // namespace cortex