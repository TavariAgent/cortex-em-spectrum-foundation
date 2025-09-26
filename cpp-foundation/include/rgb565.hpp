#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>

namespace cortex {

    // Convert one tile (BGRA8 contiguous rows) to RGB565 (little-endian) into out
    inline void bgra_tile_to_rgb565(const uint8_t* src_bgra,
                                    size_t tile_w, size_t tile_h,
                                    size_t full_row_stride_bytes,
                                    std::vector<uint8_t>& out) {
        out.resize(tile_w * tile_h * 2);
        uint8_t* dst = out.data();
        for (size_t y = 0; y < tile_h; ++y) {
            const uint8_t* row = src_bgra + y * full_row_stride_bytes;
            for (size_t x = 0; x < tile_w; ++x) {
                const uint8_t* p = &row[x * 4];
                uint16_t r = p[2] >> 3;
                uint16_t g = p[1] >> 2;
                uint16_t b = p[0] >> 3;
                uint16_t packed = static_cast<uint16_t>((r << 11) | (g << 5) | b);
                *dst++ = static_cast<uint8_t>(packed & 0xFF);
                *dst++ = static_cast<uint8_t>(packed >> 8);
            }
        }
    }

    // Blit an RGB565 tile back into a BGRA8 target (alpha=255)
    inline void rgb565_tile_to_bgra(const uint8_t* src565,
                                    size_t tile_w, size_t tile_h,
                                    uint8_t* dst_bgra,
                                    size_t full_row_stride_bytes) {
        for (size_t y = 0; y < tile_h; ++y) {
            uint8_t* row = dst_bgra + y * full_row_stride_bytes;
            for (size_t x = 0; x < tile_w; ++x) {
                uint16_t packed = static_cast<uint16_t>(src565[2 * (y * tile_w + x)]
                                                      | (src565[2 * (y * tile_w + x) + 1] << 8));
                uint8_t r = static_cast<uint8_t>(((packed >> 11) & 0x1F) << 3);
                uint8_t g = static_cast<uint8_t>(((packed >> 5)  & 0x3F) << 2);
                uint8_t b = static_cast<uint8_t>(( packed        & 0x1F) << 3);
                uint8_t* d = &row[x * 4];
                d[0] = b; d[1] = g; d[2] = r; d[3] = 255;
            }
        }
    }

} // namespace cortex