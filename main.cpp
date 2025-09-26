// Unified capture-focused main.cpp
// Consolidated “clean” entrypoint using the updated components:
// - Static scene gate (optional)
// - High-frequency screen capture with dedupe (OperandMap + byte compare)
// - LLMFramePool coalescing identical runs
// - Optional packed frame store (future – currently stub switch)
// - Optional deviation router auto-tune (pre-capture)
// - Lightweight filters + correction queue
// - Metrics JSONL logging (--metrics path)
// - Optional ContextOverflowGuard wrapping (--guard)
// - Activity tracker (quiet/static/wake state) with adaptive dedupe gating
//
// NOTE:
// 1. This intentionally removes the legacy spectrum render / calibration code.
// 2. Old theatrical logging and unused math benchmarking logic are omitted.
// 3. Keep previous main.cpp around temporarily under a different name if you still need it.
// 4. Future rename: ContextOverflowGuard -> RuntimeAdaptiveGuard (pending).
//
// Build expectation: Windows only for capture path (screen_capture_win.hpp)
// Non-Windows: prints a message and exits gracefully.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

#include "screen_capture_win.hpp"
#include "image_ops.hpp"
#include "operand_map.hpp"
#include "llm_frame_pool.hpp"
#include "live_viewer_win.hpp"
#include "frame_recorder.hpp"
#include "filters.hpp"
#include "correction_queue.hpp"
#include "static_scene_detector.hpp"
#include "scene_activity_tracker.hpp"
#include "metrics_logger.hpp"
#include "process_memory.hpp"
#include "deviation_router_autotune.hpp"
#include "runtime_adaptive_guard.hpp"

namespace fs = std::filesystem;
namespace Sig = cortex::sig;
using namespace cortex;

static bool eq_flag(const char* a, const char* b) { return std::strcmp(a,b)==0; }

static void print_help() {
    std::cout <<
      "Cortex Capture (Unified Main)\n"
      "Flags:\n"
      "  --capture N              Enter capture mode (display index, usually 1)\n"
      "  --live                   Show live viewer window\n"
      "  --fps F                  Target FPS (default 30)\n"
      "  --seconds S              Duration (<=0 => single frame)\n"
      "  --resize WxH             Resize each frame (e.g. 1280x720)\n"
      "  --record base            Write non-duplicate BMP frames (base_000000.bmp...)\n"
      "  --metrics file.jsonl     Write per-frame + aggregate metrics JSONL\n"
      "  --no-static-gate         Skip static scene preflight wait\n"
      "  --static-sec X           Required stable seconds (default 1.0)\n"
      "  --static-timeout T       Static wait timeout seconds (default 10.0)\n"
      "  --static-tolerant        Tolerant equality in static gate (signature only)\n"
      "  --grayscale              Force grayscale\n"
      "  --gamma G                Gamma correction (e.g. 2.2)\n"
      "  --brightness B           Brightness add [-1..1]\n"
      "  --contrast C             Contrast multiplier (default 1.0)\n"
      "  --pixelate N             Block size >=2 for coarse pixelation\n"
      "  --no-adaptive            Disable activity-based dedupe gating\n"
      "  --auto-tune-deviation    Run quick deviation router parameter sweep before capture\n"
      "  --guard                  Wrap main capture in ContextOverflowGuard\n"
      "  --packed-store           (Reserved) future packed frame store toggle\n"
      "  --help                   Show this help\n";
}

int main(int argc, char** argv) {
#ifdef _WIN32
    // ---------------- CLI Parameters ----------------
    bool   do_capture        = false;
    int    display_index     = 1;
    bool   live_view         = false;
    int    fps               = 30;
    int    seconds           = 5;
    size_t resize_w          = 0;
    size_t resize_h          = 0;
    std::string record_base;

    // Static gate
    bool   require_static    = true;
    double static_sec        = 1.0;
    double static_timeout    = 10.0;
    bool   static_tolerant   = false;

    // Filters
    FrameFilters filters{};
    bool use_filters = false;

    // Activity tracker (adaptive dedupe gating)
    bool adaptive_enabled    = true;
    double static_thr        = 0.03;
    double wake_thr          = 0.05;
    double dedupe_pause_sec  = 15.0;
    double static_reset_sec  = 15.0;
    int    sample_stride     = 4;
    int    channel_thr       = 4;

    // Metrics
    std::string metrics_path;
    bool metrics_enabled = false;

    // Optional modules
    bool auto_tune_deviation = false;
    bool use_guard           = false;
    bool use_packed_store    = false; // future integration (placeholder flag)

    // Parse args
    for (int i = 1; i < argc; ++i) {
        if (eq_flag(argv[i], "--help")) { print_help(); return 0; }
        else if (eq_flag(argv[i], "--capture") && i+1 < argc) { do_capture = true; display_index = std::atoi(argv[++i]); }
        else if (eq_flag(argv[i], "--live")) { live_view = true; }
        else if (eq_flag(argv[i], "--fps") && i+1 < argc) { fps = std::atoi(argv[++i]); }
        else if (eq_flag(argv[i], "--seconds") && i+1 < argc) { seconds = std::atoi(argv[++i]); }
        else if (eq_flag(argv[i], "--resize") && i+1 < argc) {
            const char* s = argv[++i]; const char* x = std::strchr(s,'x'); if (!x) x=std::strchr(s,'X');
            if (x) { resize_w = std::strtoull(s,nullptr,10); resize_h = std::strtoull(x+1,nullptr,10); }
        }
        else if (eq_flag(argv[i], "--record") && i+1 < argc) { record_base = argv[++i]; }
        else if (eq_flag(argv[i], "--metrics") && i+1 < argc) { metrics_path = argv[++i]; metrics_enabled = true; }
        else if (eq_flag(argv[i], "--no-static-gate")) { require_static = false; }
        else if (eq_flag(argv[i], "--static-sec") && i+1 < argc) { static_sec = std::atof(argv[++i]); }
        else if (eq_flag(argv[i], "--static-timeout") && i+1 < argc) { static_timeout = std::atof(argv[++i]); }
        else if (eq_flag(argv[i], "--static-tolerant")) { static_tolerant = true; }
        else if (eq_flag(argv[i], "--grayscale")) { filters.grayscale = true; use_filters = true; }
        else if (eq_flag(argv[i], "--gamma") && i+1 < argc) { filters.gamma = std::atof(argv[++i]); use_filters = true; }
        else if (eq_flag(argv[i], "--brightness") && i+1 < argc) { filters.brightness = std::atof(argv[++i]); use_filters = true; }
        else if (eq_flag(argv[i], "--contrast") && i+1 < argc) { filters.contrast = std::atof(argv[++i]); use_filters = true; }
        else if (eq_flag(argv[i], "--pixelate") && i+1 < argc) { filters.pixelate = true; filters.pixel_size = std::max(2, std::atoi(argv[++i])); use_filters = true; }
        else if (eq_flag(argv[i], "--no-adaptive")) { adaptive_enabled = false; }
        else if (eq_flag(argv[i], "--auto-tune-deviation")) { auto_tune_deviation = true; }
        else if (eq_flag(argv[i], "--guard")) { use_guard = true; }
        else if (eq_flag(argv[i], "--packed-store")) { use_packed_store = true; }
    }

    if (!do_capture) {
        std::cout << "No capture mode selected. Use --capture N (see --help).\n";
        return 0;
    }

    if (fps <= 0) fps = 1;
    const int total_frames = (seconds <= 0) ? 1 : std::max(1, seconds * fps);

    // Auto-tune deviation router (fast synthetic pass) – placeholder invocation
    if (auto_tune_deviation) {
        std::string best = cortex::autotune_deviation_router();
        if (!best.empty()) {
            std::cout << "[auto-tune] Deviation router best config: " << best << "\n";
        }
    }

    auto mon = get_monitor_by_display_index(display_index);
    if (!mon) {
        std::cout << "Display " << display_index << " not found.\n";
        return 1;
    }

    std::cout << "Capture start\n"
              << "  display=" << display_index
              << " size=" << mon->width << "x" << mon->height
              << " fps=" << fps
              << " seconds=" << seconds
              << " resize=" << (resize_w && resize_h ? std::to_string(resize_w)+"x"+std::to_string(resize_h) : "native")
              << " static_gate=" << (require_static?"yes":"no")
              << " adaptive=" << (adaptive_enabled?"on":"off")
              << " guard=" << (use_guard?"on":"off")
              << " packed_store=" << (use_packed_store?"on":"off (placeholder)")
              << "\n";

    // Static scene gate
    if (require_static) {
        std::cout << "Waiting for static scene: need " << static_sec
                  << "s stable (timeout " << static_timeout << "s)\n";
        auto chk = wait_for_static_scene(display_index,
                                         fps,
                                         static_sec,
                                         static_timeout,
                                         resize_w, resize_h,
                                         static_tolerant);
        if (!chk.ok) {
            std::cout << "Static gate failed: " << chk.message << "\n";
            return 2;
        }
        std::cout << "Static scene confirmed after " << chk.stable_seconds << "s\n";
    }

    // Prepare recording directory
    if (!record_base.empty()) {
        fs::path rb(record_base);
        if (rb.has_parent_path()) {
            std::error_code ec;
            fs::create_directories(rb.parent_path(), ec);
        }
    }

    LiveViewerWin viewer;
    if (live_view) {
        int vw = resize_w ? static_cast<int>(resize_w) : mon->width;
        int vh = resize_h ? static_cast<int>(resize_h) : mon->height;
        viewer.create(vw, vh, "Cortex Live Viewer");
    }

    // Frame pool
    LLMFramePool pool(/*retention*/600.0, /*MB*/2048, fps);
    pool.set_single_static_mode(true, 1.0);

    // Activity tracker
    SceneActivityTracker tracker(SceneActivityConfig{
        static_thr, wake_thr, dedupe_pause_sec, static_reset_sec,
        sample_stride, channel_thr
    });

    // Metrics
    MetricsLogger mlog(metrics_path);
    if (metrics_enabled && !mlog.ok()) {
        std::cout << "WARN: Could not open metrics file '" << metrics_path << "'\n";
        metrics_enabled = false;
    }
    double last_aggregate_emit = 0.0;
    size_t frames_captured = 0;
    size_t frames_unique   = 0;
    size_t frames_duplicates = 0;

    // Correction queue (supports persistent and one-shot operations)
    CorrectionQueue corrections;
    if (filters.grayscale) {
        corrections.enqueue([](RawImage& img){
            const size_t n = img.width * img.height;
            for (size_t i = 0; i < n; ++i) {
                uint8_t* p = &img.bgra[i*4];
                int gray = static_cast<int>(0.299*p[2] + 0.587*p[1] + 0.114*p[0] + 0.5);
                p[0] = p[1] = p[2] = static_cast<uint8_t>(gray);
            }
        });
    }

    // Dedupe state
    bool have_prev = false;
    std::shared_ptr<RawImage> prev;
    Sig::OperandMap prev_sig{};
    size_t skipped_dupes = 0;

    // Activity counters
    size_t quiet_frames = 0;
    size_t static_frames = 0;
    size_t dedupe_block_frames = 0;

    // Timing control
    const auto frame_interval = std::chrono::microseconds(1'000'000 / fps);
    auto next_time = std::chrono::high_resolution_clock::now();

    // Optional guard wrapper (simple RAII)
    std::unique_ptr<RuntimeAdaptiveGuard> guard_ptr;
    if (use_guard) {
        guard_ptr = std::make_unique<RuntimeAdaptiveGuard>(
            2048,  // base allocation
            50,    // overflow threshold MB
            true,  // enable worker delegation
            true,  // enable recursive protection
            2,     // max helper threads
            3      // max recursive depth
        );
        guard_ptr->enter();
    }

    // Capture loop
    for (int f = 0; f < total_frames; ++f) {
        RawImage raw = capture_monitor_bgra_by_display_index(display_index);
        if (!raw.ok()) continue;

        RawImage working = raw;
        if (resize_w && resize_h) {
            RawImage r = resize_bgra_bilinear(raw, resize_w, resize_h);
            if (r.ok()) working = std::move(r);
        }

        auto frame_sp = std::make_shared<RawImage>(std::move(working));
        auto cur_sig = Sig::compute_operand_map(*frame_sp);

        // Activity tracking / adaptive gating
        SceneActivityDecision decision{};
        double diff_ratio = 0.0;
        if (adaptive_enabled && have_prev && prev) {
            decision = tracker.update(*frame_sp, prev.get(), static_cast<double>(f)/fps);
            diff_ratio = decision.diff_ratio;
            if (decision.quiet_active)   ++quiet_frames;
            if (decision.is_static_scene) ++static_frames;
            if (decision.dedupe_block)   ++dedupe_block_frames;
        } else {
            decision.allow_dedupe = true;
        }

        bool identical = false;
        if (decision.allow_dedupe && have_prev && prev) {
            identical = Sig::frames_identical(*frame_sp, *prev, cur_sig, prev_sig);
        }

        // Corrections / filters (after dedupe decision for signature fairness)
        corrections.apply_all(*frame_sp);
        if (use_filters) apply_filters_inplace(*frame_sp, filters);

        if (live_view) viewer.update(*frame_sp);

        double tsec = static_cast<double>(f) / fps;
        pool.push(frame_sp, f, tsec);

        // Record if unique or dedupe-block (ensures frames during important transitions are kept)
        if (!record_base.empty()) {
            if (decision.dedupe_block || !identical) {
                std::string path = make_numbered(record_base, f, ".bmp", 6);
                RawImageBMPView view{ frame_sp->bgra.data(), frame_sp->width, frame_sp->height };
                write_bmp32(path, view);
            } else {
                ++skipped_dupes;
            }
        }

        // Metrics
        ++frames_captured;
        if (identical) ++frames_duplicates; else ++frames_unique;

        if (metrics_enabled) {
            auto st = pool.snapshot_recent(0.0); // cheap latest snapshot (we only need last size)
            size_t pool_frames = st.empty() ? 0 : st.size(); // NOTE: Could expose stats() if you add it
            mlog.write_object([&](std::ofstream& os){
                os  << "\"type\":\"frame\""
                    << ",\"t\":" << mlog.wall_seconds()
                    << ",\"frame_index\":" << f
                    << ",\"tsec\":" << tsec
                    << ",\"unique\":" << (identical?0:1)
                    << ",\"dup_skipped_total\":" << frames_duplicates
                    << ",\"pool_frames\":" << pool_frames
                    << ",\"rss_mb\":" << (process_rss_bytes() / (1024.0*1024.0))
                    << ",\"diff_ratio\":" << diff_ratio
                    << ",\"dedupe_block\":" << (decision.dedupe_block?1:0);
            });
            double noww = mlog.wall_seconds();
            if (noww - last_aggregate_emit >= 1.0) {
                last_aggregate_emit = noww;
                double unique_ratio = frames_captured ? (double)frames_unique / frames_captured : 0.0;
                mlog.write_object([&](std::ofstream& os){
                    os  << "\"type\":\"aggregate\""
                        << ",\"t\":" << noww
                        << ",\"frames_captured\":" << frames_captured
                        << ",\"frames_unique\":" << frames_unique
                        << ",\"frames_duplicates\":" << frames_duplicates
                        << ",\"unique_ratio\":" << unique_ratio
                        << ",\"rss_mb\":" << (process_rss_bytes() / (1024.0*1024.0))
                        << ",\"quiet_frames\":" << quiet_frames
                        << ",\"static_frames\":" << static_frames
                        << ",\"dedupe_block_frames\":" << dedupe_block_frames;
                });
            }
        }

        prev = frame_sp;
        prev_sig = cur_sig;
        have_prev = true;

        if (total_frames > 1) {
            next_time += frame_interval;
            std::this_thread::sleep_until(next_time);
        }
    }

    if (guard_ptr) {
        guard_ptr->exit();
        RuntimeAdaptiveGuard::print_global_statistics();
    }

    std::cout << "Capture complete.";
    if (!record_base.empty()) std::cout << " Duplicates skipped=" << skipped_dupes;
    std::cout << "\nAdaptive summary:\n";
    if (adaptive_enabled) {
        std::cout << "  quiet frames:        " << quiet_frames << "\n"
                  << "  static frames:       " << static_frames << "\n"
                  << "  dedupe-block frames: " << dedupe_block_frames << "\n";
    } else {
        std::cout << "  adaptive disabled\n";
    }

    if (metrics_enabled) {
        std::cout << "Metrics written to: " << metrics_path << "\n";
    }

    return 0;
#else
    (void)argc; (void)argv;
    std::cout << "Capture not supported on this platform.\n";
    return 0;
#endif
}