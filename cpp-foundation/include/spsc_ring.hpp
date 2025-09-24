#pragma once
#include <atomic>
#include <cstddef>
#include <type_traits>
#include <vector>
#include <memory>

// Single-producer single-consumer lock-free ring buffer.
// Store shared_ptr<T> to avoid copying large frames/ROIs.
namespace cortex {

    template <typename T>
    class SpscRing {
        static_assert(!std::is_reference<T>::value, "T must not be a reference");
    public:
        explicit SpscRing(size_t capacity_power_of_two = 1024)
            : cap_(round_up_pow2(capacity_power_of_two)),
              mask_(cap_ - 1),
              buf_(cap_) {}

        bool push(std::shared_ptr<T> v) {
            const size_t head = head_.load(std::memory_order_relaxed);
            const size_t next = head + 1;
            if (next - tail_.load(std::memory_order_acquire) > cap_) return false; // full (drop)
            buf_[head & mask_] = std::move(v);
            head_.store(next, std::memory_order_release);
            return true;
        }

        bool pop(std::shared_ptr<T>& out) {
            const size_t tail = tail_.load(std::memory_order_relaxed);
            if (tail == head_.load(std::memory_order_acquire)) return false; // empty
            out = std::move(buf_[tail & mask_]);
            tail_.store(tail + 1, std::memory_order_release);
            return true;
        }

        size_t capacity() const { return cap_; }

    private:
        static size_t round_up_pow2(size_t x) {
            if (x < 2) return 2;
            --x; x |= x>>1; x |= x>>2; x |= x>>4; x |= x>>8; x |= x>>16; x |= (sizeof(size_t)==8? x>>32 : 0);
            return x + 1;
        }

        const size_t cap_;
        const size_t mask_;
        std::vector<std::shared_ptr<T>> buf_;
        std::atomic<size_t> head_{0};
        std::atomic<size_t> tail_{0};
    };

} // namespace cortex