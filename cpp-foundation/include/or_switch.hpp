#pragma once
#include <atomic>
#include <cstdint>
#include <cassert>
#include <condition_variable>
#include <mutex>

// OrSwitch: multi-producer, single-consumer "boolean OR" aggregator.
// Producers are wait-free: fetch_or set; consumer either try_consume() or wait_consume().
class OrSwitch {
public:
    OrSwitch() : mask_(0) {}

    // Signal from producer i (0..63). Wait-free for producers.
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

    // Non-blocking: returns current bitmask and clears it.
    inline uint64_t try_consume() noexcept {
        return mask_.exchange(0, std::memory_order_acq_rel);
    }

    // Blocking: waits until any bit is set, then returns and clears all bits.
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
    std::atomic<uint64_t> mask_;
#if !defined(__cpp_lib_atomic_wait) || __cpp_lib_atomic_wait < 201907L
    std::condition_variable cv_;
    std::mutex cv_m_;
#endif
};