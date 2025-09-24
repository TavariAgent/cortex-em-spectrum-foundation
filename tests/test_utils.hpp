#pragma once
#include <cstdint>
#include <vector>
#include <cstring>
#include <string>
#include "screen_capture_win.hpp"

namespace test_utils {

    // Create a BGRA image (top-down) filled with a color
    inline cortex::RawImage make_bgra(size_t w, size_t h, uint8_t b, uint8_t g, uint8_t r, uint8_t a = 255) {
        cortex::RawImage img;
        img.width = w;
        img.height = h;
        img.bgra.assign(w * h * 4, 0);
        for (size_t i = 0; i < w * h; ++i) {
            img.bgra[4*i + 0] = b;
            img.bgra[4*i + 1] = g;
            img.bgra[4*i + 2] = r;
            img.bgra[4*i + 3] = a;
        }
        return img;
    }

    inline void set_pixel(cortex::RawImage& img, size_t x, size_t y, uint8_t b, uint8_t g, uint8_t r, uint8_t a = 255) {
        if (x >= img.width || y >= img.height) return;
        size_t i = (y * img.width + x) * 4;
        img.bgra[i + 0] = b;
        img.bgra[i + 1] = g;
        img.bgra[i + 2] = r;
        img.bgra[i + 3] = a;
    }

} // namespace test_utils