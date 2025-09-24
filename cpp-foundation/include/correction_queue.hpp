#pragma once
#include <functional>
#include <mutex>
#include <vector>
#include <atomic>
#include <memory>
#include "screen_capture_win.hpp"

namespace cortex {

class CorrectionQueue {
public:
    using Fn = std::function<void(RawImage&)>;

    // Persistent correction (runs every frame)
    void enqueue(Fn fn) {
        if (!fn) return;
        std::lock_guard<std::mutex> lk(mu_);
        persistent_.push_back(std::move(fn));
        dirty_.store(true, std::memory_order_release);
    }

    // One-shot correction (runs on next frame then removed)
    void enqueue_oneshot(Fn fn) {
        if (!fn) return;
        std::lock_guard<std::mutex> lk(mu_);
        oneshot_.push_back(std::move(fn));
        dirty_.store(true, std::memory_order_release);
    }

    // Apply all queued corrections in-place. Returns true if any ran.
    bool apply_all(RawImage& img) {
        if (!dirty_.load(std::memory_order_acquire)) return false;

        std::vector<Fn> local_persist;
        std::vector<Fn> local_oneshot;

        {
            std::lock_guard<std::mutex> lk(mu_);
            if (persistent_.empty() && oneshot_.empty()) {
                dirty_.store(false, std::memory_order_release);
                return false;
            }
            local_persist = persistent_;              // copy
            local_oneshot.swap(oneshot_);             // drain one-shots
            if (oneshot_.empty()) {
                // if only persistent remain this is stable; keep dirty_ true
                // so we continue applying them each frame
            }
        }

        bool ran = false;
        for (auto& f : local_persist) {
            if (f) { f(img); ran = true; }
        }
        for (auto& f : local_oneshot) {
            if (f) { f(img); ran = true; }
        }

        return ran;
    }

    void clear() {
        std::lock_guard<std::mutex> lk(mu_);
        persistent_.clear();
        oneshot_.clear();
        dirty_.store(false, std::memory_order_release);
    }

    bool empty() const {
        std::lock_guard<std::mutex> lk(mu_);
        return persistent_.empty() && oneshot_.empty();
    }

private:
    mutable std::mutex mu_;
    std::vector<Fn> persistent_;
    std::vector<Fn> oneshot_;
    std::atomic<bool> dirty_{false};
};

} // namespace cortex