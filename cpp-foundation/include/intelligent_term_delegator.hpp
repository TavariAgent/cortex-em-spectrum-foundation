#pragma once
#include <any>
#include <random>
#include <vector>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <memory>
#include <algorithm>
#include <string>
#include <chrono>
#include <iostream>
#include <ios>
#include <cmath>
#include <boost/multiprecision/cpp_dec_float.hpp>

namespace cortex {
namespace delegation {

    using CosmicPrecision = boost::multiprecision::cpp_dec_float<CORTEX_EM_SPECTRUM_PRECISION>;

struct TermComplexity {
    CosmicPrecision term_value;
    size_t byte_cost;
    std::string complexity_level;  // 'low', 'medium', 'high', 'overflow'
    double estimated_compute_time;
    size_t memory_requirement;
    int thread_group;  // 1 = wide arrays, 2 = split arrays

    // 🌈 EM SPECTRUM SPECIFIC COMPLEXITY
    bool is_em_spectrum_complex() const {
        return (byte_cost > 2048 ||
                complexity_level == "high" ||
                complexity_level == "overflow");
    }
};

struct ArrayGroupConfig {
    int group_id;
    int thread_count;
    std::string array_split_method;  // 'wide' or 'split'
    int max_terms_per_thread;
    std::string complexity_filter;  // 'complex', 'simple', 'all'
};

class IntelligentTermDelegator {
private:
    int total_threads;
    size_t byte_cost_threshold;
    std::vector<TermComplexity> complexity_samples;

    // 🧠 PYTHON LOGIC TRANSLATED!
    ArrayGroupConfig group1_config;  // Wide arrays for complex terms
    ArrayGroupConfig group2_config;  // Split arrays for simple terms

    struct DelegationStats {
        int complex_terms = 0;
        int simple_terms = 0;
        double byte_cost_average = 0.0;
        double delegation_time = 0.0;
    } delegation_stats;

    std::mutex complexity_mutex;
    std::mutex delegation_mutex;

public:
    explicit IntelligentTermDelegator(int threads = std::thread::hardware_concurrency())
        : total_threads(threads), byte_cost_threshold(1024) {

        setup_array_groups();

        std::cout << "🧠 Intelligent Term Delegator initialized\n";
        std::cout << "   Total threads: " << total_threads << "\n";
        std::cout << "   Group 1 (Wide Arrays): " << group1_config.thread_count << " threads\n";
        std::cout << "   Group 2 (Split Arrays): " << group2_config.thread_count << " threads\n";
    }

    // 🔍 COMPLEXITY DETECTION (PYTHON → C++)
    std::unordered_map<std::string, std::any>
    detect_input_complexity(const std::vector<CosmicPrecision>& input_data) {

        auto start_time = std::chrono::high_resolution_clock::now();

        std::unordered_map<std::string, std::any> complexity_analysis;
        complexity_analysis["total_terms"] = static_cast<int>(input_data.size());

        // Sample terms for complexity analysis (PYTHON)
        size_t sample_size = std::min(size_t(100), input_data.size());
        std::vector<size_t> sample_indices;

        for (size_t i = 0; i < sample_size; ++i) {
            size_t idx = (i * input_data.size()) / sample_size;
            sample_indices.push_back(idx);
        }

        std::vector<size_t> byte_costs;
        size_t total_byte_cost = 0;

        // Analyze each sample term
        for (size_t idx : sample_indices) {
            size_t byte_cost = calculate_term_byte_cost(input_data[idx]);
            byte_costs.push_back(byte_cost);
            total_byte_cost += byte_cost;
        }

        // Calculate statistics (PYTHON)
        if (!byte_costs.empty()) {
            double average_byte_cost = static_cast<double>(total_byte_cost) / byte_costs.size();
            complexity_analysis["byte_cost_average"] = average_byte_cost;
            byte_cost_threshold = static_cast<size_t>(average_byte_cost / 2);  // Half average for "low byte"
        }

        // Classify complexity distribution
        std::unordered_map<std::string, int> complexity_distribution = {
            {"low", 0}, {"medium", 0}, {"high", 0}, {"overflow", 0}
        };

        for (size_t byte_cost : byte_costs) {
            if (byte_cost < byte_cost_threshold) {
                complexity_distribution["low"]++;
            } else if (byte_cost < byte_cost_threshold * 2) {
                complexity_distribution["medium"]++;
            } else if (byte_cost < byte_cost_threshold * 4) {
                complexity_distribution["high"]++;
            } else {
                complexity_distribution["overflow"]++;
            }
        }

        complexity_analysis["complexity_distribution"] = complexity_distribution;

        // Determine overall complexity level (PYTHON)
        double high_complex_ratio = static_cast<double>(
            complexity_distribution["high"] + complexity_distribution["overflow"]
        ) / sample_size;

        if (high_complex_ratio > 0.3) {  // More than 30% high complexity
            complexity_analysis["complexity_level"] = std::string("complex");
            complexity_analysis["recommended_split"] = std::string("wide_arrays_needed");
        } else {
            complexity_analysis["complexity_level"] = std::string("simple");
            complexity_analysis["recommended_split"] = std::string("normal_split_sufficient");
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto detection_time = std::chrono::duration<double>(end_time - start_time).count();
        complexity_analysis["detection_time"] = detection_time;

        print_complexity_detection_results(complexity_analysis);

        return complexity_analysis;
    }

    // 🎯 INTELLIGENT DELEGATION (PYTHON → C++)
    std::unordered_map<std::string, std::vector<CosmicPrecision>>
    delegate_terms(const std::vector<CosmicPrecision>& input_data,
                   const std::unordered_map<std::string, std::any>& complexity_analysis)
    {
        auto start_time = std::chrono::high_resolution_clock::now();

        std::unordered_map<std::string, std::vector<CosmicPrecision>> delegated_terms = {
            {"group1_complex", {}},
            {"group2_simple", {}}
        };

        // Use size_t for the key to match i and avoid narrowing
        std::unordered_map<size_t, std::unordered_map<std::string, std::any>> delegation_map;

        if (input_data.size() <= 50000) {
            for (size_t i = 0; i < input_data.size(); ++i) {
                size_t byte_cost = calculate_term_byte_cost(input_data[i]);

                if (byte_cost >= byte_cost_threshold * 2) {
                    delegated_terms["group1_complex"].push_back(input_data[i]);
                    delegation_stats.complex_terms++;

                    delegation_map[i] = {
                        {"group",      1},                        // std::any holds int
                        {"complexity", std::string("complex")},   // std::any holds std::string
                        {"byte_cost",  byte_cost}                 // std::any holds size_t
                    };
                } else {
                    delegated_terms["group2_simple"].push_back(input_data[i]);
                    delegation_stats.simple_terms++;

                    delegation_map[i] = {
                        {"group",      2},
                        {"complexity", std::string("simple")},
                        {"byte_cost",  byte_cost}
                    };
                }
            }
        } else {
            const auto& complexity_dist =
                std::any_cast<const std::unordered_map<std::string, int>&>(
                    complexity_analysis.at("complexity_distribution"));

            double complex_ratio =
                static_cast<double>(complexity_dist.at("high")) /
                static_cast<double>(std::any_cast<int>(complexity_analysis.at("total_terms")));

            for (size_t i = 0; i < input_data.size(); ++i) {
                size_t byte_cost;
                if (i % 10 == 0) {
                    byte_cost = calculate_term_byte_cost(input_data[i]);
                } else {
                    static std::random_device rd;
                    static std::mt19937 gen(rd());
                    static std::uniform_real_distribution<double> dis(0.0, 1.0);

                    byte_cost = (dis(gen) < complex_ratio)
                                    ? byte_cost_threshold * 3
                                    : byte_cost_threshold / 2;
                }

                if (byte_cost >= byte_cost_threshold * 2) {
                    delegated_terms["group1_complex"].push_back(input_data[i]);
                    delegation_stats.complex_terms++;
                } else {
                    delegated_terms["group2_simple"].push_back(input_data[i]);
                    delegation_stats.simple_terms++;
                }
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        delegation_stats.delegation_time =
            std::chrono::duration<double>(end_time - start_time).count();

        print_delegation_results(delegated_terms);
        return delegated_terms;
    }

private:
    void setup_array_groups() {
        const int total = std::max(1, total_threads);

        // Reserve up to 4 for complex, but leave at least 1 for simple if possible.
        int group1_threads = std::min(4, std::max(1, total - 1));
        int group2_threads = total - group1_threads; // 0 if total == 1

        group1_config = {
            1,              // group_id
            group1_threads, // thread_count
            "wide",         // array_split_method
            1000,           // max_terms_per_thread
            "complex"       // complexity_filter
        };

        group2_config = {
            2,              // group_id
            group2_threads, // thread_count (may be 0 when total==1)
            "split",        // array_split_method
            5000,           // max_terms_per_thread
            "simple"        // complexity_filter
        };
    }

    // 📏 BYTE COST CALCULATION (PYTHON LOGIC TRANSLATED!)
    size_t calculate_term_byte_cost(const CosmicPrecision& term) const {
        try {
            // Convert to string to measure precision requirements
            std::string str_repr = term.str(0, std::ios_base::fmtflags(0));

            size_t base_cost = str_repr.length() + 16;

            if (str_repr.length() > 50) {
                base_cost += str_repr.length() / 2;
            }

            // Prefer Boost's abs for multiprecision numbers
            using boost::multiprecision::abs;
            if (abs(term) > CosmicPrecision("1e10")) {
                base_cost += 32;  // Large magnitude cost
            }

            if (str_repr.find('e') != std::string::npos) {
                base_cost += 16;
            }

            return base_cost;
        } catch (...) {
            return 64;  // Default 64 bytes for calculation errors
        }
    }

    void print_complexity_detection_results(const std::unordered_map<std::string, std::any>& analysis) {
        std::cout << "🔍 Input Complexity Detection Complete:\n";
        std::cout << "   Total terms: " << std::any_cast<int>(analysis.at("total_terms")) << "\n";
        std::cout << "   Average byte cost: " << std::any_cast<double>(analysis.at("byte_cost_average")) << " bytes\n";
        std::cout << "   Complexity level: " << std::any_cast<std::string>(analysis.at("complexity_level")) << "\n";
        std::cout << "   Recommended split: " << std::any_cast<std::string>(analysis.at("recommended_split")) << "\n";
        std::cout << "   Detection time: " << std::any_cast<double>(analysis.at("detection_time")) << "s\n";
    }

    void print_delegation_results(const std::unordered_map<std::string, std::vector<CosmicPrecision>>& delegated) {
        std::cout << "🎯 Term Delegation Complete:\n";
        std::cout << "   Group 1 (Complex): " << delegated.at("group1_complex").size() << " terms\n";
        std::cout << "   Group 2 (Simple): " << delegated.at("group2_simple").size() << " terms\n";
        std::cout << "   Delegation time: " << delegation_stats.delegation_time << "s\n";

        size_t total_terms = delegated.at("group1_complex").size() + delegated.at("group2_simple").size();
        if (total_terms > 0) {
            double complex_ratio = static_cast<double>(delegated.at("group1_complex").size()) / total_terms;
            std::cout << "   Complex ratio: " << (complex_ratio * 100.0) << "%\n";
        }
    }
};

} // namespace delegation
} // namespace cortex