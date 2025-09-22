#pragma once
#include <cstddef>
#include <chrono>
#include <atomic>
#include <cstdint>
#include <cassert>
#include <condition_variable>
#include <mutex>
#include <vector>

namespace cortex {

// Route decision for a tile
enum class Route {
    CPU,     // process on CPU (default)
    GPU,     // hand off this tile to GPU (future)
    Skip     // unchanged enough to reuse cached output (optional)
};

struct RouterConfig {
    // Detection thresholds
    double epsilon = 1e-30;     // per-pixel amplitude change threshold
    double k_percent = 5.0;     // tile change percentage (K), default 5%

    // Calibration requirements
    int   calibration_frames_required = 5;  // build operand map over 5 frames
    double calibration_min_seconds = 1.0;   // require ≥1s elapsed
    bool allow_skip_route = true;           // allow Route::Skip when perfectly static
};

// OrSwitch: combines two roles
// 1) Router: per-tile route decision (CPU/GPU/Skip) with calibration gating.
// 2) MPSC "boolean OR" aggregator: multiple producers set bits; one consumer drains.
class OrSwitch {
public:
    OrSwitch() : OrSwitch(RouterConfig{}) {}

    explicit OrSwitch(const RouterConfig& cfg)
        : cfg_(cfg), mask_(0) {
        reset_calibration();
    }

    // Call once per new frame
    inline void begin_frame() {
        if (!calibrated_) {
            frames_seen_++;
            if (frames_seen_ >= static_cast<size_t>(cfg_.calibration_frames_required)) {
                const double elapsed = seconds_since_start();
                if (elapsed >= cfg_.calibration_min_seconds) {
                    calibrated_ = true;
                }
            }
        }
    }

    void reset_calibration() {
        frames_seen_ = 0;
        started_ = std::chrono::steady_clock::now();
        calibrated_ = false;
    }

    bool is_calibrated() const { return calibrated_; }

    // Not thread-safe; call from a single thread or guard externally
    void update_tile_change(size_t tile_index, double percent_changed) {
        ensure_capacity(tile_index + 1);
        last_change_[tile_index] = percent_changed;
    }

    Route decide(size_t tile_index) const {
        if (tile_index >= last_change_.size()) return Route::CPU;

        const double k = last_change_[tile_index];
        if (k <= cfg_.k_percent) {
            if (cfg_.allow_skip_route && k == 0.0 && calibrated_) {
                return Route::Skip;
            }
            return Route::CPU;
        }
        return Route::GPU;
    }

    double seconds_since_start() const {
        using clock = std::chrono::steady_clock;
        return std::chrono::duration<double>(clock::now() - started_).count();
    }

    // ===== MPSC OR-aggregator =====
    inline void signal(unsigned idx) noexcept {
        assert(idx < 64);
        const uint64_t bit = (uint64_t{1} << idx);
        const uint64_t prev = mask_.fetch_or(bit, std::memory_order_release);
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
        if (prev == 0) mask_.notify_one();
#else
        if (prev == 0) {
            std::lock_guard<std::mutex> lk(cv_m_);
            cv_.notify_one();
        }
#endif
    }

    inline uint64_t try_consume() noexcept {
        return mask_.exchange(0, std::memory_order_acq_rel);
    }

    inline uint64_t wait_consume() noexcept {
#if defined(__cpp_lib_atomic_wait) && __cpp_lib_atomic_wait >= 201907L
        uint64_t m = mask_.load(std::memory_order_acquire);
        while (m == 0) {
            mask_.wait(0, std::memory_order_relaxed);
            m = mask_.load(std::memory_order_acquire);
        }
        return mask_.exchange(0, std::memory_order_acq_rel);
#else
        std::unique_lock<std::mutex> lk(cv_m_);
        cv_.wait(lk, [&]{ return mask_.load(std::memory_order_acquire) != 0; });
        (void)lk;
        return mask_.exchange(0, std::memory_order_acq_rel);
#endif
    }

    inline uint64_t peek() const noexcept {
        return mask_.load(std::memory_order_acquire);
    }

private:
    RouterConfig cfg_;
    std::vector<double> last_change_;
    size_t frames_seen_{0};
    std::chrono::steady_clock::time_point started_;
    bool calibrated_{false};

    std::atomic<uint64_t> mask_;
#if !defined(__cpp_lib_atomic_wait) || __cpp_lib_atomic_wait < 201907L
    std::condition_variable cv_;
    std::mutex cv_m_;
#endif

    void ensure_capacity(size_t n) {
        if (last_change_.size() < n) last_change_.resize(n, 100.0);
    }
};

} // namespace cortex