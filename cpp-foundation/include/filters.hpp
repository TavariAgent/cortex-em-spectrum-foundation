#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "screen_capture_win.hpp" // RawImage

namespace cortex {

struct FrameFilters {
    bool   grayscale{false};
    double brightness{0.0};   // [-1.0, 1.0] add
    double contrast{1.0};     // [0.0, 3.0] multiply around 0.5
    double gamma{1.0};        // gamma correction (>= 0.1)
    bool   pixelate{false};
    int    pixel_size{4};     // >= 2
};

// Helpers
inline uint8_t clamp_u8(int v) { return static_cast<uint8_t>(std::min(255, std::max(0, v))); }
inline uint8_t clamp_u8d(double v) { return static_cast<uint8_t>(std::min(255.0, std::max(0.0, v))); }

// Apply grayscale in-place (BGRA8)
inline void apply_grayscale(RawImage& img) {
    const size_t n = img.width * img.height;
    for (size_t i = 0; i < n; ++i) {
        uint8_t* p = &img.bgra[i*4];
        // rec. 601 luma-ish
        int gray = static_cast<int>(0.299*p[2] + 0.587*p[1] + 0.114*p[0] + 0.5);
        p[0] = p[1] = p[2] = clamp_u8(gray);
    }
}

// Apply brightness/contrast (in-place, BGRA8)
// brightness adds in [0..255] space (scaled by 255)
// contrast multiplies around 0.5 -> scaled by 255
inline void apply_bc(RawImage& img, double brightness, double contrast) {
    const int add = static_cast<int>(brightness * 255.0); // [-255..255]
    const double c = contrast; // e.g., 1.0 = no change
    const size_t n = img.width * img.height;
    for (size_t i = 0; i < n; ++i) {
        uint8_t* p = &img.bgra[i*4];
        for (int ch = 0; ch < 3; ++ch) {
            // scale around 128 (0.5 in 0..255)
            int v = static_cast<int>(std::round((p[ch] - 128) * c + 128)) + add;
            p[ch] = clamp_u8(v);
        }
    }
}

// Gamma correction (in-place). gamma>=0.1.
// Operates on normalized [0..1], converts back to [0..255]
inline void apply_gamma(RawImage& img, double gamma) {
    if (gamma <= 0.0) return;
    const double inv = 1.0 / gamma;
    // Precompute LUT
    uint8_t lut[256];
    for (int i = 0; i < 256; ++i) {
        double nrm = std::pow(static_cast<double>(i) / 255.0, inv);
        lut[i] = clamp_u8d(nrm * 255.0);
    }
    const size_t n = img.width * img.height;
    for (size_t i = 0; i < n; ++i) {
        uint8_t* p = &img.bgra[i*4];
        p[0] = lut[p[0]];
        p[1] = lut[p[1]];
        p[2] = lut[p[2]];
    }
}

// Blocky pixelate (simple box kernel)
inline void apply_pixelate(RawImage& img, int block) {
    if (block < 2) return;
    const size_t W = img.width, H = img.height;
    for (size_t y = 0; y < H; y += block) {
        for (size_t x = 0; x < W; x += block) {
            // Sample top-left as representative
            const uint8_t* src = &img.bgra[(y*W + x)*4];
            for (size_t yy = y; yy < std::min(H, y + (size_t)block); ++yy) {
                for (size_t xx = x; xx < std::min(W, x + (size_t)block); ++xx) {
                    uint8_t* d = &img.bgra[(yy*W + xx)*4];
                    d[0] = src[0]; d[1] = src[1]; d[2] = src[2];
                }
            }
        }
    }
}

// Full filter chain (in-place), cheap and branch-light
inline void apply_filters_inplace(RawImage& img, const FrameFilters& f) {
    if (f.grayscale) apply_grayscale(img);
    if (f.contrast != 1.0 || std::abs(f.brightness) > 1e-9) apply_bc(img, f.brightness, f.contrast);
    if (std::abs(f.gamma - 1.0) > 1e-9) apply_gamma(img, f.gamma);
    if (f.pixelate) apply_pixelate(img, std::max(2, f.pixel_size));
}

} // namespace cortex