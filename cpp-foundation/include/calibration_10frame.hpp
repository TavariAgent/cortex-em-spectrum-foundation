#pragma once
#include <vector>
#include <cstdint>
#include <chrono>
#include <thread>
#include <numeric>
#include <algorithm>
#include <cmath>
#include <iostream>
#include "screen_capture_win.hpp"
#include "image_ops.hpp"
#include "static_frame_generator.hpp"
#include "ppm_io.hpp"

namespace cortex {

struct CalibrationParams {
    double gain_r{1.0}, gain_g{1.0}, gain_b{1.0};
    double gamma{2.2};
    double avg_luma{0.5};
};

struct CalibrationResult {
    ElectromagneticFrame average_frame;
    CalibrationParams params;
    size_t frames_used{0};

    // Ensure average_frame can be constructed
    CalibrationResult(size_t w, size_t h) : average_frame(w, h) {}
    CalibrationResult() : average_frame(0, 0) {}
};

inline double luma_rec709(const double r, double g, double b) {
    return 0.2126*r + 0.7152*g + 0.0722*b;
}

// Estimate gamma by mapping the median luminance to 0.5 on a [0,1] scale
inline double estimate_gamma_from_median(double median_luma01) {
    const double x = std::clamp(median_luma01, 1e-4, 0.9999);
    // Solve x^(1/gamma) = 0.5  =>  1/gamma = log(0.5)/log(x)
    const double inv_gamma = std::log(0.5) / std::log(x);
    const double gamma = (inv_gamma != 0.0) ? 1.0 / inv_gamma : 2.2;
    // Clamp to a sane display gamma range
    return std::clamp(gamma, 1.6, 2.6);
}

inline CalibrationResult calibrate_primary_monitor_10(
    size_t target_w, size_t target_h,
    int frames = 10,
    int interval_ms = 80,
    bool save_average_ppm = true
) {
    CalibrationResult out; // now valid (average_frame is constructed as 0x0)
    out.frames_used = 0;

    if (target_w == 0 || target_h == 0 || frames <= 0) {
        std::cout << "Calibration skipped: invalid size/frames\n";
        return out;
    }

    // Running sums for mean and histogram
    std::vector<double> sum_r(target_w * target_h, 0.0);
    std::vector<double> sum_g(target_w * target_h, 0.0);
    std::vector<double> sum_b(target_w * target_h, 0.0);

    std::vector<size_t> hist(256, 0);

    for (int i = 0; i < frames; ++i) {
        RawImage cap = capture_primary_monitor_bgra();
        if (!cap.ok()) {
            std::cout << "Calibration capture failed on frame " << i << "\n";
            continue;
        }
        RawImage small = resize_bgra_bilinear(cap, target_w, target_h);

        for (size_t p = 0; p < target_w * target_h; ++p) {
            const uint8_t b = small.bgra[4*p + 0];
            const uint8_t g = small.bgra[4*p + 1];
            const uint8_t r = small.bgra[4*p + 2];
            sum_r[p] += r / 255.0;
            sum_g[p] += g / 255.0;
            sum_b[p] += b / 255.0;

            const double Y = luma_rec709(r/255.0, g/255.0, b/255.0);
            const int bin = std::clamp(static_cast<int>(std::round(Y * 255.0)), 0, 255);
            hist[bin]++;
        }

        out.frames_used++;
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }

    if (out.frames_used == 0) {
        std::cout << "Calibration captured 0 frames.\n";
        return out;
    }

    // Build average frame + stats
    ElectromagneticFrame avg(target_w, target_h);
    avg.pixels.resize(target_w * target_h);

    double mean_r = 0.0, mean_g = 0.0, mean_b = 0.0, mean_luma = 0.0;
    for (size_t p = 0; p < target_w * target_h; ++p) {
        double r = sum_r[p] / out.frames_used;
        double g = sum_g[p] / out.frames_used;
        double b = sum_b[p] / out.frames_used;

        avg.pixels[p] = CosmicPixel(
            CosmicPrecision(r),
            CosmicPrecision(g),
            CosmicPrecision(b),
            CosmicPrecision(1)
        );
        avg.total_energy += avg.pixels[p].red + avg.pixels[p].green + avg.pixels[p].blue;

        mean_r += r; mean_g += g; mean_b += b;
        mean_luma += luma_rec709(r, g, b);
    }
    mean_r /= (target_w * target_h);
    mean_g /= (target_w * target_h);
    mean_b /= (target_w * target_h);
    mean_luma /= (target_w * target_h);
    avg.spectrum_range = RED_MAX_WAVELENGTH - VIOLET_MIN_WAVELENGTH;

    // White balance (gray-world)
    const double gray = (mean_r + mean_g + mean_b) / 3.0;
    CalibrationParams params;
    params.gain_r = (mean_r > 1e-6) ? (gray / mean_r) : 1.0;
    params.gain_g = (mean_g > 1e-6) ? (gray / mean_g) : 1.0;
    params.gain_b = (mean_b > 1e-6) ? (gray / mean_b) : 1.0;
    params.avg_luma = mean_luma;

    // Median luma from histogram
    size_t total = std::accumulate(hist.begin(), hist.end(), static_cast<size_t>(0));
    size_t target = total / 2;
    size_t accum = 0;
    int median_bin = 127;
    for (int i = 0; i < 256; ++i) {
        accum += hist[i];
        if (accum >= target) { median_bin = i; break; }
    }
    params.gamma = estimate_gamma_from_median(median_bin / 255.0);

    out.average_frame = std::move(avg);
    out.params = frames > 0 ? params : CalibrationParams{};

    if (save_average_ppm) {
        write_ppm_p6("monitor_calibration_avg.ppm", out.average_frame);
    }

    std::cout << "Calibration complete (" << out.frames_used << " frames):\n"
              << "  mean R/G/B = " << mean_r << " / " << mean_g << " / " << mean_b << "\n"
              << "  WB gains   = " << params.gain_r << " / " << params.gain_g << " / " << params.gain_b << "\n"
              << "  avg luma   = " << params.avg_luma << "\n"
              << "  gamma est  = " << params.gamma << "\n";

    return out;
}

} // namespace cortex