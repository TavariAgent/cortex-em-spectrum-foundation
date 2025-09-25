#include <algorithm>
#include <iostream>
#include <chrono>
#include <cstring>
#include <thread>
#include <string>
#include <memory>
#include <cstdlib>
#include <filesystem>

#include "screen_capture_win.hpp"
#include "image_ops.hpp"
#include "operand_map.hpp"
#include "llm_frame_pool.hpp"
#include "live_viewer_win.hpp"
#include "frame_recorder.hpp"
#include "filters.hpp"
#include "correction_queue.hpp"
#include "static_scene_detector.hpp"    // initial stability gate
#include "scene_activity_tracker.hpp"   // adaptive dedupe control

namespace Sig = cortex::sig;
namespace fs  = std::filesystem;

static bool eq_flag(const char* a, const char* b) { return std::strcmp(a,b)==0; }

int main(int argc, char** argv) {
    using namespace cortex;

    // CLI settings
    bool   do_capture       = false;
    int    display_index    = 1;
    bool   live_view        = false;
    int    fps              = 30;
    int    seconds          = 5;
    size_t resize_w         = 0;
    size_t resize_h         = 0;
    std::string record_base;

    // Static gate
    bool   require_static   = true;
    double static_sec       = 1.0;
    double static_timeout   = 10.0;
    bool   static_tolerant  = false;

    // Filters
    FrameFilters filters{};
    bool use_filters = false;

    // Adaptive activity
    double static_thr       = 0.03;
    double wake_thr         = 0.05;
    double dedupe_pause_sec = 15.0;
    double static_reset_sec = 15.0;
    int    sample_stride    = 4;
    int    channel_thr      = 4;
    bool   adaptive_enabled = true;

    for (int i = 1; i < argc; ++i) {
        if (eq_flag(argv[i], "--help")) {
            std::cout <<
              "Usage: cortex_em_cli --capture N [options]\n"
              "General:\n"
              "  --capture N          Display index\n"
              "  --live               Show live viewer\n"
              "  --fps F              Frames per second (default 30)\n"
              "  --seconds S          Duration (0 => single frame)\n"
              "  --resize WxH         Resize\n"
              "  --record base        Write frames as base_XXXXXX.bmp (dedup aware)\n"
              "\nStatic gate:\n"
              "  --no-static-gate     Skip stability wait\n"
              "  --static-sec X       Required stable seconds (default 1.0)\n"
              "  --static-timeout T   Timeout for gate (default 10)\n"
              "  --static-tolerant    Use signature equality only\n"
              "\nFilters:\n"
              "  --grayscale --gamma G --brightness B --contrast C\n"
              "\nAdaptive activity:\n"
              "  --static-thr X       (default 0.03)\n"
              "  --wake-thr X         (default 0.05)\n"
              "  --dedupe-pause N     (default 15)\n"
              "  --static-reset N     (default 15)\n"
              "  --sample-stride N    (default 4)\n"
              "  --channel-thr N      (default 4)\n"
              "  --no-adaptive        Disable adaptive dedupe control\n";
            return 0;
        }
        else if (eq_flag(argv[i], "--capture") && i+1 < argc) { do_capture = true; display_index = std::atoi(argv[++i]); }
        else if (eq_flag(argv[i], "--live")) { live_view = true; }
        else if (eq_flag(argv[i], "--fps") && i+1 < argc) { fps = std::atoi(argv[++i]); }
        else if (eq_flag(argv[i], "--seconds") && i+1 < argc) { seconds = std::atoi(argv[++i]); }
        else if (eq_flag(argv[i], "--resize") && i+1 < argc) {
            const char* s = argv[++i]; const char* x = std::strchr(s,'x'); if (!x) x=std::strchr(s,'X');
            if (x) { resize_w = std::strtoull(s,nullptr,10); resize_h = std::strtoull(x+1,nullptr,10); }
        }
        else if (eq_flag(argv[i], "--record") && i+1 < argc) { record_base = argv[++i]; }
        else if (eq_flag(argv[i], "--no-static-gate")) { require_static = false; }
        else if (eq_flag(argv[i], "--static-sec") && i+1 < argc) { static_sec = std::atof(argv[++i]); }
        else if (eq_flag(argv[i], "--static-timeout") && i+1 < argc) { static_timeout = std::atof(argv[++i]); }
        else if (eq_flag(argv[i], "--static-tolerant")) { static_tolerant = true; }
        else if (eq_flag(argv[i], "--grayscale")) { filters.grayscale = true; use_filters = true; }
        else if (eq_flag(argv[i], "--gamma") && i+1 < argc) { filters.gamma = std::atof(argv[++i]); use_filters = true; }
        else if (eq_flag(argv[i], "--brightness") && i+1 < argc) { filters.brightness = std::atof(argv[++i]); use_filters = true; }
        else if (eq_flag(argv[i], "--contrast") && i+1 < argc) { filters.contrast = std::atof(argv[++i]); use_filters = true; }
        else if (eq_flag(argv[i], "--static-thr") && i+1 < argc) { static_thr = std::atof(argv[++i]); }
        else if (eq_flag(argv[i], "--wake-thr") && i+1 < argc) { wake_thr = std::atof(argv[++i]); }
        else if (eq_flag(argv[i], "--dedupe-pause") && i+1 < argc) { dedupe_pause_sec = std::atof(argv[++i]); }
        else if (eq_flag(argv[i], "--static-reset") && i+1 < argc) { static_reset_sec = std::atof(argv[++i]); }
        else if (eq_flag(argv[i], "--sample-stride") && i+1 < argc) { sample_stride = std::atoi(argv[++i]); }
        else if (eq_flag(argv[i], "--channel-thr") && i+1 < argc) { channel_thr = std::atoi(argv[++i]); }
        else if (eq_flag(argv[i], "--no-adaptive")) { adaptive_enabled = false; }
    }

#ifndef _WIN32
    std::cout << "Capture only supported on Windows.\n";
    return do_capture ? 1 : 0;
#else
    if (!do_capture) {
        std::cout << "Use --capture N (see --help)\n";
        return 0;
    }

    std::cout << "Capture start: display=" << display_index
              << " fps=" << fps
              << " seconds=" << seconds
              << " resize=" << (resize_w && resize_h ? std::to_string(resize_w)+"x"+std::to_string(resize_h) : "native")
              << " static_gate=" << (require_static?"yes":"no")
              << " adaptive=" << (adaptive_enabled?"on":"off")
              << "\n";

    auto mon = get_monitor_by_display_index(display_index);
    if (!mon) {
        std::cout << "Display " << display_index << " not found.\n";
        return 1;
    }

    if (require_static) {
        std::cout << "Waiting for static scene (" << static_sec << "s, timeout "
                  << static_timeout << "s)...\n";
        auto chk = wait_for_static_scene(display_index,
                                         std::max(1,fps),
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

    const int total_frames = (seconds <= 0)
        ? 1
        : std::max(1, seconds * std::max(1,fps));

    const auto interval = std::chrono::microseconds(1'000'000 / std::max(1,fps));
    auto next_time = std::chrono::high_resolution_clock::now();

    LLMFramePool pool(600.0, 2048, fps);
    pool.set_single_static_mode(true, 1.0);

    SceneActivityTracker tracker(SceneActivityConfig{
        static_thr, wake_thr, dedupe_pause_sec, static_reset_sec,
        sample_stride, channel_thr
    });

    bool have_prev = false;
    std::shared_ptr<RawImage> prev;
    Sig::OperandMap prev_sig{};
    size_t skipped_dupes = 0;
    size_t quiet_frames = 0;
    size_t static_frames = 0;
    size_t dedupe_block_frames = 0;

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

    for (int f = 0; f < total_frames; ++f) {
        RawImage raw = capture_monitor_bgra_by_display_index(display_index);
        if (!raw.ok()) continue;

        RawImage working = raw;
        if (resize_w && resize_h) {
            RawImage r = resize_bgra_bilinear(raw, resize_w, resize_h);
            if (r.ok()) working = std::move(r);
        }

        auto frame_sp = std::make_shared<RawImage>(std::move(working));

        SceneActivityDecision decision{};
        double diff_ratio = 0.0;
        if (adaptive_enabled && have_prev && prev) {
            decision = tracker.update(*frame_sp, prev.get(),
                                      static_cast<double>(f)/std::max(1,fps));
            diff_ratio = decision.diff_ratio;
            if (decision.quiet_active) ++quiet_frames;
            if (decision.is_static_scene) ++static_frames;
            if (decision.dedupe_block) ++dedupe_block_frames;
        } else {
            decision.allow_dedupe = true;
        }

        corrections.apply_all(*frame_sp);
        if (use_filters) apply_filters_inplace(*frame_sp, filters);

        auto cur_sig = Sig::compute_operand_map(*frame_sp);
        bool identical = false;
        if (decision.allow_dedupe && have_prev && prev) {
            identical = Sig::frames_identical(*frame_sp, *prev, cur_sig, prev_sig);
        }

        if (live_view) viewer.update(*frame_sp);

        pool.push(frame_sp, f, static_cast<double>(f)/std::max(1,fps));

        if (!record_base.empty()) {
            if (decision.dedupe_block || !identical) {
                std::string path = make_numbered(record_base, f, ".bmp", 6);
                RawImageBMPView view{ frame_sp->bgra.data(), frame_sp->width, frame_sp->height };
                write_bmp32(path, view);
            } else {
                ++skipped_dupes;
            }
        }

        prev = frame_sp;
        prev_sig = cur_sig;
        have_prev = true;

        if (f % std::max(1,fps) == 0) {
            std::cout << "[activity] f=" << f
                      << " diff=" << (diff_ratio*100.0) << "% "
                      << " static=" << decision.is_static_scene
                      << " awake=" << decision.is_scene_awake
                      << " quiet=" << decision.quiet_active
                      << " dedupe_block=" << decision.dedupe_block
                      << " allow_dedupe=" << decision.allow_dedupe
                      << " copilot_block=" << decision.copilot_block
                      << "\n";
        }

        if (total_frames > 1) {
            next_time += interval;
            std::this_thread::sleep_until(next_time);
        }
    }

    std::cout << "Capture complete.";
    if (!record_base.empty()) std::cout << " Duplicates skipped=" << skipped_dupes;
    std::cout << "\nAdaptive summary:\n";
    if (adaptive_enabled) {
        std::cout << "  quiet-active frames: " << quiet_frames << "\n"
                  << "  static frames:       " << static_frames << "\n"
                  << "  dedupe-block frames: " << dedupe_block_frames << "\n";
    } else {
        std::cout << "  adaptive disabled\n";
    }
    return 0;
#endif
}