#pragma once
#include <vector>
#include <thread>
#include <atomic>
#include <algorithm>
#include <functional>
#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <mutex>
#include "or_switch.hpp"

namespace cortex {

struct TileRect {
    size_t index;   // row-major index
    size_t x0, y0;  // inclusive
    size_t x1, y1;  // exclusive
    size_t width()  const { return x1 - x0; }
    size_t height() const { return y1 - y0; }
};

namespace detail {
    inline bool set_thread_affinity(unsigned /*cpu_id*/) noexcept {
        return false; // stub to keep headers portable
    }
}

// Tiler + chunked parallel_map + OR progress signaling.
// Core reservation policy: at least 4 tile workers; keep 4 cores free (defaults).
class QuadArrayManager {
public:
    QuadArrayManager() = default;

    struct Options {
        unsigned threads = 0;                 // 0 => auto policy
        unsigned min_tile_threads = 4;        // enforce ≥ 4 tile workers
        unsigned keep_free_cores = 4;         // leave for other subsystems
        bool adaptive_threads = true;

        size_t min_items_for_parallel = 50'000;
        size_t target_items_per_thread = 25'000;
        size_t chunk_size_hint = 0;

        bool pin_threads = false;
        std::function<void(size_t,size_t)> on_progress;
    };

    template<typename Container, typename MapFn>
    auto parallel_map(const Container& data, MapFn map_fn, const Options& opts = Options{}) {
        using OutT = decltype(map_fn(data[0]));
        const size_t N = data.size();
        std::vector<OutT> out;
        if (N == 0) return out;

        const unsigned T = decide_threads(N, opts);
        if (T <= 1 || N < opts.min_items_for_parallel) {
            out.reserve(N);
            for (size_t i = 0; i < N; ++i) out.push_back(map_fn(data[i]));
            if (opts.on_progress) opts.on_progress(N, N);
            return out;
        }

        const size_t chunk = opts.chunk_size_hint ? opts.chunk_size_hint
                              : std::max<size_t>(1, (N + T - 1) / T);
        const size_t chunks = (N + chunk - 1) / chunk;

        struct Slot {
            std::atomic<uint8_t> ready{0};
            size_t start=0, end=0;
            std::vector<OutT> partial;
        };
        std::vector<Slot> slots(chunks);

        OrSwitch or_switch;
        std::atomic<size_t> produced{0};

        auto worker = [&](unsigned wid) {
            if (opts.pin_threads && wid < opts.min_tile_threads) {
                detail::set_thread_affinity(wid);
            }
            for (size_t c = wid; c < chunks; c += T) {
                const size_t s = c * chunk, e = std::min(N, s + chunk);
                std::vector<OutT> local;
                local.reserve(e - s);
                for (size_t i = s; i < e; ++i) local.push_back(map_fn(data[i]));
                auto& slot = slots[c];
                slot.start = s; slot.end = e; slot.partial = std::move(local);
                slot.ready.store(1, std::memory_order_release);
                produced.fetch_add(e - s, std::memory_order_relaxed);
                or_switch.signal(wid % 64);
            }
        };

        std::vector<std::thread> pool;
        pool.reserve(T);
        for (unsigned i = 0; i < T; ++i) pool.emplace_back(worker, i);

        out.resize(N);
        size_t next_emit = 0;
        while (next_emit < N) {
            (void)or_switch.wait_consume();
            const size_t first_slot = next_emit / chunk;
            for (size_t c = first_slot; c < chunks; ++c) {
                if (slots[c].ready.load(std::memory_order_acquire) == 0) break;
                const auto& slot = slots[c];
                const size_t count = slot.end - slot.start;
                if (count == 0) continue;
                std::copy(slot.partial.begin(), slot.partial.end(), out.begin() + slot.start);
                next_emit = slot.end;
            }
            if (opts.on_progress) opts.on_progress(next_emit, N);
        }

        for (auto& t : pool) t.join();
        if (opts.on_progress) opts.on_progress(N, N);
        return out;
    }

    // Image tiler
    void configure(size_t frame_w, size_t frame_h, size_t tile_w = 32, size_t tile_h = 32) {
        fw_ = frame_w; fh_ = frame_h; tw_ = std::max<size_t>(1, tile_w); th_ = std::max<size_t>(1, tile_h);
        build_tiles();
    }

    const std::vector<TileRect>& tiles() const { return tiles_; }
    size_t tiles_x() const { return tx_; }
    size_t tiles_y() const { return ty_; }

    size_t tile_index_from_xy(size_t x, size_t y) const {
        if (x >= fw_ || y >= fh_) return size_t(-1);
        size_t ix = x / tw_;
        size_t iy = y / th_;
        return iy * tx_ + ix;
    }

    static unsigned decide_threads(size_t N, const Options& opts) {
        const unsigned hw = std::max(1u, std::thread::hardware_concurrency());

        unsigned base = hw;
        if (opts.keep_free_cores > 0 && hw > opts.keep_free_cores) base = hw - opts.keep_free_cores;
        else if (opts.keep_free_cores >= hw) base = 1;

        unsigned T = (opts.threads > 0) ? opts.threads : base;
        if (T > 1) T = std::max(T, std::max(1u, opts.min_tile_threads));
        T = std::min(T, hw);

        if (opts.adaptive_threads && N >= opts.min_items_for_parallel && opts.target_items_per_thread > 0) {
            const unsigned by_size = static_cast<unsigned>((N + opts.target_items_per_thread - 1) / opts.target_items_per_thread);
            T = std::min(std::max(std::max(1u, opts.min_tile_threads), by_size), base);
            T = std::clamp<unsigned>(T, 1, hw);
        }
        return std::max(1u, T);
    }

private:
    size_t fw_{0}, fh_{0};
    size_t tw_{32}, th_{32};
    size_t tx_{0}, ty_{0};
    std::vector<TileRect> tiles_;

    void build_tiles() {
        tiles_.clear();
        if (!fw_ || !fh_) { tx_ = ty_ = 0; return; }

        tx_ = (fw_ + tw_ - 1) / tw_;
        ty_ = (fh_ + th_ - 1) / th_;

        tiles_.reserve(tx_ * ty_);
        size_t idx = 0;
        for (size_t ty = 0; ty < this->ty_; ++ty) {
            for (size_t tx = 0; tx < this->tx_; ++tx) {
                size_t x0 = tx * tw_;
                size_t y0 = ty * th_;
                size_t x1 = std::min(fw_, x0 + tw_);
                size_t y1 = std::min(fh_, y0 + th_);
                tiles_.push_back(TileRect{ idx++, x0, y0, x1, y1 });
            }
        }
    }
};

} // namespace cortex