#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <future>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <boost/multiprecision/cpp_dec_float.hpp>

namespace cortex {
namespace threading {

using CosmicPrecision = boost::multiprecision::cpp_dec_float<141>;

struct PrecisionThreadResult {
    int thread_id;
    CosmicPrecision result;
    bool precision_maintained;
    double calculation_time;
    size_t terms_processed;

    PrecisionThreadResult(int id = -1,
                         const CosmicPrecision& res = CosmicPrecision("0"),
                         bool precision = true,
                         double time = 0.0,
                         size_t terms = 0)
        : thread_id(id), result(res), precision_maintained(precision),
          calculation_time(time), terms_processed(terms) {}
};

class PrecisionSafeThreading {
private:
    std::mutex precision_lock;
    std::mutex result_lock;
    std::unordered_map<int, CosmicPrecision> thread_results;

    struct ThreadingStats {
        std::atomic<size_t> total_operations{0};
        std::atomic<size_t> successful_threads{0};
        std::atomic<size_t> precision_errors{0};
        std::atomic<double> total_threading_time{0.0};
        std::atomic<size_t> terms_processed{0};
    } stats;

    static constexpr int COSMIC_PRECISION_DIGITS = 141;
    static constexpr double THREAD_TIMEOUT_SECONDS = 30.0;

public:
    explicit PrecisionSafeThreading();

    /**
     * Execute operation across multiple threads while maintaining 141-decimal precision
     * @param operation Function to execute on each element
     * @param input_data Vector of data to process
     * @param num_threads Number of threads to use
     * @return Pair of combined result and individual thread results
     */
    std::pair<CosmicPrecision, std::vector<PrecisionThreadResult>>
    precision_safe_map(
        std::function<CosmicPrecision(const CosmicPrecision&)> operation,
        const std::vector<CosmicPrecision>& input_data,
        int num_threads = 4
    );

    /**
     * Calculate π using Machin's formula with precision-safe threading
     * @param terms Number of terms to calculate
     * @param num_threads Number of threads to use
     * @return Pair of calculated π and calculation metrics
     */
    std::pair<CosmicPrecision, std::unordered_map<std::string, std::any>>
    precision_safe_machin_pi(int terms = 1000, int num_threads = 4);

    /**
     * Process electromagnetic spectrum data with threading
     * @param wavelength_data Vector of wavelength values in meters
     * @param num_threads Number of threads to use
     * @return Pair of total energy and thread results
     */
    std::pair<CosmicPrecision, std::vector<PrecisionThreadResult>>
    precision_safe_em_spectrum_processing(
        const std::vector<CosmicPrecision>& wavelength_data,
        int num_threads = 4
    );

    struct ThreadingStatistics {
        size_t total_operations;
        size_t successful_threads;
        size_t precision_errors;
        double total_threading_time;
        size_t terms_processed;
        double average_thread_time;
        double throughput_terms_per_second;
        double precision_success_rate;
    };

    ThreadingStatistics get_threading_statistics() const;
    void print_threading_report();

private:
    PrecisionThreadResult precision_safe_worker(
        std::function<CosmicPrecision(const CosmicPrecision&)> operation,
        const std::vector<CosmicPrecision>& args_list,
        int thread_id
    );

    bool validate_precision_maintained(const CosmicPrecision& result) const;
};

} // namespace threading
} // namespace cortex