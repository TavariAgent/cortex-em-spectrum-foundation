#include <algorithm>
#include <iostream>
#include <numeric>
#include <chrono>
#include <cmath>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include "static_frame_generator.hpp"
#include "static_frame_parallel.hpp"   // ensure this header exists locally
#include "or_switch.hpp"               // ensure this header exists locally
#include "quad_array_manager.hpp"      // ensure this header exists locally
#include "ppm_io.hpp"

int main() {

    const auto start_time = std::chrono::system_clock::now();
    using namespace cortex;


    // Generator + parallel driver
    StaticFrameGenerator gen;

    StaticFrameParallel::Config cfg;
    cfg.tile_w = 32;
    cfg.tile_h = 32;
    cfg.spp_x = 2;
    cfg.spp_y = 2;
    cfg.jitter = false;
    cfg.max_accum_weight = 4.0;
    cfg.router.k_percent = 5.0;          // K = 5%
    cfg.router.epsilon = 1e-30;          // threshold for amplitude diff
    cfg.router.calibration_frames_required = 5;  // need >=5 frames
    cfg.router.calibration_min_seconds = 2.0;    // and >=2s elapsed

    StaticFrameParallel pipeline(cfg);

    constexpr size_t width = 400, height = 300;
    pipeline.set_resolution(width, height);

    // One parallel frame (calibration likely not complete on first frame)
    auto [frame, tile_dirty_mask, calibration_complete] = pipeline.render_next_frame(gen);

    // Write outputs as binary PPM (P6)
    if (!write_ppm_p6("electromagnetic_spectrum_parallel.ppm", frame)) return 1;
    if (!write_ppm_p6("electromagnetic_spectrum_supersampled.ppm", frame)) return 1;

    // 3. Record the end time
    const auto end_time = std::chrono::system_clock::now();

    // 4. Calculate the duration
    const std::chrono::duration<double> elapsed_seconds = end_time - start_time;

    // 5. Output the elapsed time
    std::cout << "Elapsed time: " << elapsed_seconds.count() << " seconds" << std::endl;

    // Report tile mask stats
    const size_t dynamic_tiles = std::accumulate(tile_dirty_mask.begin(), tile_dirty_mask.end(), size_t(0));
    const size_t total_tiles = tile_dirty_mask.size();
    std::cout << "Rendered " << width << "x" << height
              << " with " << total_tiles << " tiles ("
              << dynamic_tiles << " marked dynamic > K=5%)\n";
    std::cout << "Calibration complete: " << (calibration_complete ? "yes" : "no") << "\n";

    return 0;
}