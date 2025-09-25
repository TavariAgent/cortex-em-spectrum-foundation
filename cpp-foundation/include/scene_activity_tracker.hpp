#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <algorithm>
#include <cmath>
#include "screen_capture_win.hpp"

namespace cortex {

/*
Adaptive scene activity tracker:

diff_ratio = fraction of sampled pixels whose per-channel delta > channel_thr

Bands:
  diff <= static_thr        -> static (eligible to sleep)
  diff >= wake_thr          -> high activity (start dedupe pause window)
  static_thr < diff < wake_thr -> quiet mid-band (subtle motion; wakes scene but not “high”)

Flags:
  is_static_scene  : current frame qualifies as static
  is_scene_awake   : latched after leaving static until enough continuous static resets it
  quiet_active     : in mid band
  dedupe_block     : inside high-activity suppression window (record all frames)
  allow_dedupe     : can skip identical frames (false when dedupe_block)
  copilot_block    : when “sleeping” static (optional downstream filter)
*/

struct SceneActivityConfig {
    double static_thr{0.03};
    double wake_thr{0.05};
    double dedupe_pause_sec{15.0};
    double static_reset_sec{15.0};
    int    sample_stride{4};
    int    channel_thr{4};
};

struct SceneActivityDecision {
    double diff_ratio{0.0};
    bool is_static_scene{false};
    bool is_scene_awake{false};
    bool quiet_active{false};
    bool dedupe_block{false};
    bool allow_dedupe{true};
    bool copilot_block{false};
    double seconds_in_static{0.0};
    double seconds_since_high{0.0};
};

inline double sampled_diff_ratio(const RawImage& cur,
                                 const RawImage& prev,
                                 int stride,
                                 int channel_thr)
{
    if (!cur.ok() || !prev.ok() ||
        cur.width != prev.width || cur.height != prev.height) return 1.0;
    stride = std::max(1, stride);
    channel_thr = std::max(0, channel_thr);

    const size_t W = cur.width;
    const size_t H = cur.height;
    size_t sampled = 0;
    size_t changed = 0;

    for (size_t y = 0; y < H; y += static_cast<size_t>(stride)) {
        const size_t row = y * W;
        for (size_t x = 0; x < W; x += static_cast<size_t>(stride)) {
            const size_t i = (row + x) * 4;
            const uint8_t* a = &prev.bgra[i];
            const uint8_t* b = &cur.bgra[i];
            const int db = std::abs(int(a[0]) - int(b[0]));
            const int dg = std::abs(int(a[1]) - int(b[1]));
            const int dr = std::abs(int(a[2]) - int(b[2]));
            if (db > channel_thr || dg > channel_thr || dr > channel_thr)
                ++changed;
            ++sampled;
        }
    }
    return sampled ? double(changed) / double(sampled) : 0.0;
}

class SceneActivityTracker {
public:
    explicit SceneActivityTracker(SceneActivityConfig cfg = {})
        : cfg_(cfg) {}

    SceneActivityDecision update(const RawImage& cur,
                                 const RawImage* prev,
                                 double tsec) {
        SceneActivityDecision d;

        if (!prev || !prev->ok() || !cur.ok()) {
            d.is_static_scene = true;
            d.is_scene_awake = false;
            start_static_if_needed(tsec, true);
            finalize_common(d, tsec);
            return d;
        }

        d.diff_ratio = sampled_diff_ratio(cur, *prev,
                                          cfg_.sample_stride,
                                          cfg_.channel_thr);

        const bool is_static = d.diff_ratio <= cfg_.static_thr;
        const bool is_high   = d.diff_ratio >= cfg_.wake_thr;
        const bool is_mid    = !is_static && !is_high;

        if (is_static) {
            start_static_if_needed(tsec, false);
            if (scene_awake_ &&
                (tsec - static_start_) >= cfg_.static_reset_sec &&
                (tsec - last_high_time_) >= cfg_.dedupe_pause_sec) {
                scene_awake_ = false;
            }
        } else {
            static_run_active_ = false;
        }

        if (is_high) {
            scene_awake_ = true;
            last_high_time_ = tsec;
            dedupe_block_until_ = tsec + cfg_.dedupe_pause_sec;
        } else if (is_mid) {
            scene_awake_ = true;
        }

        d.is_static_scene = is_static;
        d.is_scene_awake  = scene_awake_;
        d.quiet_active    = is_mid;
        d.dedupe_block    = (tsec < dedupe_block_until_);
        d.allow_dedupe    = !d.dedupe_block;
        d.copilot_block   = (is_static && !scene_awake_);
        d.seconds_in_static  = is_static ? (tsec - static_start_) : 0.0;
        d.seconds_since_high = tsec - last_high_time_;

        finalize_common(d, tsec);
        return d;
    }

private:
    SceneActivityConfig cfg_;
    bool   scene_awake_{false};
    bool   static_run_active_{false};
    double static_start_{0.0};
    double last_high_time_{-1e9};
    double dedupe_block_until_{-1e9};

    void start_static_if_needed(double tsec, bool force) {
        if (!static_run_active_ || force) {
            static_run_active_ = true;
            static_start_ = tsec;
        }
    }

    void finalize_common(SceneActivityDecision&, double) {}
};

} // namespace cortex