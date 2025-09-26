#include "runtime_adaptive_guard.hpp"

namespace cortex {

GuardGlobalStats RuntimeAdaptiveGuard::globals_{};
std::mutex       RuntimeAdaptiveGuard::global_mu_{};
std::atomic<int> RuntimeAdaptiveGuard::next_id_{0};
std::unique_ptr<AdaptiveCvTuner> RuntimeAdaptiveGuard::tuner_{};

size_t RuntimeAdaptiveGuard::process_rss_bytes() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(),
        reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof(pmc))) {
        return static_cast<size_t>(pmc.WorkingSetSize);
    }
    return 0;
#else
    FILE* f = std::fopen("/proc/self/statm", "r");
    if (!f) return 0;
    long pages=0,resident=0;
    if (std::fscanf(f, "%ld %ld", &pages, &resident) != 2) {
        std::fclose(f); return 0;
    }
    std::fclose(f);
    long ps = sysconf(_SC_PAGESIZE);
    return static_cast<size_t>(resident) * static_cast<size_t>(ps);
#endif
}

RuntimeAdaptiveGuard::RuntimeAdaptiveGuard(
    size_t base_alloc_bytes,
    size_t overflow_threshold_mb,
    bool enable_worker_delegation,
    bool enable_recursive_protection,
    int max_helper_threads,
    int max_recursive_depth
)
  : base_alloc_(base_alloc_bytes),
    overflow_threshold_bytes_(overflow_threshold_mb * 1024ull * 1024ull),
    enable_worker_delegation_(enable_worker_delegation),
    enable_recursive_protection_(enable_recursive_protection),
    max_helper_threads_(max_helper_threads),
    max_recursive_depth_(max_recursive_depth)
{
    {
        std::lock_guard<std::mutex> lk(global_mu_);
        id_ = ++next_id_;
        if (!tuner_) tuner_ = std::make_unique<AdaptiveCvTuner>(0.30);
        ++globals_.total_contexts;
    }
    dyn_alloc_.store(base_alloc_);
    worker_ = std::make_shared<AdaptiveWorker>();
    worker_->worker_id = id_;
}

RuntimeAdaptiveGuard::~RuntimeAdaptiveGuard() = default;

void RuntimeAdaptiveGuard::enter() {
    start_ = std::chrono::steady_clock::now();
    rss_start_ = process_rss_bytes();
    adapt_allocation();
}

void RuntimeAdaptiveGuard::exit(const std::exception_ptr& eptr) {
    auto end = std::chrono::steady_clock::now();
    double dur = std::chrono::duration<double>(end - start_).count();
    size_t rss_end = process_rss_bytes();
    size_t growth = (rss_end >= rss_start_) ? (rss_end - rss_start_) : 0;

    if (growth > overflow_threshold_bytes_) on_overflow(growth);
    bool handled = eptr ? handle_exception(eptr) : false;

    if (tuner_ && dur > 0) {
        double sample = static_cast<double>(base_alloc_) / dur;
        tuner_->sample(sample);
        adapt_allocation();
    }

    {
        std::lock_guard<std::mutex> lk(global_mu_);
        double new_total = globals_.total_exec_time.load() + dur;
        globals_.total_exec_time.store(new_total);
        size_t total_ctx = globals_.total_contexts.load();
        globals_.average_exec_time.store(
            total_ctx ? (new_total / double(total_ctx)) : 0.0
        );
        if (handled) ++globals_.exceptions_handled;
    }

    finalize_snapshot(dur, growth);

    std::cout << "[Guard] id=" << id_
              << " dur=" << dur << "s"
              << " growth=" << (growth / (1024.0*1024.0)) << "MB"
              << (overflow_.load() ? " (overflow)" : "")
              << " aggr=" << aggressiveness_.load()
              << "\n";
}

void RuntimeAdaptiveGuard::adapt_allocation() {
    if (!tuner_) return;
    double ag = tuner_->aggressiveness();
    aggressiveness_.store(ag);
    size_t new_alloc = static_cast<size_t>(double(base_alloc_) * ag);
    dyn_alloc_.store(new_alloc);
    std::lock_guard<std::mutex> lk(global_mu_);
    globals_.aggressiveness_factor.store(ag);
    globals_.base_term_allocation.store(new_alloc);
}

void RuntimeAdaptiveGuard::on_overflow(size_t growth_bytes) {
    overflow_.store(true);
    ++recurse_count_;
    {
        std::lock_guard<std::mutex> lk(global_mu_);
        ++globals_.overflow_events;
        size_t depth = recurse_count_.load();
        if (depth > globals_.max_recursive_depth.load())
            globals_.max_recursive_depth.store(depth);
    }
    if (enable_recursive_protection_ &&
        recurse_count_.load() <= static_cast<size_t>(max_recursive_depth_)) {
        apply_recursive_protection(growth_bytes);
    }
}

void RuntimeAdaptiveGuard::apply_recursive_protection(size_t) {
    {
        std::lock_guard<std::mutex> lk(global_mu_);
        ++globals_.recursive_overflow_events;
    }
}

bool RuntimeAdaptiveGuard::handle_exception(const std::exception_ptr& eptr) {
    try {
        std::rethrow_exception(eptr);
    } catch(const std::exception& e) {
        std::cout << "[Guard] exception: " << e.what() << "\n";
        if (overflow_.load()) return true;
    } catch(...) {
        std::cout << "[Guard] unknown exception\n";
    }
    return false;
}

bool RuntimeAdaptiveGuard::check_recursive_overflow_against_self() {
    if (!overflow_.load()) return false;
    size_t rss = process_rss_bytes();
    if (rss > overflow_threshold_bytes_ * 2) {
        apply_recursive_protection(rss - rss_start_);
        return true;
    }
    return false;
}

void RuntimeAdaptiveGuard::finalize_snapshot(double dur, size_t growth) {
    std::lock_guard<std::mutex> lk(mu_);
    last_.id = id_;
    last_.duration_sec = dur;
    last_.memory_growth_bytes = growth;
    last_.aggressiveness = aggressiveness_.load();
    last_.overflow = overflow_.load();
    last_.recursive_depth = recurse_count_.load();
}

const GuardGlobalStats& RuntimeAdaptiveGuard::global_stats() {
    return globals_;
}

void RuntimeAdaptiveGuard::print_global_statistics() {
    std::lock_guard<std::mutex> lk(global_mu_);
    std::cout << "\n[Guard] GLOBAL\n"
              << "  contexts=" << globals_.total_contexts.load()
              << " overflows=" << globals_.overflow_events.load()
              << " recursive=" << globals_.recursive_overflow_events.load()
              << " delegations=" << globals_.worker_delegations.load()
              << " exceptions=" << globals_.exceptions_handled.load()
              << " max_depth=" << globals_.max_recursive_depth.load()
              << " avg_exec=" << globals_.average_exec_time.load()
              << " aggr=" << globals_.aggressiveness_factor.load()
              << " base_alloc=" << globals_.base_term_allocation.load()
              << "\n";
}

} // namespace cortex