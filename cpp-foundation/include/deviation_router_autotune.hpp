#pragma once
#include <string>
#include <sstream>
#include <vector>
#include <iostream>
#include <tuple>
#include "micro_param_tuner.hpp"
#include "deviation_router.hpp"
#include "frame_cache.hpp"
#include "static_frame_generator.hpp"
#include "image_ops.hpp"

namespace cortex {

// Synthetic trial to evaluate DeviationRouter parameter sets.
// Metrics we collect (example):
//  [0] tiles_per_second
//  [1] false_positive_ratio (simulated)
//  [2] avg_spp_accum (lower may mean more churn)
// Score example:  tiles_per_second - 40*false_positive_ratio - 0.5*avg_spp_accum
//
// In real usage, replace the synthetic diff generator with actual captured
// consecutive frames or a recorded short clip.
inline std::vector<double> deviation_router_trial(const std::string& label) {
    // Parse "tw=64,th=32,thr=0.05"
    size_t tw=64, th=32;
    double thr=0.05;
    {
        std::istringstream iss(label);
        std::string kv;
        while (std::getline(iss, kv, ',')) {
            auto eq = kv.find('=');
            if (eq==std::string::npos) continue;
            std::string k = kv.substr(0,eq);
            std::string v = kv.substr(eq+1);
            if (k=="tw") tw = static_cast<size_t>(std::stoul(v));
            else if (k=="th") th = static_cast<size_t>(std::stoul(v));
            else if (k=="thr") thr = std::stod(v);
        }
    }

    DeviationRouter router({tw, th, thr, 4});
    FrameCache cache;

    // Fake frames: generate two frames, second has controlled sparse changes.
    const size_t W=512, H=288;
    ElectromagneticFrame prev(W,H);
    ElectromagneticFrame curr(W,H);

    // Fill with deterministic pattern
    for (size_t i=0;i<W*H;++i) {
        double base = (i % 97) * (1.0/97.0);
        prev.pixels.push_back(CosmicPixel(base, base*0.7, base*0.4, 1.0));
        curr.pixels.push_back(prev.pixels.back());
    }

    // Inject synthetic deviations in 1% of pixels
    size_t injected = 0;
    for (size_t y=0; y<H; y+=32) {
        for (size_t x=0; x<W; x+=64) {
            size_t idx = y*W + x;
            if (idx < curr.pixels.size()) {
                curr.pixels[idx].red = curr.pixels[idx].red + 0.1; // 10% change
                ++injected;
            }
        }
    }

    // Time N iterations to emulate streaming
    const int iterations = 30;
    auto t0 = std::chrono::high_resolution_clock::now();
    for (int i=0;i<iterations;++i) {
        router.analyze_and_route(curr, &prev, cache);
    }
    auto t1 = std::chrono::high_resolution_clock::now();

    // Assume: W, H, tw, th, iterations are size_t, iterations > 0.
    const size_t tiles_x = (W + tw - 1) / tw;
    const size_t tiles_y = (H + th - 1) / th;
    const size_t total_tiles = tiles_x * tiles_y;

    const auto secs = std::chrono::duration<double>(t1 - t0).count();

    // Throughput (tiles * iterations) per second as double
    const double work_units = static_cast<double>(total_tiles) * static_cast<double>(iterations);
    const double tiles_per_second = (secs > 0.0) ? (work_units / secs) : work_units;

    // Simulate ROI pops to approximate false positives / churn
    int roi_popped=0;
    std::shared_ptr<SubpixelChunk> chunk;
    while (cache.roi_chunks.pop(chunk)) {
        ++roi_popped;
    }

    // Heuristic (placeholder): approximate false positive ratio
    // For a real system you would compare against ground-truth diff mask.
    double expected_real_changes = injected / static_cast<double>(tw*th+1);
    double fp_ratio = (roi_popped>0)
        ? std::max(0.0, (roi_popped - expected_real_changes) / (roi_popped+1.0))
        : 0.0;

    double avg_spp = 2.0; // Placeholder (no direct access to internal states here)

    return {tiles_per_second, fp_ratio, avg_spp};
}

// Run a sweep and return best label
inline std::string autotune_deviation_router() {
    std::vector<std::string> configs;
    const std::vector<size_t> tile_ws = {32, 48, 64};
    const std::vector<size_t> tile_hs = {16, 32};
    const std::vector<double> thrs = {0.03, 0.05, 0.08};

    for (auto w: tile_ws)
        for (auto h: tile_hs)
            for (auto t: thrs) {
                std::ostringstream oss;
                oss << "tw="<<w<<",th="<<h<<",thr="<<t;
                configs.push_back(oss.str());
            }

    auto results = MicroParamTuner::run(
        configs,
        deviation_router_trial,
        [](const std::string& label, const std::vector<double>& m){
            // m[0]=tiles/sec (higher better), m[1]=fp ratio (lower), m[2]=avg_spp (lower)
            double score = m[0] - 40.0*m[1] - 0.5*m[2];
            // gentle preference for smaller tiles (more granularity) if score close
            if (label.find("tw=32") != std::string::npos) score += 2.0;
            return score;
        },
        /*parallel*/ true
    );

    if (!results.empty()) {
        std::cout << "[TUNER] Best: " << results.front().label
                  << " score=" << results.front().score
                  << " tiles/sec=" << results.front().metrics[0]
                  << " fp_ratio=" << results.front().metrics[1] << "\n";
        return results.front().label;
    }
    return {};
}

} // namespace cortex