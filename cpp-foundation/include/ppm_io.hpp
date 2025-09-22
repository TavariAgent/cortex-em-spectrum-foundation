#pragma once
#include <vector>
#include <fstream>
#include <string>
#include <algorithm>
#include "static_frame_generator.hpp"

namespace cortex {

    // Clamp to [0,1] then convert to 0..255
    inline unsigned char to_u8(double v) {
        if (v <= 0.0) return 0;
        if (v >= 1.0) return 255;
        return static_cast<unsigned char>(v * 255.0 + 0.5);
    }

    // Write ElectromagneticFrame as binary PPM (P6)
    inline bool write_ppm_p6(const std::string& filename, const ElectromagneticFrame& frame) {
        std::ofstream out(filename, std::ios::binary);
        if (!out) return false;

        // Header
        out << "P6\n" << frame.width << " " << frame.height << "\n255\n";

        // Stream row-by-row to avoid big temporaries
        std::vector<unsigned char> row(frame.width * 3);
        for (size_t y = 0; y < frame.height; ++y) {
            for (size_t x = 0; x < frame.width; ++x) {
                const auto& p = frame.pixels[y * frame.width + x];
                row[3*x + 0] = to_u8(static_cast<double>(p.red));
                row[3*x + 1] = to_u8(static_cast<double>(p.green));
                row[3*x + 2] = to_u8(static_cast<double>(p.blue));
            }
            out.write(reinterpret_cast<const char*>(row.data()), row.size());
        }
        return true;
    }

} // namespace cortex