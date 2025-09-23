#pragma once
#include <any>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <future>
#include <exception>
#include <iostream>
#include <algorithm>
#include <numeric>          // ADDED: for std::accumulate
#include <random>
#include <boost/multiprecision/cpp_dec_float.hpp>

namespace cortex {

// 📊 CONTEXT STATISTICS (PYTHON DATACLASS → C++)
struct ContextStats {
    std::atomic<size_t> total_contexts{0};
    std::atomic<size_t> overflow_events{0};
    std::atomic<size_t> worker_delegations{0};
    std::atomic<double> total_execution_time{0.0};
    std::atomic<double> average_execution_time{0.0};
    std::atomic<size_t> memory_before_bytes{0};
    std::atomic<size_t> memory_after_bytes{0};
    std::atomic<size_t> garbage_collections{0};
    std::atomic<size_t> exceptions_handled{0};
    std::atomic<size_t> child_processes_created{0};
    std::atomic<size_t> recursive_overflow_events{0};
    std::atomic<size_t> self_capture_events{0};
    std::atomic<size_t> helper_threads_created{0};
    std::atomic<size_t> max_recursive_depth{0};

    // 🎯 DYNAMIC PERFORMANCE TUNING PARAMETERS (FUTURE ADJUSTMENTS!)
    std::atomic<double> aggressiveness_factor{1.0};
    std::atomic<size_t> allocated_term_base_size{1024};
    std::atomic<size_t> allocated_term_max_size{1048576};
    std::atomic<double> performance_consistency_target{0.85};
};

// 🚀 OVERFLOW WORKER (PYTHON DATACLASS → C++)
struct OverflowWorker {
    int worker_id;
    int process_id;
    size_t allocated_memory_bytes;
    std::chrono::steady_clock::time_point creation_time;
    std::atomic<size_t> assigned_tasks{0};
    std::atomic<size_t> completed_tasks{0};
    std::atomic<bool> is_active{true};
    std::vector<std::thread::id> helper_threads;
    std::atomic<size_t> recursive_overflow_count{0};
    std::shared_ptr<class ContextOverflowGuard> self_capturing_context;

    std::atomic<double> term_size_multiplier{1.0};
    std::atomic<size_t> dynamic_allocation_adjustment{0};

    OverflowWorker(int id, int pid, size_t allocated_memory)
        : worker_id(id), process_id(pid), allocated_memory_bytes(allocated_memory),
          creation_time(std::chrono::steady_clock::now()) {}

    void adjust_allocation_for_consistency(double performance_ratio) {
        if (performance_ratio < 0.8) {
            term_size_multiplier.store(term_size_multiplier.load() * 1.2);
            dynamic_allocation_adjustment.store(allocated_memory_bytes / 4);
        } else if (performance_ratio > 1.2) {
            term_size_multiplier.store(std::max(0.5, term_size_multiplier.load() * 0.9));
        }
    }
};

    // 🎯 ADAPTIVE PERFORMANCE TUNER
    class AdaptivePerformanceTuner {
    private:
        std::vector<double> performance_history;
        std::mutex history_lock;
        double target_consistency;

    public:
        explicit AdaptivePerformanceTuner(double target = 0.85)
            : target_consistency(target) {}

        void record_performance(double execution_time, size_t terms_processed) {
            std::lock_guard<std::mutex> lock(history_lock);
            const double throughput = terms_processed / execution_time;
            performance_history.push_back(throughput);
            if (performance_history.size() > 100) {
                performance_history.erase(performance_history.begin());
            }
        }

        double calculate_aggressiveness_adjustment() {
            std::lock_guard<std::mutex> lock(history_lock);
            if (performance_history.size() < 10) return 1.0;

            const double mean =
                std::accumulate(performance_history.begin(), performance_history.end(), 0.0)
                / static_cast<double>(performance_history.size());

            if (mean == 0.0) return 1.0;

            double variance = 0.0;
            for (double perf : performance_history) {
                const double d = perf - mean;
                variance += d * d;
            }
            variance /= static_cast<double>(performance_history.size());

            const double consistency = 1.0 / (1.0 + variance / (mean * mean));
            if (consistency < target_consistency) return 0.8;
            if (consistency > target_consistency + 0.1) return 1.3;
            return 1.0;
        }

        // Keep the 2-arg signature used by call sites
        size_t suggest_term_allocation_size(size_t base_size, double /*current_throughput*/) {
            double adj = calculate_aggressiveness_adjustment();
            return static_cast<size_t>(static_cast<double>(base_size) * adj);
        }
    };

};

class ContextOverflowGuard {
private:
    // 🌌 STATIC CLASS-LEVEL TRACKING (PYTHON CLASS VARIABLES → C++)
    static cortex::ContextStats global_stats;
    static std::unordered_map<int, std::shared_ptr<ContextOverflowGuard>> active_contexts;
    static std::unordered_map<int, std::shared_ptr<cortex::OverflowWorker>> overflow_workers;
    static std::atomic<int> worker_counter;
    static std::atomic<int> context_counter;
    static std::mutex stats_lock;
    static constexpr size_t overflow_threshold_default_mb = 100;
    static std::unique_ptr<cortex::AdaptivePerformanceTuner> performance_tuner;

    // 🎭 INSTANCE VARIABLES (PYTHON INSTANCE VARS → C++)
    int context_id;
    size_t base_byte_allocation;
    size_t overflow_threshold_bytes;
    bool enable_worker_delegation;
    bool enable_recursive_protection;
    int max_helper_threads;
    int max_recursive_depth;
    std::chrono::steady_clock::time_point start_time;
    size_t initial_memory;
    std::vector<int> child_processes;
    std::vector<int> delegated_workers;
    std::unordered_map<std::string, std::any> context_data;
    std::atomic<bool> overflow_detected{false};
    std::atomic<size_t> recursive_overflow_count{0};

    // 🤯 REVOLUTIONARY: SELF-CAPTURING CONTEXT (PYTHON → C++)
    std::shared_ptr<ContextOverflowGuard> self_capturing_context;
    std::vector<std::thread> helper_thread_pool;

    // 🎯 BOOLEAN-BASED THREAD STATE MANAGEMENT (NON-BLOCKING!)
    std::atomic<bool> helpers_ready{false};
    std::atomic<bool> helpers_requested{false};
    std::atomic<bool> allocation_shift_ready{false};
    std::atomic<bool> thread_flow_active{true};
    std::atomic<bool> overflow_flag_triggered{false};

    // 🎭 BOOLEAN STATE FLAGS FOR ELEGANT FLOW CONTROL
    struct StateFlags {
        std::atomic<bool> helpers_available{false};
        std::atomic<bool> memory_expanded{false};
        std::atomic<bool> recursive_active{false};
        std::atomic<bool> allocation_doubled{false};
        std::atomic<bool> flow_uninterrupted{true};
    } state_flags;

    // 🎯 DYNAMIC PERFORMANCE TRACKING
    struct PerformanceMetrics {
        static constexpr size_t KB = 1024;
        static constexpr size_t MB = 1024 * KB;

        static double formatBytesMB(const size_t bytes) {
            return static_cast<double>(bytes) / static_cast<double>(MB);
        }

        // Static helper that takes the instance to read from
        static size_t computeReassignmentAllocation(const PerformanceMetrics& pm, const size_t depth) {
            const double aggr = pm.aggressiveness_level.load();
            const size_t base = pm.current_term_allocation.load();
            const size_t scale = (1ULL << depth);
            return static_cast<size_t>(static_cast<double>(base) * static_cast<double>(scale) * aggr);
        }

        std::atomic<double> current_throughput{0.0};
        std::atomic<double> aggressiveness_level{1.0};
        std::chrono::steady_clock::time_point last_adjustment_time{};
        std::atomic<size_t> current_term_allocation{0};
    }; PerformanceMetrics performance_metrics;

public:
    explicit ContextOverflowGuard(
        const size_t base_allocation = 1024,
        const size_t overflow_threshold_mb = 100,
        const bool enable_worker_delegation = true,
        const bool enable_recursive_protection = true,
        const int max_helper_threads = 2,
        const int max_recursive_depth = 3
    ) : base_byte_allocation(base_allocation),
        overflow_threshold_bytes(overflow_threshold_mb * 1024 * 1024),
        enable_worker_delegation(enable_worker_delegation),
        enable_recursive_protection(enable_recursive_protection),
        max_helper_threads(max_helper_threads),
        max_recursive_depth(max_recursive_depth),
        initial_memory(0) {

        // 🎭 REGISTER CONTEXT (PYTHON → C++)
        {
            std::lock_guard<std::mutex> lock(stats_lock);
            context_id = ++context_counter;
            active_contexts[context_id] = std::shared_ptr<ContextOverflowGuard>(this, [](ContextOverflowGuard*){});

            // Initialize performance tuner if not exists
            if (!performance_tuner) {
                performance_tuner = std::make_unique<cortex::AdaptivePerformanceTuner>(0.85);
            }
        }

        // 🎯 INITIALIZE DYNAMIC PERFORMANCE TRACKING
        performance_metrics.current_term_allocation = base_allocation;
        performance_metrics.last_adjustment_time = std::chrono::steady_clock::now();

        std::cout << "🎭 Context " << context_id << ": Entering optimization context\n";
        std::cout << "   Base allocation: " << base_allocation << " bytes\n";
        std::cout << "   Dynamic tuning: ENABLED\n";
    }

    ~ContextOverflowGuard() {
        cleanup_context();

        std::lock_guard<std::mutex> lock(stats_lock);
        active_contexts.erase(context_id);
    }

    // 🚀 CONTEXT MANAGER FUNCTIONALITY
    void enter() {
        std::cout << "🎭 Context " << context_id << ": Entering optimization context\n";

        start_time = std::chrono::steady_clock::now();
        initial_memory = get_current_memory_usage();

        // Update global statistics
        {
            std::lock_guard<std::mutex> lock(stats_lock);
            ++global_stats.total_contexts;
            global_stats.memory_before_bytes += initial_memory;
        }

        // 🎯 DYNAMIC ALLOCATION ADJUSTMENT BASED ON SYSTEM STATE
        adjust_allocation_for_system_consistency();
    }


    void exit(const std::exception_ptr &exc_ptr = nullptr) {
        const auto duration = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start_time
        ).count();

        const size_t final_memory = get_current_memory_usage();
        const size_t memory_growth = final_memory - initial_memory;

        // 🎯 RECORD PERFORMANCE FOR DYNAMIC TUNING
        if (performance_tuner) {
            const size_t terms_processed = delegated_workers.size() * 1000;  // Estimate
            performance_tuner->record_performance(duration, terms_processed);
            performance_metrics.current_throughput = terms_processed / duration;
        }

        // 🤯 DETECT OVERFLOW CONDITION (YOUR PYTHON LOGIC → C++)
        if (memory_growth > overflow_threshold_bytes) {
            handle_context_overflow(memory_growth);
        }

        // Handle exceptions
        bool exception_handled = false;
        if (exc_ptr) {
            exception_handled = handle_context_exception(exc_ptr);
        }

        // Update global statistics with dynamic adjustments
        {
            std::lock_guard<std::mutex> lock(stats_lock);
            auto& stats = global_stats;
            stats.total_execution_time = + duration;
            stats.average_execution_time = stats.total_execution_time.load() / stats.total_contexts.load();
            stats.memory_after_bytes += final_memory;

            if (exception_handled) {
                ++stats.exceptions_handled;
            }

            // 🎯 UPDATE DYNAMIC PERFORMANCE PARAMETERS
            if (performance_tuner) {
                const double aggressiveness = performance_tuner->calculate_aggressiveness_adjustment();
                stats.aggressiveness_factor = aggressiveness;
                stats.allocated_term_base_size = performance_tuner->suggest_term_allocation_size(
                    base_byte_allocation, performance_metrics.current_throughput.load()
                );
            }
        }

        cleanup_context();
        report_context_completion(duration, memory_growth);

        std::cout << "🏴‍☠️ Context " << context_id << " complete: " << duration << "s\n";
    }

    // 🎯 DYNAMIC ALLOCATION ADJUSTMENT FOR SYSTEM CONSISTENCY
    void adjust_allocation_for_system_consistency() {
        if (!performance_tuner) return;

        const auto now = std::chrono::steady_clock::now();
        const auto time_since_adjustment = std::chrono::duration<double>(
            now - performance_metrics.last_adjustment_time
        ).count();

        // Adjust every 5 seconds
        if (time_since_adjustment > 5.0) {
            const double aggressiveness = performance_tuner->calculate_aggressiveness_adjustment();

            // 🎯 ADJUST TERM SIZES FOR CONSISTENCY (unified signature)
            const size_t new_allocation = performance_tuner->suggest_term_allocation_size(
                 base_byte_allocation, performance_metrics.current_throughput.load()
             );

            if (new_allocation != performance_metrics.current_term_allocation.load()) {
                performance_metrics.current_term_allocation = new_allocation;
                performance_metrics.aggressiveness_level = aggressiveness;
                performance_metrics.last_adjustment_time = now;

                std::cout << "🎯 Context " << context_id << ": Dynamic allocation adjusted\n";
                std::cout << "   New allocation: " << new_allocation << " bytes\n";
                std::cout << "   Aggressiveness: " << aggressiveness << "\n";

                // Adjust existing workers
                for (int worker_id : delegated_workers) {
                    if (auto it = overflow_workers.find(worker_id); it != overflow_workers.end()) {
                        const double performance_ratio = performance_metrics.current_throughput.load() / 1000.0;
                        it->second->adjust_allocation_for_consistency(performance_ratio);
                    }
                }
            }
        }
    }

    // 🚀 DELEGATE TO OVERFLOW WORKER (PYTHON → C++)
    template<typename Operation, typename... Args>
    auto delegate_to_overflow_worker(Operation&& operation, Args&&... args)
        -> decltype(operation(args...)) {

        if (!enable_worker_delegation) {
            return operation(args...);
        }

        // 🎯 USE DYNAMIC ALLOCATION SIZE
        const size_t dynamic_allocation = performance_metrics.current_term_allocation.load() * 2;
        const auto worker = create_overflow_worker(dynamic_allocation);

        std::cout << "🚀 Context " << context_id << ": Delegating to worker "
                  << worker->worker_id << " with " << dynamic_allocation << " byte allocation\n";

        try {
            ++worker->assigned_tasks;

            // Execute operation with dynamic sizing
            auto result = execute_in_worker(std::forward<Operation>(operation),
                                          std::forward<Args>(args)...);

            ++worker->completed_tasks;

            std::cout << "✅ Context " << context_id << ": Worker "
                      << worker->worker_id << " completed task\n";
            return result;

        } catch (const std::exception& e) {
            std::cout << "❌ Context " << context_id << ": Worker "
                      << worker->worker_id << " failed: " << e.what() << "\n";
            throw;
        }
    }

    // 🤯 APPLY RECURSIVE OVERFLOW PROTECTION WITH SELF-CAPTURING CONTEXT
    void apply_recursive_overflow_protection(size_t memory_growth) {
        std::cout << "🤯 Context " << context_id << ": APPLYING RECURSIVE OVERFLOW PROTECTION!\n";
        std::cout << "   Recursive depth: " << recursive_overflow_count.load()
                  << "/" << max_helper_threads << "\n";

        // 🎭 STEP 1: CREATE SELF-CAPTURING CONTEXT TO MONITOR OURSELVES
        if (!self_capturing_context) {
            self_capturing_context = create_self_capturing_context();
            {
                std::lock_guard<std::mutex> lock(stats_lock);
                global_stats.self_capture_events++;
            }
            std::cout << "🎭 Context " << context_id << ": Self-capturing context "
                      << self_capturing_context->context_id << " created!\n";
        }

        // 🎯 STEP 2: DYNAMIC ALLOCATION SCALING
        size_t recursive_count = recursive_overflow_count.load();
        double aggressiveness = performance_metrics.aggressiveness_level.load();
        size_t base_dynamic_allocation = performance_metrics.current_term_allocation.load();

        size_t scaled_allocation = static_cast<size_t>(
            base_dynamic_allocation * (4 << recursive_count) * aggressiveness
        );

        std::cout << "💾 Context " << context_id << ": Dynamic allocation scaling\n";
        std::cout << "   Base allocation: " << base_dynamic_allocation << " bytes\n";
        std::cout << "   Aggressiveness factor: " << aggressiveness << "\n";
        std::cout << "   Scaled allocation: " << scaled_allocation << " bytes\n";

        // 🧵 STEP 3: BOOLEAN-BASED HELPER THREAD ALLOCATION
        int helper_threads_needed = std::min(static_cast<int>(recursive_count), max_helper_threads);

        bool should_create_helpers = (
            helper_threads_needed > static_cast<int>(helper_thread_pool.size())
            && thread_flow_active.load()
            && !helpers_requested.load()
        );

        if (should_create_helpers || can_shift_allocation_immediately()) {
            helpers_requested = true;
            allocation_shift_ready = true;

            std::cout << "🎯 Context " << context_id << ": Boolean-triggered helper allocation\n";

            // FIX: avoid signed/unsigned subtraction mismatch
            const size_t deficit = (helper_threads_needed > static_cast<int>(helper_thread_pool.size()))
                ? static_cast<size_t>(helper_threads_needed) - helper_thread_pool.size()
                : 0;
            if (deficit > 0) {
                create_helpers_with_boolean_flow(static_cast<int>(deficit));
            }

            state_flags.helpers_available = true;
            helpers_ready = state_flags.helpers_available.load() || allocation_shift_ready.load();
        }

        thread_flow_active = thread_flow_active.load() || helpers_ready.load() || true;

        // 🚀 STEP 4: UPDATE WORKERS ...
        for (int worker_id : delegated_workers) {
            auto it = overflow_workers.find(worker_id);
            if (it != overflow_workers.end()) {
                auto& worker = it->second;
                worker->recursive_overflow_count = recursive_count;
                worker->self_capturing_context = self_capturing_context;
                worker->allocated_memory_bytes = scaled_allocation;

                double performance_ratio = performance_metrics.current_throughput.load() / 1000.0;
                worker->adjust_allocation_for_consistency(performance_ratio);
            }
        }

        {
            std::lock_guard<std::mutex> lock(stats_lock);
            global_stats.recursive_overflow_events++;
        }

        std::cout << "🚀 Context " << context_id << ": Recursive protection applied!\n";
        std::cout << "   Helper threads: " << helper_thread_pool.size() << "\n";
        std::cout << "   Dynamic allocation: " << scaled_allocation << " bytes\n";
        std::cout << "   Self-capture monitoring: Active\n";
    }

    // 🎭 CREATE SELF-CAPTURING CONTEXT (PYTHON → C++)
    std::shared_ptr<ContextOverflowGuard> create_self_capturing_context() const {
        // 🎯 CREATE NEW CONTEXT WITH DYNAMIC THRESHOLD
        const double aggressiveness = performance_metrics.aggressiveness_level.load();
        size_t monitor_threshold = std::max(static_cast<size_t>(1),
            static_cast<size_t>((overflow_threshold_bytes / (1024 * 1024 * 4)) * aggressiveness)
        );

        auto monitor_context = std::make_shared<ContextOverflowGuard>(
            performance_metrics.current_term_allocation.load() * 2,  // Dynamic base allocation
            monitor_threshold,           // Dynamic threshold
            true,                        // enable_worker_delegation
            false,                       // enable_recursive_protection (prevent infinite recursion)
            1                            // max_helper_threads (minimal for monitor)
        );

        std::cout << "🎭 Created self-capturing context " << monitor_context->context_id
                  << " to monitor context " << context_id << "\n";
        std::cout << "   Dynamic monitor threshold: " << monitor_threshold << " MB\n";
        std::cout << "   Aggressiveness factor: " << aggressiveness << "\n";

        return monitor_context;
    }

    // 🧵 CREATE HELPERS WITH BOOLEAN FLOW (YOUR PYTHON NON-BLOCKING LOGIC → C++)
    void create_helpers_with_boolean_flow(const int thread_count) {
        std::cout << "🧵 Context " << context_id << ": Boolean-flow creating "
                  << thread_count << " helper threads...\n";

        int threads_created = 0;

        for (int i = 0; i < thread_count; ++i) {
            std::string thread_name = "BoolHelper_" + std::to_string(context_id) +
                                    "_" + std::to_string(helper_thread_pool.size() + 1);

            helper_thread_pool.emplace_back([this]() {
                boolean_helper_worker(helper_thread_pool.size());
            });

            threads_created++;

            {
                std::lock_guard<std::mutex> lock(stats_lock);
                ++global_stats.helper_threads_created;
            }

            std::cout << "🧵 Boolean helper thread '" << thread_name << "' started\n";
        }

        // 🎯 BOOLEAN LOGIC: SET FLAGS BASED ON SUCCESS
        helpers_ready = (threads_created == thread_count);
        state_flags.helpers_available = helpers_ready.load();
        allocation_shift_ready = helpers_ready.load() || state_flags.allocation_doubled.load();

        std::cout << "🎯 Context " << context_id << ": Boolean flags updated - helpers_ready: "
                  << helpers_ready.load() << "\n";
    }

    // 🎯 BOOLEAN CHECK IF ALLOCATION CAN SHIFT WITHOUT WAITING
    bool can_shift_allocation_immediately() const {
        return (
            state_flags.flow_uninterrupted.load()
            && !overflow_flag_triggered.load()
            // FIX: cast max_helper_threads to size_t for comparison
            && (state_flags.memory_expanded.load() || helper_thread_pool.size() < static_cast<size_t>(max_helper_threads))
        );
    }


    // ⚡ BOOLEAN-DRIVEN HELPER THREAD WORKER (NO BLOCKING WAITS!)
    void boolean_helper_worker(const int thread_id) {
        std::cout << "⚡ Boolean helper thread " << thread_id << " for context "
                  << context_id << " is active\n";

        try {
            // 🎯 BOOLEAN-DRIVEN WORK LOOP - CONTINUE WHILE FLAGS ARE ACTIVE
            while (
                overflow_detected.load()
                || recursive_overflow_count.load() > 0
                || !state_flags.flow_uninterrupted.load()
            ) {
                // 🎭 BOOLEAN-DRIVEN HELPER TASKS
                const bool task_completed = perform_boolean_helper_tasks(thread_id);

                // 🎯 USE OR LOGIC TO DETERMINE CONTINUATION
                const bool should_continue = (
                    overflow_detected.load()
                    || task_completed
                    || allocation_shift_ready.load()
                );

                if (!should_continue) {
                    break;
                }

                // 🌈 MICRO-SLEEP TO PREVENT CPU SPINNING (NON-BLOCKING)
                std::this_thread::sleep_for(std::chrono::microseconds(1000));  // 1ms
            }

        } catch (const std::exception& e) {
            std::cout << "❌ Boolean helper thread " << thread_id << " error: " << e.what() << "\n";
        }

        // 🎯 SET COMPLETION FLAGS
        state_flags.helpers_available = true;
        std::cout << "🏁 Boolean helper thread " << thread_id << " for context "
                  << context_id << " completed\n";
    }

    // 🔧 BOOLEAN-DRIVEN HELPER TASKS WITH FLAG-BASED FLOW CONTROL + DYNAMIC TUNING
    bool perform_boolean_helper_tasks(const int thread_id) {
        bool task_completed = false;

        try {
            // 🎯 TASK SELECTION USING BOOLEAN OR LOGIC + DYNAMIC PERFORMANCE CONSIDERATION
            const double aggressiveness = performance_metrics.aggressiveness_level.load();

            if (thread_id == 1 || !state_flags.memory_expanded.load()) {
                // Aggressive garbage collection simulation with dynamic intensity
                const int collections = static_cast<int>(aggressiveness * 3);  // 1-4 collections
                if (const int collected = simulate_garbage_collection(collections); collected > 0) {
                    std::cout << "🗑️ Boolean helper " << thread_id << ": Collected "
                              << collected << " objects (aggressiveness: " << aggressiveness << ")\n";
                    state_flags.memory_expanded = true;
                    task_completed = true;
                }
            }

            else if (thread_id == 2 || !state_flags.allocation_doubled.load()) {
                // Cache cleanup for delegated workers with dynamic sizing
                const bool cleaned = boolean_cleanup_worker_caches(aggressiveness);
                state_flags.allocation_doubled = cleaned || state_flags.allocation_doubled.load();
                task_completed = cleaned;
            }

            else {
                // Memory defragmentation simulation with dynamic efficiency
                bool defrag_success = boolean_memory_defragmentation(aggressiveness);
                state_flags.recursive_active = defrag_success;
                task_completed = defrag_success;
            }

            // 🎯 UPDATE FLOW FLAGS USING OR LOGIC
            state_flags.flow_uninterrupted = (
                state_flags.flow_uninterrupted.load()
                || task_completed
                || allocation_shift_ready.load()
            );

        } catch (const std::exception& e) {
            std::cout << "⚠️ Boolean helper thread " << thread_id << " task error: " << e.what() << "\n";
            task_completed = false;
        }

        return task_completed;
    }

    // 🤯 CHECK FOR RECURSIVE OVERFLOW AGAINST SELF (PYTHON → C++)
    bool check_recursive_overflow_against_self() {
        if (!self_capturing_context) {
            return false;
        }

        try {
            const size_t monitor_memory = self_capturing_context->get_current_memory_usage();

            if (const size_t monitor_threshold = self_capturing_context->overflow_threshold_bytes; monitor_memory > monitor_threshold) {
                std::cout << "🤯 Context " << context_id << ": RECURSIVE OVERFLOW DETECTED!\n";
                std::cout << "   Self-capture context " << self_capturing_context->context_id
                          << " overflowed!\n";
                std::cout << "   Monitor memory: " << (monitor_memory / (1024*1024)) << " MB\n";
                std::cout << "   Monitor threshold: " << (monitor_threshold / (1024*1024)) << " MB\n";

                apply_emergency_recursive_protection();
                return true;
            }

        } catch (const std::exception& e) {
            std::cout << "❌ Recursive overflow check error: " << e.what() << "\n";
        }

        return false;
    }

    // 🚨 APPLY EMERGENCY RECURSIVE PROTECTION + DYNAMIC SCALING
    void apply_emergency_recursive_protection() {
        std::cout << "🚨 Context " << context_id << ": APPLYING EMERGENCY RECURSIVE PROTECTION!\n";

        // 🎯 EMERGENCY ALLOCATION WITH DYNAMIC SCALING
        const double aggressiveness = performance_metrics.aggressiveness_level.load();
        const size_t emergency_allocation = static_cast<size_t>(
            performance_metrics.current_term_allocation.load() *
            (8 << recursive_overflow_count.load()) *
            aggressiveness
        );

        // 🧵 CREATE EMERGENCY HELPER THREADS (DYNAMIC COUNT)
        const int emergency_threads = std::min(
            static_cast<int>(max_helper_threads * 2 * aggressiveness),
            6
        );

        if (emergency_threads > static_cast<int>(helper_thread_pool.size())) {
            create_helpers_with_boolean_flow(emergency_threads - helper_thread_pool.size());
        }

        // 🗑️ FORCE AGGRESSIVE CLEANUP WITH DYNAMIC INTENSITY
        const int cleanup_rounds = static_cast<int>(3 * aggressiveness);
        for (int i = 0; i < cleanup_rounds; ++i) {
            simulate_garbage_collection(static_cast<int>(aggressiveness));
        }

        std::cout << "🚨 Emergency protection applied:\n";
        std::cout << "   Emergency allocation: " << emergency_allocation << " bytes\n";
        std::cout << "   Emergency helper threads: " << helper_thread_pool.size() << "\n";
        std::cout << "   Dynamic cleanup rounds: " << cleanup_rounds << "\n";
        std::cout << "   Aggressiveness factor: " << aggressiveness << "\n";

        {
            std::lock_guard<std::mutex> lock(stats_lock);
            ++global_stats.recursive_overflow_events;
        }
    }

private:
    // 🔧 HELPER METHODS WITH DYNAMIC TUNING (PYTHON → C++)
    void handle_context_overflow(size_t memory_growth) {
        overflow_detected = true;
        ++recursive_overflow_count;

        {
            std::lock_guard<std::mutex> lock(stats_lock);
            ++global_stats.overflow_events;
            global_stats.max_recursive_depth = std::max(
                global_stats.max_recursive_depth.load(),
                recursive_overflow_count.load()
            );
        }

        std::cout << "⚠️ Context " << context_id << ": OVERFLOW DETECTED! (Recursive: "
                  << recursive_overflow_count.load() << ")\n";
        std::cout << "Memory growth:" << (static_cast<double>(memory_growth) / PerformanceMetrics::MB) << " MB";
        std::cout << "Threshold:" << (overflow_threshold_bytes = (PerformanceMetrics::KB / PerformanceMetrics::MB)) << " MB\n";

        if (enable_recursive_protection &&
            recursive_overflow_count.load() <= max_recursive_depth) {
            apply_recursive_overflow_protection(memory_growth);
        }

        if (enable_worker_delegation) {
            // 🎯 DYNAMIC REASSIGNMENT ALLOCATION
            const double aggressiveness = performance_metrics.aggressiveness_level.load();
            const size_t reassignment_allocation = static_cast<size_t>(
                performance_metrics.current_term_allocation.load() *
                (2 << recursive_overflow_count.load()) *
                aggressiveness
            );
            const auto worker = create_overflow_worker(reassignment_allocation);

            std::cout << "🔄 Context " << context_id << ": Created overflow reassignment worker "
                      << worker->worker_id << " (dynamic allocation: " << reassignment_allocation << ")\n";
        }
    }

    std::shared_ptr<cortex::OverflowWorker> create_overflow_worker(size_t allocated_memory) {
        int worker_id = ++worker_counter;
        auto worker = std::make_shared<cortex::OverflowWorker>(worker_id, 0, allocated_memory);
        overflow_workers[worker_id] = worker; delegated_workers.push_back(worker_id);
        return worker;
    }

    template<typename Operation, typename... Args>
    static auto execute_in_worker(Operation&& operation, Args&&... args)
        -> decltype(operation(args...)) {
        // Execute operation in current context (simplified)
        return operation(args...);
    }

    bool handle_context_exception(const std::exception_ptr &exc_ptr) {
        try {
            std::rethrow_exception(exc_ptr);
        } catch (const std::exception& e) {
            std::cout << "⚠️ Context " << context_id << ": Exception caught: " << e.what() << "\n";

            context_data["exception"] = std::string(e.what());

            if (overflow_detected.load() && enable_worker_delegation) {
                std::cout << "🔄 Context " << context_id << ": Attempting overflow recovery\n";
                return true;  // Suppress exception for overflow recovery
            }
        }

        return false;
    }

    void cleanup_context() {
        // Cleanup helper threads
        for (auto& thread : helper_thread_pool) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        helper_thread_pool.clear();

        // Cleanup self-capturing context
        if (self_capturing_context) {
            self_capturing_context->cleanup_context();
        }

        // Cleanup delegated workers
        for (int worker_id : delegated_workers) {
            if (auto it = overflow_workers.find(worker_id); it != overflow_workers.end()) {
                it->second->is_active = false;
            }
        }

        context_data.clear();
    }

    void report_context_completion(double duration, size_t memory_growth) {
        std::cout << "🏴‍☠️ Context " << context_id << " complete: " << duration << "s\n";
        std::cout << "   Memory growth: " << (memory_growth / (1024*1024)) << " MB\n";
        std::cout << "   Dynamic throughput: " << performance_metrics.current_throughput.load() << " terms/s\n";
        std::cout << "   Aggressiveness level: " << performance_metrics.aggressiveness_level.load() << "\n";

        if (overflow_detected.load()) {
            std::cout << "   ⚠️ Overflow handled with " << delegated_workers.size() << " workers\n";
        }

        if (context_data.find("exception") != context_data.end()) {
            std::cout << "   🛡️ Exception handled\n";
        }
    }

    static size_t get_current_memory_usage() {
        // Simplified memory usage calculation
        return 1024 * 1024;  // 1MB default
    }

    static int simulate_garbage_collection(const int intensity = 1) {
        // Simulate garbage collection with dynamic intensity
        return 42 * intensity;  // Simulated collected objects
    }

    bool boolean_cleanup_worker_caches(const double aggressiveness = 1.0) const {
        // Simulate cache cleanup with dynamic aggressiveness
        const int cleaned_count = static_cast<int>(delegated_workers.size() * aggressiveness);
        return cleaned_count > 0;
    }

    static bool boolean_memory_defragmentation(double aggressiveness = 1.0) {
        // Simulate memory defragmentation with dynamic efficiency
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.7, 1.0);

        const double efficiency = dis(gen) * aggressiveness;
        return efficiency > 0.8;
    }

public:
    // 📊 STATIC METHODS WITH DYNAMIC PERFORMANCE INSIGHTS
    // FIX: do not copy atomics; return const ref
    static const ContextStats& get_global_stats() {
        return global_stats;
    }

    static std::unordered_map<int, std::shared_ptr<ContextOverflowGuard>> get_active_contexts() {
        std::lock_guard<std::mutex> lock(stats_lock);
        return active_contexts; // map of shared_ptr is copyable; OK
    }

    static void print_global_statistics() {
        // FIX: avoid copying ContextStats with atomics
        const auto& stats = get_global_stats();

        std::cout << "\n📊 GLOBAL CONTEXT OVERFLOW GUARD STATISTICS:\n";
        std::cout << "=" << std::string(60, '=') << "\n";
        std::cout << "🔢 Total contexts: " << stats.total_contexts.load() << "\n";
        std::cout << "⚠️ Overflow events: " << stats.overflow_events.load() << "\n";
        std::cout << "🤯 Recursive overflow events: " << stats.recursive_overflow_events.load() << "\n";
        std::cout << "🎭 Self-capture events: " << stats.self_capture_events.load() << "\n";
        std::cout << "🚀 Worker delegations: " << stats.worker_delegations.load() << "\n";
        std::cout << "🧵 Helper threads created: " << stats.helper_threads_created.load() << "\n";
        std::cout << "📏 Max recursive depth: " << stats.max_recursive_depth.load() << "\n";
        std::cout << "⏱️ Average execution time: " << stats.average_execution_time.load() << "s\n";
        std::cout << "🛡️ Exceptions handled: " << stats.exceptions_handled.load() << "\n";
        std::cout << "🗑️ Garbage collections: " << stats.garbage_collections.load() << "\n";

        std::cout << "\n🎯 DYNAMIC PERFORMANCE TUNING:\n";
        std::cout << std::string(40, '-') << "\n";
        std::cout << "📈 Aggressiveness factor: " << stats.aggressiveness_factor.load() << "\n";
        std::cout << "💾 Current term allocation: " << stats.allocated_term_base_size.load() << " bytes\n";
        std::cout << "🎯 Max term allocation: " << stats.allocated_term_max_size.load() << " bytes\n";
        std::cout << "📊 Performance consistency target: " << (stats.performance_consistency_target.load() * 100) << "%\n";
    }

    // 🌌 STATIC MEMBER DEFINITIONS
    ContextStats ContextOverflowGuard::global_stats{};
    std::unordered_map<int, std::shared_ptr<ContextOverflowGuard>> ContextOverflowGuard::active_contexts{};
    std::unordered_map<int, std::shared_ptr<OverflowWorker>> ContextOverflowGuard::overflow_workers{};
    std::atomic<int> ContextOverflowGuard::worker_counter{0};
    std::atomic<int> ContextOverflowGuard::context_counter{0};
    std::mutex ContextOverflowGuard::stats_lock{};
    std::unique_ptr<AdaptivePerformanceTuner> ContextOverflowGuard::performance_tuner = nullptr;
};

};// namespace cortex