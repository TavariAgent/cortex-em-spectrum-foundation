#pragma once
#include "intelligent_term_delegator.hpp"
#include "precision_safe_threading.hpp"
#include "context_overflow_guard.hpp"
#include "adaptive_gpu_delegation.hpp"
#include <vector>
#include <string>
#include <iostream>

namespace cortex {
namespace frames {

using CosmicPrecision = boost::multiprecision::cpp_dec_float<141>;

struct PixelRGB {
    CosmicPrecision red;
    CosmicPrecision green;
    CosmicPrecision blue;
    CosmicPrecision alpha;

    PixelRGB();
    PixelRGB(const CosmicPrecision& r, const CosmicPrecision& g,
             const CosmicPrecision& b, const CosmicPrecision& a = CosmicPrecision("1"));
};

struct StaticFrame {
    int width;
    int height;
    std::vector<PixelRGB> pixels;
    CosmicPrecision total_energy;
    std::string spectrum_range;

    StaticFrame();
    void reserve_pixels();
    PixelRGB& get_pixel(int x, int y);
    const PixelRGB& get_pixel(int x, int y) const;
};

class StaticFrameGenerator {
private:
    // Your Python optimization foundations translated to C++
    delegation::IntelligentTermDelegator term_delegator;
    threading::PrecisionSafeThreading precision_threading;
    overflow::ContextOverflowGuard overflow_guard;
    gpu::AdaptiveGPUDelegator gpu_delegator;

public:
    explicit StaticFrameGenerator();

    // Method declarations only
    StaticFrame generate_em_spectrum_frame(int width = 800, int height = 600);
    PixelRGB wavelength_to_rgb_pixel(const CosmicPrecision& wavelength_nm);
    CosmicPrecision wavelength_to_rgb_intensity(const CosmicPrecision& wavelength_nm);
    StaticFrame generate_test_frame(int width = 400, int height = 300);
    void save_frame_data(const StaticFrame& frame, const std::string& filename);
};

} // namespace frames
} // namespace cortex