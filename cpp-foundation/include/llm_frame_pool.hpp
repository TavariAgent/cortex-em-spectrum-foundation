#pragma once
#include <deque>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <cstdlib>

#include "screen_capture_win.hpp" // RawImage
#include "frame_recorder.hpp"     // write_bmp32, RawImageBMPView, make_numbered
#include "spsc_ring.hpp"          // SpscRing
#include "image_ops.hpp"          // optional resize
#include "operand_map.hpp"        // cortex::sig

namespace cortex {
namespace fs = std::filesystem;

class LLMFramePool {
public:
    struct Frame {
        int64_t index{0};
        double  tsec{0.0};      // start time of this unique image
        double  t_end{0.0};     // last observed time for this image (>= tsec)
        uint64_t run_len{1};    // count of coalesced identical frames (stats)
        std::shared_ptr<RawImage> img;  // BGRA8
        sig::OperandMap sig;             // fingerprint for dedupe
    };

    explicit LLMFramePool(double retention_seconds = 300.0,
                          size_t  budget_mb        = 1024,
                          int     fps_hint         = 30,
                          size_t  quick_lane_capacity = 2048)
    : dynamic_retention_sec_(retention_seconds),
      budget_bytes_(budget_mb * 1024ull * 1024ull),
      fps_hint_(std::max(1, fps_hint)),
      quick_lane_(quick_lane_capacity) {}

    // Back-compat alias for “dynamic scrub window”
    void set_retention(double seconds) { set_dynamic_retention_sec(seconds); }
    void set_dynamic_retention_sec(double seconds) { dynamic_retention_sec_.store(std::max(0.0, seconds)); }
    void set_budget_mb(size_t mb) { budget_bytes_.store(mb * 1024ull * 1024ull); }

    // While frames are identical, keep exactly one in RAM.
    // grace_seconds avoids flapping; you set 1.0s which is good.
    void set_single_static_mode(bool enabled, double grace_seconds = 1.0) {
        single_static_mode_.store(enabled);
        static_grace_sec_.store(std::max(0.0, grace_seconds));
    }

    // Push a frame. Identicals coalesce, changes are appended and sent to quick-lane.
    // Returns the same shared_ptr the caller provided (not moved).
    std::shared_ptr<RawImage> push(const std::shared_ptr<RawImage>& img, int64_t index, double tsec) {
        if (!img || !img->ok()) return img;

        sig::OperandMap cur_sig = sig::compute_operand_map(*img);

        std::lock_guard<std::mutex> lk(mu_);
        latest_ts_ = tsec;

        if (!frames_.empty()) {
            Frame& last = frames_.back();

            if (last.img && last.img->ok() && sig::frames_identical(*img, *last.img, cur_sig, last.sig)) {
                // Extend static run
                last.t_end = tsec;
                last.run_len++;

                // Collapse to one frame after grace
                if (single_static_mode_.load()) {
                    if (!in_static_run_) {
                        in_static_run_ = true;
                        static_since_ts_ = tsec;
                    }
                    if ((tsec - static_since_ts_) >= static_grace_sec_.load()) {
                        while (frames_.size() > 1) {
                            total_bytes_ -= frames_.front().img ? frames_.front().img->bgra.size() : 0;
                            frames_.pop_front();
                        }
                        total_bytes_ = frames_.empty() ? 0 : frames_.back().img->bgra.size();
                    }
                }

                // No quick-lane push for static frames
                evict_unlocked_keep_one();
                return img;
            }

            // Change detected; exit static run
            in_static_run_ = false;
            static_since_ts_ = 0.0;
        }

        // Append unique (changed) frame
        Frame f;
        f.index   = index;
        f.tsec    = tsec;
        f.t_end   = tsec;  // until next change
        f.run_len = 1;
        f.img     = img;   // copy shared_ptr
        f.sig     = cur_sig;

        total_bytes_ += f.img->bgra.size();
        frames_.push_back(std::move(f));

        // Quick-lane push for changed frames only
        auto ref = std::make_shared<Frame>(frames_.back());
        quick_lane_.push(ref);

        evict_unlocked_keep_one();
        return img;
    }

    bool pop_quick(std::shared_ptr<Frame>& out) { return quick_lane_.pop(out); }

    // Coalesced snapshot of last N seconds. Always returns at least one frame if pool non-empty.
    std::vector<Frame> snapshot_recent(double last_seconds) {
        std::vector<Frame> out;
        std::lock_guard<std::mutex> lk(mu_);
        if (frames_.empty()) return out;

        const double cutoff = latest_ts_ - std::max(0.0, last_seconds);
        for (auto it = frames_.rbegin(); it != frames_.rend(); ++it) {
            if (it->tsec >= cutoff) out.push_back(*it);
            else break;
        }
        if (out.empty()) out.push_back(frames_.back());
        std::reverse(out.begin(), out.end());
        return out;
    }

    // Expand coalesced runs to repeated frames for correct timing on export.
    bool export_recent_to_video(double last_seconds, const fs::path& out_mp4, int fps) {
        auto clip = snapshot_recent(last_seconds);
        if (clip.empty()) {
            std::cout << "LLMFramePool: No frames to export.\n";
            return false;
        }

        const fs::path temp_dir = ensure_dir(fs::path("captures") / timestamp_now());
        const std::string prefix = "cap";

        size_t written = 0;
        for (size_t i = 0; i < clip.size(); ++i) {
            const Frame& cur = clip[i];
            const Frame* nxt = (i + 1 < clip.size()) ? &clip[i + 1] : nullptr;

            const size_t repeats = expand_repeats(cur, nxt, std::max(1, fps));
            if (!cur.img || !cur.img->ok()) continue;

            for (size_t r = 0; r < repeats; ++r) {
                std::string path = make_numbered((temp_dir / prefix).string(),
                                                 static_cast<int>(written), ".bmp", 6);
                RawImageBMPView view{ cur.img->bgra.data(), cur.img->width, cur.img->height };
                write_bmp32(path, view);
                ++written;
            }
        }

        if (!written) {
            std::cout << "LLMFramePool: No valid frames written; abort export.\n";
            return false;
        }

        const fs::path pattern = temp_dir / (prefix + "_%06d.bmp");
        std::ostringstream oss;
        oss << "ffmpeg -y -hide_banner -loglevel error"
            << " -framerate " << std::max(1, fps)
            << " -i \"" << pattern.string() << "\""
            << " -pix_fmt yuv420p -vf \"scale=trunc(iw/2)*2:trunc(ih/2)*2\""
            << " \"" << out_mp4.string() << "\"";
        std::cout << "Running: " << oss.str() << "\n";
        int rc = std::system(oss.str().c_str());
        if (rc != 0) {
            std::cout << "ffmpeg failed (" << rc << "). Leaving BMPs at " << temp_dir.string() << "\n";
            return false;
        }

        cleanup_dir_bmps(temp_dir, prefix);
        std::error_code rmec;
        fs::remove(temp_dir, rmec); // if empty
        return true;
    }

private:
    std::mutex mu_;
    std::deque<Frame> frames_;
    size_t total_bytes_{0};
    double latest_ts_{0.0};

    // “Scrub window” for changed frames (seconds). Older changed frames roll off.
    std::atomic<double>  dynamic_retention_sec_{300.0};
    std::atomic<uint64_t> budget_bytes_{1024ull * 1024ull * 1024ull};
    int fps_hint_{30};

    // Static run control
    std::atomic<bool>   single_static_mode_{true};
    std::atomic<double> static_grace_sec_{1.0};
    bool   in_static_run_{false};
    double static_since_ts_{0.0};

    SpscRing<Frame> quick_lane_;

    static size_t expand_repeats(const Frame& cur, const Frame* nxt, int fps) {
        const double end = (cur.t_end > cur.tsec) ? cur.t_end : (nxt ? nxt->tsec : cur.tsec);
        const double span = std::max(0.0, end - cur.tsec);
        const int n = static_cast<int>(std::round(span * fps));
        return static_cast<size_t>(std::max(1, n));
    }

    static fs::path ensure_dir(const fs::path& p) {
        std::error_code ec; fs::create_directories(p, ec); return p;
    }
    static void cleanup_dir_bmps(const fs::path& d, const std::string& prefix) {
        std::error_code ec;
        for (fs::directory_iterator it(d, ec), end; !ec && it != end; it.increment(ec)) {
            const auto& ent = *it;
            if (!ent.is_regular_file()) continue;
            const auto name = ent.path().filename().string();
            if (name.rfind(prefix + "_", 0) == 0 && name.size() > 4 && name.substr(name.size()-4)==".bmp") {
                std::error_code rec; fs::remove(ent.path(), rec);
            }
        }
    }
    static std::string timestamp_now() {
        auto t = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(t);
        std::tm tm{};
    #ifdef _WIN32
        localtime_s(&tm, &tt);
    #else
        localtime_r(&tt, &tm);
    #endif
        std::ostringstream oss; oss << std::put_time(&tm, "%Y%m%d_%H%M%S"); return oss.str();
    }

    // Evict by dynamic scrub window (uses frame.t_end) and memory budget.
    // Always keep at least one frame (latest).
    void evict_unlocked_keep_one() {
        if (frames_.empty()) return;

        // Time-based eviction: keep only frames whose coverage ends within the scrub window
        const double keep_sec = std::max(0.0, dynamic_retention_sec_.load());
        const double cutoff_end = latest_ts_ - keep_sec;

        while (frames_.size() > 1 && frames_.front().t_end < cutoff_end) {
            total_bytes_ -= frames_.front().img ? frames_.front().img->bgra.size() : 0;
            frames_.pop_front();
        }

        // Memory budget eviction (keep at least one)
        const uint64_t budget = budget_bytes_.load();
        while (frames_.size() > 1 && total_bytes_ > budget) {
            total_bytes_ -= frames_.front().img ? frames_.front().img->bgra.size() : 0;
            frames_.pop_front();
        }
    }
};

} // namespace cortex