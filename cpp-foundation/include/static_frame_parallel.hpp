#pragma once
#include <thread>
#include <vector>
#include <iostream>
#include "static_frame_generator.hpp"
#include "quad_array_manager.hpp"

// Lightweight index range to avoid allocating a large wavelengths array.
struct IndexRange {
    size_t N;
    size_t size() const noexcept { return N; }
    size_t operator[](size_t i) const noexcept { return i; }
};

// Parallel wrapper mirroring generate_test_frame with progress callbacks.
inline cortex::ElectromagneticFrame generate_test_frame_parallel(
    cortex::StaticFrameGenerator& gen,
    size_t width, size_t height,
    unsigned threads = std::max(1u, std::thread::hardware_concurrency()),
    size_t min_items_for_parallel = 50'000
) {
    using namespace cortex;
    ElectromagneticFrame frame(width, height);
    if (width == 0 || height == 0) {
        frame.spectrum_range = RED_MAX_WAVELENGTH - VIOLET_MIN_WAVELENGTH;
        return frame;
    }

    const size_t N = width * height;

    QuadArrayManager qam;
    QuadArrayManager::Options opts;
    opts.threads = std::max(1u, threads);
    opts.min_items_for_parallel = min_items_for_parallel;
    opts.on_progress = [&](size_t done, size_t total) {
        const size_t pct = (100 * done) / (total ? total : 1);
        std::cout << "Progress: " << pct << "% (" << done << "/" << total << ")\r" << std::flush;
    };

    auto map_fn = [&](size_t idx) {
        const size_t y = idx / width;
        const size_t x = idx - y * width;
        (void)y; // wavelength only varies with x; keep y for symmetry and future use

        CosmicPrecision xN = (CosmicPrecision(x) + CosmicPrecision("0.5")) / CosmicPrecision(width);
        CosmicPrecision wl = VIOLET_MIN_WAVELENGTH +
                             (RED_MAX_WAVELENGTH - VIOLET_MIN_WAVELENGTH) * xN;
        return gen.wavelength_to_rgb_pixel(wl);
    };

    auto pixels = qam.parallel_map(IndexRange{N}, map_fn, opts);

    frame.pixels = std::move(pixels);
    for (const auto& p : frame.pixels) frame.total_energy += p.red + p.green + p.blue;
    frame.spectrum_range = RED_MAX_WAVELENGTH - VIOLET_MIN_WAVELENGTH;
    std::cout << "\n";
    return frame;
}