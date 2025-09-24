#pragma once
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <utility>
#include "screen_capture_win.hpp"
#include "image_ops.hpp"
#include "operand_map.hpp"

namespace cortex {

struct StaticSceneCheckResult {
    bool ok{false};
    double stable_seconds{0.0};
    int frames_observed{0};
    int identical_streak{0};
    sig::OperandMap signature{};
    std::shared_ptr<RawImage> frame;
    std::string message;
};

inline StaticSceneCheckResult wait_for_static_scene(
    int display_index,
    int fps_hint,
    double min_stable_sec,
    double timeout_sec,
    size_t resize_w = 0,
    size_t resize_h = 0,
    bool tolerant = false)
{
    StaticSceneCheckResult out{};
#ifdef _WIN32
    if (fps_hint <= 0) fps_hint = 1;
    const auto interval = std::chrono::microseconds(1'000'000 / fps_hint);
    const auto t0 = std::chrono::high_resolution_clock::now();
    auto next = t0;

    if (!get_monitor_by_display_index(display_index)) {
        out.message = "Display not found.";
        return out;
    }

    bool have_prev = false;
    std::shared_ptr<RawImage> prev;
    sig::OperandMap prev_map{};
    int streak = 0;
    const int needed = std::max(1, static_cast<int>(min_stable_sec * fps_hint));

    while (true) {
        const auto now = std::chrono::high_resolution_clock::now();
        const double elapsed = std::chrono::duration<double>(now - t0).count();
        if (elapsed >= timeout_sec) {
            out.ok = (streak >= needed);
            out.stable_seconds = static_cast<double>(streak) / std::max(1, fps_hint);
            out.frames_observed; // keep as-is
            out.message = out.ok ? "Stable at timeout boundary." : "Timeout: scene did not become static.";
            if (out.ok && prev) { out.frame = prev; out.signature = prev_map; }
            return out;
        }

        RawImage raw = capture_monitor_bgra_by_display_index(display_index);
        if (!raw.ok()) { std::this_thread::sleep_for(interval); continue; }

        RawImage working = raw;
        if (resize_w && resize_h) {
            RawImage resized = resize_bgra_bilinear(raw, resize_w, resize_h);
            if (resized.ok()) working = std::move(resized);
        }

        auto sp = std::make_shared<RawImage>(std::move(working));
        const sig::OperandMap cur = sig::compute_operand_map(*sp);

        bool identical = false;
        if (have_prev && prev) {
            identical = tolerant ? sig::same_signature(prev_map, cur)
                                 : sig::frames_identical(*sp, *prev, cur, prev_map);
        }
        streak = identical ? (streak + 1) : 1;

        ++out.frames_observed;
        prev = sp; prev_map = cur; have_prev = true;

        if (streak >= needed) {
            out.ok = true;
            out.stable_seconds = static_cast<double>(streak) / std::max(1, fps_hint);
            out.identical_streak = streak;
            out.frame = sp;
            out.signature = cur;
            out.message = "Static scene confirmed.";
            return out;
        }

        next += interval;
        std::this_thread::sleep_until(next);
    }
#else
    out.message = "Static scene detection requires Windows capture.";
    return out;
#endif
}

} // namespace cortex