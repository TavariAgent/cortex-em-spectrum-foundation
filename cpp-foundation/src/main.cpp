#include <algorithm>
#include <iostream>
#include <numeric>
#include <chrono>
#include <cmath>
#include <cstring>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include "static_frame_generator.hpp"
#include "static_frame_parallel.hpp"
#include "or_switch.hpp"
#include "quad_array_manager.hpp"
#include "ppm_io.hpp"
#include "calibration_10frame.hpp"
#include "calibration_io.hpp"
#include "image_corrections.hpp"

static bool eq_flag(const char* a, const char* b) { return std::strcmp(a,b)==0; }

int main(int argc, char** argv) {
    using namespace cortex;
    const auto start_time = std::chrono::system_clock::now();

    // Defaults
    size_t width = 400, height = 300;
    int depth = 16;            // 8 or 16
    bool dither8 = false;
    bool skip_calib = false;
    std::string load_calib_path;
    std::string save_calib_path = "calibration.json";

    // Parse simple CLI: cortex_em_cli [W H] [--depth 8|16] [--dither] [--skip-calib] [--load-calib file] [--save-calib file]
    int i = 1;
    if (argc >= 3 && argv[1][0] != '-' && argv[2][0] != '-') {
        width  = static_cast<size_t>(std::strtoull(argv[1], nullptr, 10));
        height = static_cast<size_t>(std::strtoull(argv[2], nullptr, 10));
        i = 3;
    }
    for (; i < argc; ++i) {
        if (eq_flag(argv[i], "--depth") && i+1 < argc) { depth = std::atoi(argv[++i]); }
        else if (eq_flag(argv[i], "--dither")) { dither8 = true; }
        else if (eq_flag(argv[i], "--skip-calib")) { skip_calib = true; }
        else if (eq_flag(argv[i], "--load-calib") && i+1 < argc) { load_calib_path = argv[++i]; }
        else if (eq_flag(argv[i], "--save-calib") && i+1 < argc) { save_calib_path = argv[++i]; }
    }

    // 0) Acquire calibration (load or capture)
    CalibrationParams calib_params;
    bool have_calib = false;

    if (!load_calib_path.empty()) {
        if (auto p = load_calibration_json(load_calib_path)) {
            calib_params = *p;
            have_calib = true;
            std::cout << "Loaded calibration from " << load_calib_path << "\n";
        } else {
            std::cout << "Failed to load calibration from " << load_calib_path << ", will capture.\n";
        }
    }

    if (!have_calib && !skip_calib) {
        CalibrationResult calib_result;
        calib_result = calibrate_primary_monitor_10(width, height, 10, 80, true);
        if (calib_result.frames_used > 0) {
            calib_params = calib_result.params;
            have_calib = true;
            if (!save_calib_path.empty()) save_calibration_json(save_calib_path, calib_params);
        }
    }

    // 1) Generator + parallel driver (unchanged semantics)
    StaticFrameGenerator gen;

    StaticFrameParallel::Config cfg;
    cfg.tile_w = (width >= 1280) ? 64 : 32;   // ultrawide-friendly tiles
    cfg.tile_h = (height >= 720) ? 32 : 32;
    cfg.spp_x = 2;
    cfg.spp_y = 2;
    cfg.jitter = false;
    cfg.max_accum_weight = 4.0;
    cfg.router.k_percent = 5.0;
    cfg.router.epsilon = 1e-30;
    cfg.router.calibration_frames_required = 5;
    cfg.router.calibration_min_seconds = 2.0;

    StaticFrameParallel pipeline(cfg);
    pipeline.set_resolution(width, height);

    // 2) Render two frames so tile change mask stabilizes (first frame primes prev_amp)
    auto [frame1, mask1, cal1] = pipeline.render_next_frame(gen);
    auto [frame,  mask2, cal2] = pipeline.render_next_frame(gen);

    // 3) Save raw (for A/B)
    write_ppm_p6("electromagnetic_spectrum_parallel.ppm", frame);

    // 4) Apply calibration (WB + auto-exposure + gamma), then save
    if (have_calib) {
        double exposure = compute_auto_exposure(frame, calib_params, 0.18);
        ElectromagneticFrame corrected = frame;
        apply_corrections_inplace(corrected, calib_params, exposure);

        if (depth == 16) {
            write_ppm_p6_16("electromagnetic_spectrum_parallel_corrected16.ppm", corrected);
        } else {
            if (dither8) write_ppm_p6_dither8("electromagnetic_spectrum_parallel_corrected.ppm", corrected);
            else         write_ppm_p6("electromagnetic_spectrum_parallel_corrected.ppm", corrected);
        }
    }

    // Timing
    const auto end_time = std::chrono::system_clock::now();
    const std::chrono::duration<double> elapsed_seconds = end_time - start_time;
    std::cout << "Elapsed time: " << elapsed_seconds.count() << " seconds\n";

    // Tile stats from the 2nd frame
    const size_t dynamic_tiles = std::accumulate(mask2.begin(), mask2.end(), size_t(0));
    const size_t total_tiles = mask2.size();
    std::cout << "Rendered " << width << "x" << height
              << " with " << total_tiles << " tiles ("
              << dynamic_tiles << " marked dynamic > K=5%)\n";
    std::cout << "Calibration complete: " << (cal2 ? "yes" : "no") << "\n";

    // Report calibration used
    if (have_calib) {
        std::cout << "Calibration in use: WB "
                  << calib_params.gain_r << "/"
                  << calib_params.gain_g << "/"
                  << calib_params.gain_b << ", gamma " << calib_params.gamma
                  << ", avg luma " << calib_params.avg_luma << "\n";
    }

    return 0;
}