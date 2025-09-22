#pragma once
#include <vector>
#include <mutex>
#include <chrono>
#include <cstdint>
#include <algorithm>
#include <optional>

namespace cortex {

// Minimal raw frame: BGRA top-down, 32-bit
struct MirrorFrame {
    std::vector<uint8_t> bgra;
    int width{0}, height{0};
    std::chrono::steady_clock::time_point ts{};
    bool ok() const { return width > 0 && height > 0 && bgra.size() == size_t(width*height*4); }
};

class MirrorCache {
public:
    explicit MirrorCache(size_t capacity_frames = 300) : cap(std::max<size_t>(2, capacity_frames)) {
        ring.resize(cap);
    }

    void push(const uint8_t* bgra, int w, int h) {
        if (!bgra || w <= 0 || h <= 0) return;
        std::lock_guard<std::mutex> lock(m);
        MirrorFrame f;
        f.width = w; f.height = h;
        f.bgra.assign(bgra, bgra + size_t(w) * size_t(h) * 4);
        f.ts = std::chrono::steady_clock::now();
        ring[head] = std::move(f);
        head = (head + 1) % cap;
        if (size < cap) ++size;
    }

    // Get last N frames (most recent last)
    std::vector<MirrorFrame> last_n(size_t n) const {
        std::lock_guard<std::mutex> lock(m);
        n = std::min(n, size);
        std::vector<MirrorFrame> out;
        out.reserve(n);
        size_t idx = (head + cap - n) % cap;
        for (size_t i = 0; i < n; ++i) {
            const auto& f = ring[(idx + i) % cap];
            if (f.ok()) out.push_back(f);
        }
        return out;
    }

    // Clip by time window (seconds back). Returns frames in chronological order.
    std::vector<MirrorFrame> window(double seconds_back) const {
        std::lock_guard<std::mutex> lock(m);
        if (size == 0) return {};
        const auto now = std::chrono::steady_clock::now();
        const auto span = std::chrono::duration<double>(seconds_back);
        std::vector<MirrorFrame> out;
        out.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            size_t idx = (head + cap - 1 - i) % cap;
            const auto& f = ring[idx];
            if (!f.ok()) break;
            if ((now - f.ts) <= span) out.push_back(f); else break;
        }
        std::reverse(out.begin(), out.end());
        return out;
    }

private:
    size_t cap;
    mutable std::mutex m;
    std::vector<MirrorFrame> ring;
    size_t head{0};
    size_t size{0};
};

} // namespace cortex