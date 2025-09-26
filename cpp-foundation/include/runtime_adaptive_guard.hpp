#pragma once
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include <boost/multiprecision/cpp_dec_float.hpp>

#ifdef _WIN32
  #include <windows.h>
  #include <psapi.h>
#else
  #include <cstdio>
  #include <unistd.h>
#endif

namespace cortex {

struct GuardGlobalStats {
    std::atomic<size_t> total_contexts{0};
    std::atomic<size_t> overflow_events{0};
    std::atomic<size_t> recursive_overflow_events{0};
    std::atomic<size_t> worker_delegations{0};
    std::atomic<size_t> exceptions_handled{0};
    std::atomic<size_t> max_recursive_depth{0};
    std::atomic<double> total_exec_time{0.0};
    std::atomic<double> average_exec_time{0.0};
    std::atomic<double> aggressiveness_factor{1.0};
    std::atomic<size_t> base_term_allocation{0};
};

struct RunningStats {
    uint64_t n{0};
    double mean{0.0};
    double m2{0.0};
    void add(double x) {
        ++n;
        double d = x - mean;
        mean += d / static_cast<double>(n);
        double d2 = x - mean;
        m2 += d * d2;
    }
    double variance() const { return (n > 1) ? (m2 / double(n - 1)) : 0.0; }
    double stddev() const { double v = variance(); return v > 0 ? std::sqrt(v) : 0.0; }
    void reset() { n = 0; mean = 0.0; m2 = 0.0; }
};

class AdaptiveCvTuner {
public:
    explicit AdaptiveCvTuner(double target_cv = 0.30)
        : target_cv_(target_cv) {}
    void sample(double throughput) {
        std::lock_guard<std::mutex> lk(mu_);
        stats_.add(throughput);
        if (stats_.n > max_samples_) stats_.reset();
    }
    double aggressiveness() {
        std::lock_guard<std::mutex> lk(mu_);
        if (stats_.n < warmup_) return 1.0;
        if (stats_.mean <= 0.0) return 1.0;
        double cv = stats_.stddev() / stats_.mean;
        if (cv > target_cv_) return 0.85;
        if (cv < target_cv_ * 0.5) return 1.15;
        return 1.0;
    }
private:
    RunningStats stats_;
    std::mutex   mu_;
    double       target_cv_;
    const uint64_t warmup_ = 20;
    const uint64_t max_samples_ = 400;
};

struct AdaptiveWorker {
    int worker_id{0};
    std::atomic<size_t> assigned{0};
    std::atomic<size_t> completed{0};
};

class RuntimeAdaptiveGuard : public std::enable_shared_from_this<RuntimeAdaptiveGuard> {
public:
    struct Snapshot {
        int    id{0};
        double duration_sec{0.0};
        size_t memory_growth_bytes{0};
        double aggressiveness{1.0};
        bool   overflow{false};
        size_t recursive_depth{0};
    };

    explicit RuntimeAdaptiveGuard(
        size_t base_alloc_bytes = 1024,
        size_t overflow_threshold_mb = 100,
        bool enable_worker_delegation = true,
        bool enable_recursive_protection = true,
        int max_helper_threads = 2,
        int max_recursive_depth = 3
    );

    ~RuntimeAdaptiveGuard();

    void enter();
    void exit(const std::exception_ptr& eptr = nullptr);

    template<typename Fn, typename... Args>
    auto delegate(Fn&& fn, Args&&... args)
        -> decltype(fn(std::forward<Args>(args)...)) {
        if (!enable_worker_delegation_) return fn(std::forward<Args>(args)...);
        {
            std::lock_guard<std::mutex> lk(global_mu_);
            ++globals_.worker_delegations;
        }
        if (worker_) ++worker_->assigned;
        auto r = fn(std::forward<Args>(args)...);
        if (worker_) ++worker_->completed;
        return r;
    }

    bool check_recursive_overflow_against_self();
    Snapshot snapshot() const {
        std::lock_guard<std::mutex> lk(mu_);
        return last_;
    }
    static void print_global_statistics();
    static const GuardGlobalStats& global_stats();

private:
    // static
    static GuardGlobalStats globals_;
    static std::mutex       global_mu_;
    static std::atomic<int> next_id_;
    static std::unique_ptr<AdaptiveCvTuner> tuner_;

    // instance
    int id_{0};
    size_t base_alloc_;
    size_t overflow_threshold_bytes_;
    bool enable_worker_delegation_;
    bool enable_recursive_protection_;
    int  max_helper_threads_;
    int  max_recursive_depth_;
    std::chrono::steady_clock::time_point start_;
    size_t rss_start_{0};
    std::atomic<bool> overflow_{false};
    std::atomic<size_t> recurse_count_{0};
    std::shared_ptr<AdaptiveWorker> worker_;
    std::atomic<double> aggressiveness_{1.0};
    std::atomic<size_t> dyn_alloc_{0};

    mutable std::mutex mu_;
    Snapshot last_;

    void adapt_allocation();
    void finalize_snapshot(double dur, size_t growth);
    void on_overflow(size_t growth_bytes);
    void apply_recursive_protection(size_t growth_bytes);
    bool handle_exception(const std::exception_ptr& eptr);

    static size_t process_rss_bytes();
    RuntimeAdaptiveGuard(const RuntimeAdaptiveGuard&) = delete;
    RuntimeAdaptiveGuard& operator=(const RuntimeAdaptiveGuard&) = delete;
};

} // namespace cortex