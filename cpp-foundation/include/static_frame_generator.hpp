#pragma once
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <boost/multiprecision/cpp_dec_float.hpp>

namespace cortex {

using CosmicPrecision = boost::multiprecision::number<boost::multiprecision::cpp_dec_float<CORTEX_EM_SPECTRUM_PRECISION>>;

// Electromagnetic spectrum constants with 141-decimal precision
inline const CosmicPrecision VIOLET_MIN_WAVELENGTH("380.0");
inline const CosmicPrecision BLUE_WAVELENGTH("450.0");
inline const CosmicPrecision GREEN_WAVELENGTH("550.0");
inline const CosmicPrecision YELLOW_WAVELENGTH("580.0");
inline const CosmicPrecision RED_WAVELENGTH("650.0");
inline const CosmicPrecision RED_MAX_WAVELENGTH("750.0");

struct CosmicPixel {
    CosmicPrecision red;
    CosmicPrecision green;
    CosmicPrecision blue;
    CosmicPrecision alpha;

    CosmicPixel() : red(0), green(0), blue(0), alpha(1) {}
    CosmicPixel(const CosmicPrecision& r, const CosmicPrecision& g, const CosmicPrecision& b, const CosmicPrecision& a = CosmicPrecision(1))
        : red(r), green(g), blue(b), alpha(a) {}
};

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

class StaticFrameGenerator {
private:
    CosmicPrecision gamma_correction;
    bool high_precision_mode;

    struct PixelAccumulator {
        CosmicPrecision r{0}, g{0}, b{0}, w{0};
        void add(const CosmicPixel& p, const CosmicPrecision& weight = CosmicPrecision(1)) {
            r += p.red   * weight;
            g += p.green * weight;
            b += p.blue  * weight;
            w += weight;
        }
        CosmicPixel to_pixel() const {
            if (w == 0) return CosmicPixel(0,0,0,1);
            return CosmicPixel(r / w, g / w, b / w, 1);
        }
    };

public:
    StaticFrameGenerator() : gamma_correction("2.2"), high_precision_mode(true) {
        std::cout << "RAINBOW Electromagnetic Spectrum Foundation initialized\n";
        std::cout << "   Precision: " << CORTEX_EM_SPECTRUM_PRECISION << " decimals\n";
        std::cout << "   Wavelength range: " << VIOLET_MIN_WAVELENGTH << "nm - " << RED_MAX_WAVELENGTH << "nm\n";
    }

    // Convert wavelength to RGB with 141-decimal precision (kept as-is)
    CosmicPixel wavelength_to_rgb_pixel(const CosmicPrecision& wavelength) {
        using boost::multiprecision::pow;

        CosmicPrecision intensity = wavelength_to_rgb_intensity(wavelength);
        CosmicPrecision red(0), green(0), blue(0);

        if (wavelength >= CosmicPrecision("380") && wavelength < CosmicPrecision("440")) {
            red = -(wavelength - CosmicPrecision("440")) / CosmicPrecision("60");
            blue = CosmicPrecision("1.0");
        } else if (wavelength >= CosmicPrecision("440") && wavelength < CosmicPrecision("490")) {
            green = (wavelength - CosmicPrecision("440")) / CosmicPrecision("50");
            blue = CosmicPrecision("1.0");
        } else if (wavelength >= CosmicPrecision("490") && wavelength < CosmicPrecision("510")) {
            green = CosmicPrecision("1.0");
            blue = -(wavelength - CosmicPrecision("510")) / CosmicPrecision("20");
        } else if (wavelength >= CosmicPrecision("510") && wavelength < CosmicPrecision("580")) {
            red = (wavelength - CosmicPrecision("510")) / CosmicPrecision("70");
            green = CosmicPrecision("1.0");
        } else if (wavelength >= CosmicPrecision("580") && wavelength < CosmicPrecision("645")) {
            red = CosmicPrecision("1.0");
            green = -(wavelength - CosmicPrecision("645")) / CosmicPrecision("65");
        } else if (wavelength >= CosmicPrecision("645") && wavelength <= CosmicPrecision("750")) {
            red = CosmicPrecision("1.0");
        }

        // Apply intensity and gamma correction
        red   = pow(red   * intensity, CosmicPrecision("1") / gamma_correction);
        green = pow(green * intensity, CosmicPrecision("1") / gamma_correction);
        blue  = pow(blue  * intensity, CosmicPrecision("1") / gamma_correction);

        return CosmicPixel(red, green, blue);
    }

    CosmicPrecision wavelength_to_rgb_intensity(const CosmicPrecision& wavelength) {
        CosmicPrecision intensity("1.0");
        if (wavelength >= CosmicPrecision("380") && wavelength < CosmicPrecision("420")) {
            intensity = CosmicPrecision("0.3") + CosmicPrecision("0.7") * (wavelength - CosmicPrecision("380")) / CosmicPrecision("40");
        } else if (wavelength >= CosmicPrecision("701") && wavelength <= CosmicPrecision("750")) {
            intensity = CosmicPrecision("0.3") + CosmicPrecision("0.7") * (CosmicPrecision("750") - wavelength) / CosmicPrecision("49");
        }
        return intensity;
    }

    // Existing simple frame (per-pixel sampling) with progress heartbeat
    ElectromagneticFrame generate_test_frame(size_t width, size_t height) {
        using clock = std::chrono::steady_clock;
        auto t0 = clock::now();

        ElectromagneticFrame frame(width, height);

        if (width == 0 || height == 0) {
            std::cout << "Nothing to render (width or height is zero)\n";
            frame.spectrum_range = RED_MAX_WAVELENGTH - VIOLET_MIN_WAVELENGTH;
            return frame;
        }

        // Print every ~10% of rows (at least once per row for tiny heights)
        const size_t report_interval = std::max<size_t>(1, height / 10);

        // Avoid reallocs during push_back
        frame.pixels.reserve(width * height);

        std::cout << "Rendering " << width << "x" << height << "...\n";

        for (size_t y = 0; y < height; ++y) {
            if ((y % report_interval) == 0) {
                const size_t pct = (100 * y) / height;
                std::cout << "Progress: " << pct << "% (" << y << "/" << height << " rows)\r" << std::flush;
            }

            for (size_t x = 0; x < width; ++x) {
                // Sample at pixel center in NDC
                CosmicPrecision xN = (CosmicPrecision(x) + CosmicPrecision("0.5")) / CosmicPrecision(width);
                CosmicPrecision wavelength = VIOLET_MIN_WAVELENGTH +
                    (RED_MAX_WAVELENGTH - VIOLET_MIN_WAVELENGTH) * xN;

                CosmicPixel pixel = wavelength_to_rgb_pixel(wavelength);
                frame.pixels.push_back(pixel);
                frame.total_energy += pixel.red + pixel.green + pixel.blue;
            }
        }

        // Final heartbeat and timing
        auto t1 = clock::now();
        double secs = std::chrono::duration<double>(t1 - t0).count();
        std::cout << "Progress: 100% (" << height << "/" << height << " rows). Done in "
                  << secs << "s\n";

        frame.spectrum_range = RED_MAX_WAVELENGTH - VIOLET_MIN_WAVELENGTH;
        return frame;
    }

    // Subpixel sample representation (optional public type)
    struct SubpixelSample {
        CosmicPrecision xN;   // normalized [0,1)
        CosmicPrecision yN;   // normalized [0,1)
        CosmicPixel color;
        CosmicPrecision weight{1};
    };

    // Generic converter: subpixel samples -> pixel grid (box filter / weighted average)
    ElectromagneticFrame resample_subpixels_to_pixels(
        const std::vector<SubpixelSample>& samples,
        size_t width, size_t height
    ) {
        std::vector<PixelAccumulator> acc(width * height);

        for (const auto& s : samples) {
            // Map normalized coords to pixel index (clamp to edge)
            double xd = std::min(std::max(0.0, static_cast<double>(s.xN)), 0.999999999);
            double yd = std::min(std::max(0.0, static_cast<double>(s.yN)), 0.999999999);
            size_t ix = static_cast<size_t>(xd * width);
            size_t iy = static_cast<size_t>(yd * height);
            size_t idx = iy * width + ix;

            acc[idx].add(s.color, s.weight);
        }

        ElectromagneticFrame frame(width, height);
        frame.pixels.resize(width * height);

        for (size_t i = 0; i < acc.size(); ++i) {
            frame.pixels[i] = acc[i].to_pixel();
            frame.total_energy += frame.pixels[i].red + frame.pixels[i].green + frame.pixels[i].blue;
        }

        frame.spectrum_range = RED_MAX_WAVELENGTH - VIOLET_MIN_WAVELENGTH;
        return frame;
    }

    // Direct supersampled rendering without storing all subpixel samples
    ElectromagneticFrame generate_supersampled_frame(
        size_t width, size_t height,
        size_t spp_x, size_t spp_y,
        bool jitter = false
    ) {
        std::vector<PixelAccumulator> acc(width * height);

        // Very light PRNG for jitter if requested
        auto rand01 = []() -> double {
            // xorshift64* minimalistic RNG per call (not cryptographic)
            static uint64_t s = 0x9E3779B97F4A7C15ull;
            s ^= s >> 12; s ^= s << 25; s ^= s >> 27;
            uint64_t z = s * 0x2545F4914F6CDD1Dull;
            return (z >> 11) * (1.0 / 9007199254740992.0); // ~[0,1)
        };

        for (size_t y = 0; y < height; ++y) {
            for (size_t x = 0; x < width; ++x) {

                for (size_t sy = 0; sy < spp_y; ++sy) {
                    for (size_t sx = 0; sx < spp_x; ++sx) {
                        double jx = jitter ? rand01() : 0.5;
                        double jy = jitter ? rand01() : 0.5;

                        // Subpixel in [0,1) of the pixel
                        double fx = (static_cast<double>(sx) + jx) / static_cast<double>(spp_x);
                        double fy = (static_cast<double>(sy) + jy) / static_cast<double>(spp_y);

                        // Sample position in NDC at pixel center of subcell
                        CosmicPrecision xN = (CosmicPrecision(x) + CosmicPrecision(fx)) / CosmicPrecision(width);
                        CosmicPrecision yN = (CosmicPrecision(y) + CosmicPrecision(fy)) / CosmicPrecision(height);

                        // For spectrum, wavelength varies only with x
                        (void)yN; // reserved if you later vary with y as well
                        CosmicPrecision wavelength = VIOLET_MIN_WAVELENGTH +
                            (RED_MAX_WAVELENGTH - VIOLET_MIN_WAVELENGTH) * xN;

                        CosmicPixel sp = wavelength_to_rgb_pixel(wavelength);

                        size_t idx = y * width + x;
                        acc[idx].add(sp, CosmicPrecision("1"));
                    }
                }
            }
        }

        // Finalize to frame (mean over spp)
        ElectromagneticFrame frame(width, height);
        frame.pixels.resize(width * height);

        CosmicPrecision samples_per_pixel = CosmicPrecision(spp_x * spp_y);
        for (size_t i = 0; i < acc.size(); ++i) {
            // Box filter average
            CosmicPixel p = acc[i].to_pixel();
            // Since we added unit weights spp_x*spp_y times, to_pixel already divides by w
            frame.pixels[i] = p;
            frame.total_energy += p.red + p.green + p.blue;
        }

        frame.spectrum_range = RED_MAX_WAVELENGTH - VIOLET_MIN_WAVELENGTH;
        return frame;
    }
};

} // namespace cortex