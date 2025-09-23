#include "context_overflow_guard.hpp"

namespace cortex {

    // Define static data members declared in ContextOverflowGuard
    ContextStats ContextOverflowGuard::global_stats{};
    std::unordered_map<int, std::shared_ptr<ContextOverflowGuard>> ContextOverflowGuard::active_contexts{};
    std::unordered_map<int, std::shared_ptr<OverflowWorker>> ContextOverflowGuard::overflow_workers{};
    std::atomic<int> ContextOverflowGuard::worker_counter{0};
    std::atomic<int> ContextOverflowGuard::context_counter{0};
    std::mutex ContextOverflowGuard::stats_lock{};
    std::unique_ptr<AdaptivePerformanceTuner> ContextOverflowGuard::performance_tuner = nullptr;

    // If this constexpr is odr-used anywhere, provide an out-of-class definition (no initializer)
    constexpr size_t ContextOverflowGuard::overflow_threshold_default_mb;

} // namespace cortex