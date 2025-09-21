#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cstdint>
#include "static_frame_generator.hpp"

namespace image_io {

enum class PpmFormat { P3, P6 };

inline uint8_t clamp255(double v) {
    if (v < 0.0) v = 0.0;
    if (v > 255.0) v = 255.0;
    return static_cast<uint8_t>(v + 0.5);
}

inline void write_ppm(const cortex::ElectromagneticFrame& frame,
                      const std::string& filename,
                      PpmFormat fmt = PpmFormat::P6,
                      const std::vector<std::string>& comments = {}) {
    if (fmt == PpmFormat::P3) {
        std::ofstream f(filename);
        if (!f) throw std::runtime_error("Cannot open " + filename);
        f << "P3\n";
        for (const auto& c : comments) f << "# " << c << "\n";
        f << frame.width << " " << frame.height << "\n255\n";
        for (const auto& p : frame.pixels) {
            const int r = clamp255(255.0 * static_cast<double>(p.red));
            const int g = clamp255(255.0 * static_cast<double>(p.green));
            const int b = clamp255(255.0 * static_cast<double>(p.blue));
            f << r << ' ' << g << ' ' << b << '\n';
        }
        return;
    }
    // P6
    std::ofstream f(filename, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open " + filename);
    f << "P6\n";
    for (const auto& c : comments) f << "# " << c << "\n";
    f << frame.width << " " << frame.height << "\n255\n";
    std::string row;
    row.resize(frame.width * 3);
    for (size_t i = 0; i < frame.height; ++i) {
        for (size_t x = 0; x < frame.width; ++x) {
            const auto& p = frame.pixels[i * frame.width + x];
            row[3*x + 0] = static_cast<char>(clamp255(255.0 * static_cast<double>(p.red)));
            row[3*x + 1] = static_cast<char>(clamp255(255.0 * static_cast<double>(p.green)));
            row[3*x + 2] = static_cast<char>(clamp255(255.0 * static_cast<double>(p.blue)));
        }
        f.write(row.data(), row.size());
    }
}

} // namespace image_io