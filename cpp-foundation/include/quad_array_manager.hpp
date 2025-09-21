#pragma once
#include <vector>
#include <thread>
#include <atomic>
#include <algorithm>
#include <functional>
#include <cstddef>
#include <iostream>
#include "or_switch.hpp"

// QuadArrayManager: chunked parallel_map with OR-switch progress signaling.
class QuadArrayManager {
public:
    struct Options {
        unsigned threads = std::max(1u, std::thread::hardware_concurrency());
        size_t   min_items_for_parallel = 50'000; // Tune per workload
        size_t   chunk_size_hint = 0;            // 0 -> auto
        bool     pin_threads = false;            // placeholder; no-op for now
        std::function<void(size_t,size_t)> on_progress; // (done, total)
    };

    template<typename Container, typename MapFn>
    auto parallel_map(const Container& data, MapFn map_fn, const Options& opts = Options{}) {
        using OutT = decltype(map_fn(data[0]));
        const size_t N = data.size();
        std::vector<OutT> out;
        if (N == 0) return out;

        if (opts.threads <= 1 || N < opts.min_items_for_parallel) {
            out.reserve(N);
            for (size_t i = 0; i < N; ++i) out.push_back(map_fn(data[i]));
            if (opts.on_progress) opts.on_progress(N, N);
            return out;
        }

        const unsigned T = std::max(1u, opts.threads);
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
            or_switch.wait_consume();
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
};