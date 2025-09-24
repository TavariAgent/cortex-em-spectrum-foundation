#include <algorithm>
#include <iostream>
#include <numeric>
#include <chrono>
#include <cstring>
#include <thread>
#include <string>
#include <memory>
#include <cstdlib>

#include "static_scene_detector.hpp"
#include "screen_capture_win.hpp"
#include "image_ops.hpp"
#include "operand_map.hpp"
#include "llm_frame_pool.hpp"
#include "live_viewer_win.hpp"
#include "frame_recorder.hpp"
#include "filters.hpp"            // (optional simple filters)
#include "correction_queue.hpp"   // new (see below)

namespace Sig = cortex::sig;

static bool eq_flag(const char* a, const char* b) { return std::strcmp(a,b)==0; }

int main(int argc, char** argv) {
    using namespace cortex;

    // Capture configuration
    bool   do_capture          = false;
    int    display_index       = 1;
    bool   live_view           = false;
    int    fps                 = 30;
    int    seconds             = 5;
    size_t resize_w            = 0;
    size_t resize_h            = 0;
    std::string record_base;
    bool   require_static      = true;
    double static_sec          = 1.0;
    double static_timeout      = 10.0;
    bool   static_tolerant     = false;

    // Simple filter toggles (demonstration only)
    FrameFilters filters{};
    bool use_filters = false;

    // Parse args
    for (int i = 1; i < argc; ++i) {
        if (eq_flag(argv[i], "--capture") && i+1 < argc) { do_capture = true; display_index = std::atoi(argv[++i]); }
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
        else if (eq_flag(argv[i], "--filters")) { use_filters = true; }
        else if (eq_flag(argv[i], "--grayscale")) { filters.grayscale = true; use_filters = true; }
        else if (eq_flag(argv[i], "--gamma") && i+1 < argc) { filters.gamma = std::atof(argv[++i]); use_filters = true; }
        else if (eq_flag(argv[i], "--brightness") && i+1 < argc) { filters.brightness = std::atof(argv[++i]); use_filters = true; }
        else if (eq_flag(argv[i], "--contrast") && i+1 < argc) { filters.contrast = std::atof(argv[++i]); use_filters = true; }
    }

#ifndef _WIN32
    if (do_capture) {
        std::cout << "Capture only supported on Windows right now.\n";
        return 1;
    }
    std::cout << "Nothing to do.\n";
    return 0;
#else
    if (!do_capture) {
        std::cout << "No mode specified. Use --capture N\n";
        return 0;
    }

    std::cout << "Capture start: display=" << display_index
              << " fps=" << fps << " seconds=" << seconds
              << " resize=" << (resize_w && resize_h ? std::to_string(resize_w)+"x"+std::to_string(resize_h) : "native")
              << " static_gate=" << (require_static?"yes":"no")
              << "\n";

    auto mon = get_monitor_by_display_index(display_index);
    if (!mon) {
        std::cout << "Display " << display_index << " not found.\n";
        return 1;
    }

    if (require_static) {
        std::cout << "Waiting for static scene (" << static_sec << "s needed, timeout "
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

    LiveViewerWin viewer;
    if (live_view) {
        int vw = resize_w ? static_cast<int>(resize_w) : mon->width;
        int vh = resize_h ? static_cast<int>(resize_h) : mon->height;
        viewer.create(vw, vh, "Cortex Live Viewer");
    }

    const int total_frames = (seconds <= 0) ? 1 : std::max(1, seconds * std::max(1,fps));
    const auto interval = std::chrono::microseconds(1'000'000 / std::max(1,fps));
    auto next_time = std::chrono::high_resolution_clock::now();

    LLMFramePool pool(/*retention*/600.0, /*MB*/2048, fps);
    pool.set_single_static_mode(true, 1.0);

    bool have_prev = false;
    std::shared_ptr<RawImage> prev;
    Sig::OperandMap prev_sig{};
    size_t skipped_dupes = 0;

    CorrectionQueue corrections; // new queue
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
        auto cur_sig = Sig::compute_operand_map(*frame_sp);
        bool identical = have_prev && prev && Sig::frames_identical(*frame_sp, *prev, cur_sig, prev_sig);

        // Apply queued corrections if any (in-place)
        corrections.apply_all(*frame_sp);

        if (use_filters) apply_filters_inplace(*frame_sp, filters);

        if (live_view) viewer.update(*frame_sp);

        const double tsec = static_cast<double>(f) / std::max(1,fps);
        pool.push(frame_sp, f, tsec);

        if (!record_base.empty()) {
            if (!identical) {
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

        if (total_frames > 1) {
            next_time += interval;
            std::this_thread::sleep_until(next_time);
        }
    }

    std::cout << "Capture complete.";
    if (!record_base.empty()) std::cout << " Duplicates skipped=" << skipped_dupes;
    std::cout << "\n";
    return 0;
#endif
}