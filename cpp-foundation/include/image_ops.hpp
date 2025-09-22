#pragma once
#include <vector>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include "static_frame_generator.hpp"
#include "screen_capture_win.hpp"

namespace cortex {

// Bilinear resize BGRA -> BGRA
inline RawImage resize_bgra_bilinear(const RawImage& src, size_t newW, size_t newH) {
    RawImage dst;
    if (!src.ok() || newW == 0 || newH == 0) return dst;
    dst.width = newW; dst.height = newH;
    dst.bgra.resize(newW * newH * 4);

    const double sx = static_cast<double>(src.width) / static_cast<double>(newW);
    const double sy = static_cast<double>(src.height) / static_cast<double>(newH);

    auto sample = [&](double fx, double fy, int c)->double {
        fx = std::clamp(fx, 0.0, static_cast<double>(src.width - 1));
        fy = std::clamp(fy, 0.0, static_cast<double>(src.height - 1));
        const int x0 = static_cast<int>(std::floor(fx));
        const int y0 = static_cast<int>(std::floor(fy));
        const int x1 = std::min(x0 + 1, static_cast<int>(src.width - 1));
        const int y1 = std::min(y0 + 1, static_cast<int>(src.height - 1));
        const double tx = fx - x0;
        const double ty = fy - y0;
        const uint8_t* p00 = &src.bgra[(y0 * src.width + x0) * 4];
        const uint8_t* p10 = &src.bgra[(y0 * src.width + x1) * 4];
        const uint8_t* p01 = &src.bgra[(y1 * src.width + x0) * 4];
        const uint8_t* p11 = &src.bgra[(y1 * src.width + x1) * 4];
        const double a = (1.0 - tx) * p00[c] + tx * p10[c];
        const double b = (1.0 - tx) * p01[c] + tx * p11[c];
        return (1.0 - ty) * a + ty * b;
    };

    for (size_t y = 0; y < newH; ++y) {
        for (size_t x = 0; x < newW; ++x) {
            const double fx = (x + 0.5) * sx - 0.5;
            const double fy = (y + 0.5) * sy - 0.5;
            uint8_t* d = &dst.bgra[(y * newW + x) * 4];
            d[0] = static_cast<uint8_t>(std::clamp(sample(fx, fy, 0), 0.0, 255.0)); // B
            d[1] = static_cast<uint8_t>(std::clamp(sample(fx, fy, 1), 0.0, 255.0)); // G
            d[2] = static_cast<uint8_t>(std::clamp(sample(fx, fy, 2), 0.0, 255.0)); // R
            d[3] = 255;
        }
    }
    return dst;
}

inline ElectromagneticFrame bgra_to_frame(const RawImage& img) {
    ElectromagneticFrame f(img.width, img.height);
    f.pixels.resize(img.width * img.height);
    for (size_t i = 0; i < img.width * img.height; ++i) {
        const uint8_t b = img.bgra[4*i + 0];
        const uint8_t g = img.bgra[4*i + 1];
        const uint8_t r = img.bgra[4*i + 2];
        const double rd = r / 255.0, gd = g / 255.0, bd = b / 255.0;
        f.pixels[i] = CosmicPixel(
            CosmicPrecision(rd),
            CosmicPrecision(gd),
            CosmicPrecision(bd),
            CosmicPrecision(1)
        );
        f.total_energy += f.pixels[i].red + f.pixels[i].green + f.pixels[i].blue;
    }
    f.spectrum_range = RED_MAX_WAVELENGTH - VIOLET_MIN_WAVELENGTH;
    return f;
}

} // namespace cortex