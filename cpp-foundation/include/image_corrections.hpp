#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
#include "static_frame_generator.hpp"
#include "calibration_10frame.hpp"

namespace cortex {

// Auto-exposure: target mean luma and protect highlights via 99th percentile
inline double compute_auto_exposure(const ElectromagneticFrame& frame, const CalibrationParams& p, const double target_luma = 0.18) {
    if (frame.pixels.empty()) return 1.0;

    const size_t N = frame.pixels.size();
    std::vector<size_t> hist(256, 0);
    double sum_luma = 0.0;

    for (const auto& px : frame.pixels) {
        double r = clamp01(static_cast<double>(px.red)   * p.gain_r);
        double g = clamp01(static_cast<double>(px.green) * p.gain_g);
        double b = clamp01(static_cast<double>(px.blue)  * p.gain_b);
        sum_luma += luma_rec709(r, g, b);
        const double mx = std::max({r, g, b});
        const int bin = std::clamp(static_cast<int>(mx * 255.0 + 0.5), 0, 255);
        hist[bin]++;
    }

    double mean = sum_luma / static_cast<double>(N);

    const size_t total = N;
    // 99th percentile of max channel
    size_t acc = 0, cutoff = static_cast<size_t>(std::ceil(0.99 * total));
    int p99_bin = 255;
    for (int i = 0; i <= 255; ++i) { acc += hist[i]; if (acc >= cutoff) { p99_bin = i; break; } }
    const double p99 = std::max(1e-6, p99_bin / 255.0);

    const double e1 = (mean > 1e-9) ? (target_luma / mean) : 1.0;
    const double e2 = 0.98 / p99;
    return std::min(e1, e2);
}

inline void apply_corrections_inplace(ElectromagneticFrame& frame, const CalibrationParams& p, double exposure = 1.0) {
    const double inv_gamma = (p.gamma > 0.0) ? (1.0 / p.gamma) : (1.0 / 2.2);
    for (auto& px : frame.pixels) {
        double r = clamp01(static_cast<double>(px.red)   * p.gain_r * exposure);
        double g = clamp01(static_cast<double>(px.green) * p.gain_g * exposure);
        double b = clamp01(static_cast<double>(px.blue)  * p.gain_b * exposure);
        r = std::pow(r, inv_gamma); g = std::pow(g, inv_gamma); b = std::pow(b, inv_gamma);
        px.red = CosmicPrecision(r); px.green = CosmicPrecision(g); px.blue = CosmicPrecision(b);
    }
}

} // namespace cortex