#pragma once
#include <vector>
#include <cstddef>
#include "static_frame_generator.hpp"

namespace cortex {

    // Reusable buffers to avoid per-run reallocations.
    // Call reserve(width*height) once; reuse across frames.
    class FrameArena {
    public:
        void reserve(size_t width, size_t height) {
            const size_t px = width * height;
            rgb_float.resize(px * 3);     // host scratch for CUDA/CPU fast path
            pixels.resize(px);            // final frame pixels
            w = width; h = height;
        }

        // Build an ElectromagneticFrame from the current pixel buffer (no extra allocs)
        ElectromagneticFrame build_frame_from_pixels() const {
            ElectromagneticFrame f(w, h);
            f.pixels = pixels; // copy (same size)
            for (const auto& p : f.pixels) {
                f.total_energy += p.red + p.green + p.blue;
            }
            f.spectrum_range = RED_MAX_WAVELENGTH - VIOLET_MIN_WAVELENGTH;
            return f;
        }

        // Convert rgb_float (0..1) into CosmicPixel array in-place
        void copy_from_rgb_float() {
            const size_t px = w * h;
            for (size_t i = 0; i < px; ++i) {
                double r = rgb_float[3*i + 0];
                double g = rgb_float[3*i + 1];
                double b = rgb_float[3*i + 2];
                pixels[i] = CosmicPixel(CosmicPrecision(r), CosmicPrecision(g), CosmicPrecision(b), CosmicPrecision(1));
            }
        }

        std::vector<float>& scratch_rgb() { return rgb_float; }
        std::vector<CosmicPixel>& pixel_buffer() { return pixels; }
        size_t width() const { return w; }
        size_t height() const { return h; }

    private:
        size_t w{0}, h{0};
        std::vector<float> rgb_float;
        std::vector<CosmicPixel> pixels;
    };

} // namespace cortex