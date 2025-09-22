#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace cortex {

    struct SpectrumCudaParams {
        int width{0};
        int height{0};
        int spp_x{2};
        int spp_y{2};
        bool jitter{false};
        double gamma{2.2};
        double wl_min{380.0};
        double wl_max{750.0};
    };

    // Host API:
    // - out_rgb must be size >= width*height*3 floats (RGB, row-major).
    // - Returns true if CUDA executed; false means “not available” (you can fall back to CPU).
    bool spectrum_shade_cuda(const SpectrumCudaParams& p, float* out_rgb);

} // namespace cortex