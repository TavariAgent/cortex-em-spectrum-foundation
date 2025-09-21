#pragma once

// Electromagnetic spectrum foundation configuration
#ifndef CORTEX_EM_SPECTRUM_PRECISION
#define CORTEX_EM_SPECTRUM_PRECISION 141
#endif

#ifndef CORTEX_ELECTROMAGNETIC_FOUNDATION
#define CORTEX_ELECTROMAGNETIC_FOUNDATION
#endif

// Standard C++ includes
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <memory>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <sstream>

// Windows-specific optimizations
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
#endif

// Math constants
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#include <math.h>
#endif

// Complete Boost multiprecision includes
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/number.hpp>
#include <boost/math/special_functions/pow.hpp>
#include <boost/multiprecision/detail/default_ops.hpp>

namespace cortex {
namespace frames {

// 141-decimal precision electromagnetic spectrum foundation
using CosmicPrecision = boost::multiprecision::number<boost::multiprecision::cpp_dec_float<CORTEX_EM_SPECTRUM_PRECISION>>;

// Helper function for safe CosmicPrecision to double conversion
inline double cosmic_to_double(const CosmicPrecision& value) {
    return static_cast<double>(value);
}

// Helper function for safe CosmicPrecision to int conversion
inline int cosmic_to_int(const CosmicPrecision& value) {
    return static_cast<int>(static_cast<double>(value));
}

// Helper function for safe CosmicPrecision stream output
inline std::string cosmic_to_string(const CosmicPrecision& value, int precision = 6) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision);
    oss << value;
    return oss.str();
}

// Electromagnetic spectrum constants with 141-decimal precision
const CosmicPrecision VIOLET_MIN_WAVELENGTH("380.0");
const CosmicPrecision BLUE_WAVELENGTH("450.0");
const CosmicPrecision GREEN_WAVELENGTH("550.0");
const CosmicPrecision YELLOW_WAVELENGTH("580.0");
const CosmicPrecision RED_WAVELENGTH("650.0");
const CosmicPrecision RED_MAX_WAVELENGTH("750.0");

// Perfect electromagnetic spectrum pixel representation
struct CosmicPixel {
    CosmicPrecision red;
    CosmicPrecision green;
    CosmicPrecision blue;
    CosmicPrecision alpha;

    CosmicPixel() : red(0), green(0), blue(0), alpha(1) {}
    CosmicPixel(const CosmicPrecision& r, const CosmicPrecision& g, const CosmicPrecision& b, const CosmicPrecision& a = CosmicPrecision(1))
        : red(r), green(g), blue(b), alpha(a) {}
};

// Electromagnetic spectrum frame with 141-decimal precision
struct ElectromagneticFrame {
    std::vector<CosmicPixel> pixels;
    size_t width;
    size_t height;
    CosmicPrecision spectrum_range;
    CosmicPrecision total_energy;
    std::chrono::high_resolution_clock::time_point creation_time;

    ElectromagneticFrame(size_t w, size_t h)
        : width(w), height(h), spectrum_range(0), total_energy(0) {
        pixels.reserve(w * h);
        creation_time = std::chrono::high_resolution_clock::now();
    }
};

// Your electromagnetic spectrum foundation class
class StaticFrameGenerator {
private:
    CosmicPrecision gamma_correction;
    bool high_precision_mode;
    CosmicPrecision intensity_scale = CosmicPrecision("1.0");

public:
    void set_intensity_scale(const CosmicPrecision& s) { intensity_scale = s; }
    StaticFrameGenerator() : gamma_correction("2.2"), high_precision_mode(true) {
        std::cout << "RAINBOW Electromagnetic Spectrum Foundation initialized\n";
        std::cout << "   Precision: " << CORTEX_EM_SPECTRUM_PRECISION << " decimals\n";
        std::cout << "   Wavelength range: " << VIOLET_MIN_WAVELENGTH << "nm - " << RED_MAX_WAVELENGTH << "nm\n";
    }

    // Convert wavelength to RGB with 141-decimal precision
    CosmicPixel wavelength_to_rgb_pixel(const CosmicPrecision& wavelength) {
        CosmicPrecision intensity = wavelength_to_rgb_intensity(wavelength);

        CosmicPrecision red(0), green(0), blue(0);

        if (wavelength >= CosmicPrecision("380") && wavelength < CosmicPrecision("440")) {
            // Violet to blue transition
            red = -(wavelength - CosmicPrecision("440")) / CosmicPrecision("60");
            blue = CosmicPrecision("1.0");
        } else if (wavelength >= CosmicPrecision("440") && wavelength < CosmicPrecision("490")) {
            // Blue to cyan transition
            green = (wavelength - CosmicPrecision("440")) / CosmicPrecision("50");
            blue = CosmicPrecision("1.0");
        } else if (wavelength >= CosmicPrecision("490") && wavelength < CosmicPrecision("510")) {
            // Cyan to green transition
            green = CosmicPrecision("1.0");
            blue = -(wavelength - CosmicPrecision("510")) / CosmicPrecision("20");
        } else if (wavelength >= CosmicPrecision("510") && wavelength < CosmicPrecision("580")) {
            // Green to yellow transition
            red = (wavelength - CosmicPrecision("510")) / CosmicPrecision("70");
            green = CosmicPrecision("1.0");
        } else if (wavelength >= CosmicPrecision("580") && wavelength < CosmicPrecision("645")) {
            // Yellow to red transition
            red = CosmicPrecision("1.0");
            green = -(wavelength - CosmicPrecision("645")) / CosmicPrecision("65");
        } else if (wavelength >= CosmicPrecision("645") && wavelength <= CosmicPrecision("750")) {
            // Pure red
            red = CosmicPrecision("1.0");
        }

        // Apply intensity and gamma correction with 141-decimal precision
        red = boost::multiprecision::pow(red * intensity, CosmicPrecision("1") / gamma_correction);
        green = boost::multiprecision::pow(green * intensity, CosmicPrecision("1") / gamma_correction);
        blue = boost::multiprecision::pow(blue * intensity, CosmicPrecision("1") / gamma_correction);

        return CosmicPixel(red, green, blue);
    }

    // Calculate wavelength intensity with 141-decimal precision
    CosmicPrecision wavelength_to_rgb_intensity(const CosmicPrecision& wavelength) {
        CosmicPrecision intensity("1.0");

        if (wavelength >= CosmicPrecision("380") && wavelength < CosmicPrecision("420")) {
            intensity = CosmicPrecision("0.3") + CosmicPrecision("0.7") * (wavelength - CosmicPrecision("380")) / CosmicPrecision("40");
        } else if (wavelength >= CosmicPrecision("701") && wavelength <= CosmicPrecision("750")) {
            intensity = CosmicPrecision("0.3") + CosmicPrecision("0.7") * (CosmicPrecision("750") - wavelength) / CosmicPrecision("49");
        }

        return intensity;
    }

    // Generate electromagnetic spectrum frame with 141-decimal precision
    ElectromagneticFrame generate_test_frame(size_t width, size_t height) {
        ElectromagneticFrame frame(width, height);

        for (size_t y = 0; y < height; ++y) {
            for (size_t x = 0; x < width; ++x) {
                // Map x coordinate to wavelength across electromagnetic spectrum
                CosmicPrecision wavelength = VIOLET_MIN_WAVELENGTH +
                    (RED_MAX_WAVELENGTH - VIOLET_MIN_WAVELENGTH) * CosmicPrecision(x) / CosmicPrecision(width - 1);

                CosmicPixel pixel = wavelength_to_rgb_pixel(wavelength);
                frame.pixels.push_back(pixel);

                // Accumulate total energy
                frame.total_energy += pixel.red + pixel.green + pixel.blue;
            }
        }

        frame.spectrum_range = RED_MAX_WAVELENGTH - VIOLET_MIN_WAVELENGTH;

        return frame;
    }

    // Generate electromagnetic spectrum visualization
    ElectromagneticFrame generate_em_spectrum_frame(size_t width, size_t height) {
        return generate_test_frame(width, height);
    }

    // Save electromagnetic spectrum frame data
    void save_frame_data(const ElectromagneticFrame& frame, const std::string& filename) {
        std::ofstream file(filename);
        if (file.is_open()) {
            file << "# Electromagnetic Spectrum Frame Data - 141-decimal precision\n";
            file << "# TavariAgent/cortex-em-spectrum-foundation\n";
            file << "# Width: " << frame.width << ", Height: " << frame.height << "\n";
            file << "# Spectrum Range: " << frame.spectrum_range << "nm\n";
            file << "# Total Energy: " << frame.total_energy << "\n";
            file << "# Pixels: " << frame.pixels.size() << "\n";

            for (size_t i = 0; i < frame.pixels.size(); ++i) {
                const auto& pixel = frame.pixels[i];
                file << pixel.red << " " << pixel.green << " " << pixel.blue << " " << pixel.alpha << "\n";
            }

            file.close();
            std::cout << "DISK Electromagnetic spectrum frame saved: " << filename << "\n";
        }
    }
};

} // namespace frames
} // namespace cortex