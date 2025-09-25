#pragma once
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <string>
#include <algorithm>
#include <cmath>

#include "screen_capture_win.hpp"  // RawImage + capture_monitor_bgra_by_display_index
#include "image_ops.hpp"           // resize_bgra_bilinear (optional)

namespace cortex {

/*
    Static / activity detection model (sampled):

    diff_ratio = (#sampled pixels whose |channel delta| > channel_threshold) / sampled_pixels

    Threshold semantics:
      - diff_ratio <= static_threshold        -> static
      - diff_ratio >= wake_threshold          -> high activity
      - static_threshold < diff < wake_threshold -> mid band (quiet / ambiguous)

    Gate policy (blocking):
      - We require continuous time with diff_ratio <= static_threshold
        accumulating to required_static_seconds.
      - If diff >= wake_threshold: streak resets.
      - If mid-band:
           freeze_streak_on_midband == true  -> neither increment nor reset (hold)
           freeze_streak_on_midband == false -> reset streak.
*/

struct StaticActivityConfig {
    double static_threshold{0.03};       // <= 3% = static
    double wake_threshold{0.05};         // >= 5% = high / motion
    bool   freeze_streak_on_midband{true}; // hold streak during 3–5% noise if true
    int    sample_stride{4};             // pixel sampling stride
    int    channel_threshold{4};         // per-channel (0..255) delta to count as changed
};

struct StaticGateResult {
    bool ok{false};
    double stable_seconds{0.0};
    int frames_observed{0};
    double last_diff_ratio{0.0};
    std::string message;
};

struct ActivityDecision {
    double diff_ratio{1.0};
    bool is_static{false};
    bool is_high_activity{false};
    bool is_mid_band{false};
    bool gate_static_condition{false};    // qualifies for gate accumulation
    double seconds_in_static_streak{0.0};
};

/* --- Sampling diff ratio (standalone helper) --- */
inline double compute_sampled_diff_ratio(const RawImage& a,
                                         const RawImage& b,
                                         int stride,
                                         int channel_thr)
{
    if (!a.ok() || !b.ok() || a.width != b.width || a.height != b.height) return 1.0;
    stride = std::max(1, stride);
    channel_thr = std::max(0, channel_thr);

    const size_t W = a.width;
    const size_t H = a.height;
    size_t sampled = 0;
    size_t changed = 0;

    for (size_t y = 0; y < H; y += static_cast<size_t>(stride)) {
        const size_t row = y * W;
        for (size_t x = 0; x < W; x += static_cast<size_t>(stride)) {
            const size_t i = (row + x) * 4;
            const uint8_t* pa = &a.bgra[i];
            const uint8_t* pb = &b.bgra[i];
            const int db = std::abs(int(pa[0]) - int(pb[0]));
            const int dg = std::abs(int(pa[1]) - int(pb[1]));
            const int dr = std::abs(int(pa[2]) - int(pb[2]));
            if (db > channel_thr || dg > channel_thr || dr > channel_thr)
                ++changed;
            ++sampled;
        }
    }
    if (!sampled) return 0.0;
    return static_cast<double>(changed) / static_cast<double>(sampled);
}

/* --- Incremental detector (use inside capture loop) --- */
class StaticActivityDetector {
public:
    explicit StaticActivityDetector(StaticActivityConfig cfg = {})
        : cfg_(cfg) {}

    // Pass current frame + time (seconds since start). Returns classification & streak info.
    ActivityDecision update(const RawImage& current, double tsec) {
        ActivityDecision dec{};
        if (!last_ || !last_->ok() || !current.ok()) {
            last_ = std::make_shared<RawImage>(current);
            start_or_continue_streak(tsec, true);
            dec.diff_ratio = 0.0;
            dec.is_static = true;
            dec.gate_static_condition = true;
            dec.seconds_in_static_streak = 0.0;
            return dec;
        }

        double diff = compute_sampled_diff_ratio(current, *last_,
                                                 cfg_.sample_stride,
                                                 cfg_.channel_threshold);
        dec.diff_ratio = diff;
        classify(diff, dec);

        // Gate streak logic
        if (dec.is_static) {
            start_or_continue_streak(tsec, false);
        } else if (dec.is_mid_band && cfg_.freeze_streak_on_midband) {
            // hold streak (no reset)
        } else {
            reset_streak();
        }

        dec.seconds_in_static_streak = streak_active_ ? (tsec - streak_start_) : 0.0;
        last_ = std::make_shared<RawImage>(current);
        return dec;
    }

    double current_static_streak_seconds(double tsec) const {
        return streak_active_ ? (tsec - streak_start_) : 0.0;
    }

    void reset() {
        last_.reset();
        reset_streak();
    }

    const StaticActivityConfig& config() const { return cfg_; }

private:
    StaticActivityConfig cfg_;
    std::shared_ptr<RawImage> last_;

    bool   streak_active_{false};
    double streak_start_{0.0};

    void reset_streak() {
        streak_active_ = false;
        streak_start_ = 0.0;
    }

    void start_or_continue_streak(double tsec, bool force_new) {
        if (!streak_active_ || force_new) {
            streak_active_ = true;
            streak_start_ = tsec;
        }
    }

    void classify(double diff, ActivityDecision& dec) const {
        if (diff <= cfg_.static_threshold) {
            dec.is_static = true;
            dec.gate_static_condition = true;
        } else if (diff >= cfg_.wake_threshold) {
            dec.is_high_activity = true;
        } else {
            dec.is_mid_band = true;
        }
    }
};

/* --- Blocking gate function (similar purpose to wait_for_static_scene) --- */
inline StaticGateResult static_activity_gate(int display_index,
                                             int fps_hint,
                                             double required_static_seconds,
                                             double timeout_seconds,
                                             StaticActivityConfig cfg = {},
                                             size_t resize_w = 0,
                                             size_t resize_h = 0)
{
    StaticGateResult out{};
#ifndef _WIN32
    out.ok = false;
    out.message = "static_activity_gate requires Windows capture backend.";
    return out;
#else
    fps_hint = std::max(1, fps_hint);
    const auto interval = std::chrono::microseconds(1'000'000 / fps_hint);
    const auto t0 = std::chrono::high_resolution_clock::now();
    auto next = t0;

    if (!get_monitor_by_display_index(display_index)) {
        out.message = "Display not found.";
        return out;
    }

    bool have_prev = false;
    RawImage prev;
    double streak_start_sec = 0.0;
    bool streak_active = false;

    while (true) {
        const auto now = std::chrono::high_resolution_clock::now();
        const double elapsed = std::chrono::duration<double>(now - t0).count();
        if (elapsed >= timeout_seconds) {
            out.ok = (streak_active &&
                      (elapsed - streak_start_sec) >= required_static_seconds);
            out.stable_seconds = streak_active ? (elapsed - streak_start_sec) : 0.0;
            out.message = out.ok ? "Static at timeout boundary." : "Timeout without sufficient static interval.";
            return out;
        }

        RawImage raw = capture_monitor_bgra_by_display_index(display_index);
        if (!raw.ok()) {
            std::this_thread::sleep_for(interval);
            continue;
        }

        RawImage working = raw;
        if (resize_w && resize_h) {
            RawImage r = resize_bgra_bilinear(raw, resize_w, resize_h);
            if (r.ok()) working = std::move(r);
        }

        double diff_ratio = 1.0;
        if (have_prev) {
            diff_ratio = compute_sampled_diff_ratio(working, prev,
                                                    cfg.sample_stride,
                                                    cfg.channel_threshold);
        } else {
            diff_ratio = 0.0; // first frame
        }

        out.last_diff_ratio = diff_ratio;
        ++out.frames_observed;

        // Classification
        bool is_static = diff_ratio <= cfg.static_threshold;
        bool is_high   = diff_ratio >= cfg.wake_threshold;
        bool mid_band  = (!is_static && !is_high);

        if (is_static) {
            if (!streak_active) {
                streak_active = true;
                streak_start_sec = elapsed;
            }
        } else if (mid_band && cfg.freeze_streak_on_midband) {
            // hold streak (do nothing)
        } else {
            streak_active = false;
        }

        if (streak_active) {
            out.stable_seconds = elapsed - streak_start_sec;
            if (out.stable_seconds >= required_static_seconds) {
                out.ok = true;
                out.message = "Static activity gate satisfied.";
                return out;
            }
        }

        prev = std::move(working);
        have_prev = true;

        next += interval;
        std::this_thread::sleep_until(next);
    }
#endif
}

} // namespace cortex