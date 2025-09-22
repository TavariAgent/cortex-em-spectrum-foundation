#pragma once
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <thread>
#include <cmath>
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
            CosmicPrecision inv_gamma = CosmicPrecision("1") / gamma_correction;
            using boost::multiprecision::pow;
            red   = pow(red   * intensity, inv_gamma);
            green = pow(green * intensity, inv_gamma);
            blue  = pow(blue  * intensity, inv_gamma);

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

        ElectromagneticFrame generate_test_frame(size_t width, size_t height) {
            using clock = std::chrono::steady_clock;
            auto t0 = clock::now();

            ElectromagneticFrame frame(width, height);

            if (width == 0 || height == 0) {
                std::cout << "Nothing to render (width or height is zero)\n";
                frame.spectrum_range = RED_MAX_WAVELENGTH - VIOLET_MIN_WAVELENGTH;
                return frame;
            }

            const size_t report_interval = std::max<size_t>(1, height / 10);
            frame.pixels.reserve(width * height);

            std::cout << "Rendering " << width << "x" << height << "...\n";

            for (size_t y = 0; y < height; ++y) {
                if ((y % report_interval) == 0) {
                    const size_t pct = (100 * y) / height;
                    std::cout << "Progress: " << pct << "% (" << y << "/" << height << " rows)\r" << std::flush;
                }

                for (size_t x = 0; x < width; ++x) {
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
            // Local accumulator for weighted average in CosmicPrecision
            struct PixelAccumulator {
                CosmicPrecision r{0}, g{0}, b{0}, w{0};
                void add(const CosmicPixel& p, const CosmicPrecision& weight) {
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

            ElectromagneticFrame frame(width, height);
            if (width == 0 || height == 0) {
                frame.spectrum_range = RED_MAX_WAVELENGTH - VIOLET_MIN_WAVELENGTH;
                return frame;
            }

            std::vector<PixelAccumulator> acc(width * height);

            // Clamp normalized coords safely below 1.0 to avoid ix==width
            const double one_minus = std::nextafter(1.0, 0.0);

            for (const auto& s : samples) {
                double xd = std::clamp(static_cast<double>(s.xN), 0.0, one_minus);
                double yd = std::clamp(static_cast<double>(s.yN), 0.0, one_minus);

                size_t ix = static_cast<size_t>(xd * static_cast<double>(width));
                size_t iy = static_cast<size_t>(yd * static_cast<double>(height));

                // Double-safety (in case of edge numeric quirks)
                if (ix >= width)  ix = width  - 1;
                if (iy >= height) iy = height - 1;

                acc[iy * width + ix].add(s.color, s.weight);
            }
        }
};

} // namespace cortex