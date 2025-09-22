#pragma once
#include <vector>
#include <fstream>
#include <string>
#include <algorithm>
#include <cstdint>
#include "static_frame_generator.hpp"

namespace cortex {

// Clamp helpers
inline double clamp01(const double v) {
    if (v <= 0.0) return 0.0;
    if (v >= 1.0) return 1.0;
    return v;
}
inline unsigned char to_u8(const double v) {
    return static_cast<unsigned char>(clamp01(v) * 255.0 + 0.7);
}
inline uint16_t to_u16(const double v) {
    // PPM 16-bit uses maxval up to 65535, big-endian per spec
    return static_cast<uint16_t>(clamp01(v) * 65535.0 + 0.7);
}

// Standard 8-bit P6 writer
inline bool write_ppm_p6(const std::string& filename, const ElectromagneticFrame& frame) {
    std::ofstream out(filename, std::ios::binary);
    if (!out) return false;

    out << "P6\n" << frame.width << " " << frame.height << "\n255\n";

    std::vector<unsigned char> row(frame.width * 3);
    for (size_t y = 0; y < frame.height; ++y) {
        const auto* px = &frame.pixels[y * frame.width];
        for (size_t x = 0; x < frame.width; ++x) {
            row[3*x + 0] = to_u8(static_cast<double>(px[x].red));
            row[3*x + 1] = to_u8(static_cast<double>(px[x].green));
            row[3*x + 2] = to_u8(static_cast<double>(px[x].blue));
        }
        out.write(reinterpret_cast<const char*>(row.data()), row.size());
    }
    return true;
}

// Optional: ordered Bayer 8x8 dithering for 8-bit P6 output (reduces banding without changing pipeline)
inline bool write_ppm_p6_dither8(const std::string& filename, const ElectromagneticFrame& frame) {
    static const int bayer8[8][8] = {
        { 0,48,12,60, 3,51,15,63},
        {32,16,44,28,35,19,47,31},
        { 8,56, 4,52,11,59, 7,55},
        {40,24,36,20,43,27,39,23},
        { 2,50,14,62, 1,49,13,61},
        {34,18,46,30,33,17,45,29},
        {10,58, 6,54, 9,57, 5,53},
        {42,26,38,22,41,25,37,21}
    };
    std::ofstream out(filename, std::ios::binary);
    if (!out) return false;

    out << "P6\n" << frame.width << " " << frame.height << "\n255\n";

    std::vector<unsigned char> row(frame.width * 3);
    for (size_t y = 0; y < frame.height; ++y) {
        const auto* px = &frame.pixels[y * frame.width];
        for (size_t x = 0; x < frame.width; ++x) {
            // Dither amplitude ~ 1 LSB
            const double d = (bayer8[y & 7][x & 7] - 32) / 256.0;
            double r = clamp01(static_cast<double>(px[x].red)   + d);
            double g = clamp01(static_cast<double>(px[x].green) + d);
            double b = clamp01(static_cast<double>(px[x].blue)  + d);
            row[3*x + 0] = to_u8(r);
            row[3*x + 1] = to_u8(g);
            row[3*x + 2] = to_u8(b);
        }
        out.write(reinterpret_cast<const char*>(row.data()), row.size());
    }
    return true;
}

// 16-bit P6 writer (maxval 65535, big-endian per Netpbm)
inline bool write_ppm_p6_16(const std::string& filename, const ElectromagneticFrame& frame) {
    std::ofstream out(filename, std::ios::binary);
    if (!out) return false;

    out << "P6\n" << frame.width << " " << frame.height << "\n65535\n";

    std::vector<uint8_t> row(frame.width * 3 * 2); // 2 bytes per channel
    for (size_t y = 0; y < frame.height; ++y) {
        const auto* px = &frame.pixels[y * frame.width];
        for (size_t x = 0; x < frame.width; ++x) {
            const uint16_t r = to_u16(static_cast<double>(px[x].red));
            const uint16_t g = to_u16(static_cast<double>(px[x].green));
            const uint16_t b = to_u16(static_cast<double>(px[x].blue));
            // big-endian
            size_t i = x * 6;
            row[i + 0] = static_cast<uint8_t>((r >> 8) & 0xFF);
            row[i + 1] = static_cast<uint8_t>(r & 0xFF);
            row[i + 2] = static_cast<uint8_t>((g >> 8) & 0xFF);
            row[i + 3] = static_cast<uint8_t>(g & 0xFF);
            row[i + 4] = static_cast<uint8_t>((b >> 8) & 0xFF);
            row[i + 5] = static_cast<uint8_t>(b & 0xFF);
        }
        out.write(reinterpret_cast<const char*>(row.data()), row.size());
    }
    return true;
}

} // namespace cortex