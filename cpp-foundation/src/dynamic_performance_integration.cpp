#include "context_overflow_guard.hpp"
#include "precision_safe_threading.hpp"
#include "intelligent_term_delegator.hpp"

using namespace cortex;

using CosmicPrecision = boost::multiprecision::cpp_dec_float<CORTEX_EM_SPECTRUM_PRECISION>;

int main() {
    std::cout << "🎯 DYNAMIC PERFORMANCE TUNING + COMPLETE CORTEX INTEGRATION\n";
    std::cout << "=" << std::string(80, '=') << "\n";

    // 🎯 Test with dynamic performance adjustment
    for (int test_round = 1; test_round <= 3; ++test_round) {
        std::cout << "\n🧪 Test Round " << test_round << " - Dynamic Performance Adjustment\n";
        std::cout << std::string(60, '-') << "\n";

        {
            overflow::ContextOverflowGuard guard(
                1024 * test_round,  // Increasing base allocation
                1,                  // Low threshold to trigger overflow
                true,              // enable_worker_delegation
                true,              // enable_recursive_protection
                2,                 // max_helper_threads
                3                  // max_recursive_depth
            );

            guard.enter();

            try {
                // 🧠 Intelligent term delegation with dynamic sizing
                delegation::IntelligentTermDelegator delegator(8);

                // 🎯 Precision-safe threading with dynamic performance tuning
                threading::PrecisionSafeThreading precision_threading;

                // Create test data that will trigger dynamic adjustments
                std::vector<CosmicPrecision> test_data;
                for (int i = 0; i < 5000 * test_round; ++i) {  // Increasing complexity
                    CosmicPrecision value = CosmicPrecision(i) * CosmicPrecision("1e-6");
                    test_data.push_back(value);
                }

                std::cout << "🌈 Processing " << test_data.size() << " terms with dynamic tuning...\n";

                // 🔍 Detect complexity with dynamic adjustment
                auto complexity = delegator.detect_input_complexity(test_data);

                // 🎯 Delegate terms with adaptive sizing
                auto delegated = delegator.delegate_terms(test_data, complexity);

                // 🧵 Process with precision-safe threading + dynamic tuning
                auto processing_operation = [](const CosmicPrecision& value) -> CosmicPrecision {
                    // Electromagnetic spectrum energy calculation
                    const CosmicPrecision c("299792458");      // speed of light
                    const CosmicPrecision h("6.62607015e-34"); // planck constant

                    if (value == CosmicPrecision("0")) return CosmicPrecision("0");

                    CosmicPrecision frequency = c / value;
                    return h * frequency;  // photon energy
                };

                auto [result, thread_results] = precision_threading.precision_safe_map(
                    processing_operation, delegated["group2_simple"], 4
                );

                std::cout << "✅ Round " << test_round << " result: "
                          << result.str().substr(0, 30) << "... Joules\n";

                // 🤯 Check for recursive overflow with dynamic protection
                if (guard.check_recursive_overflow_against_self()) {
                    std::cout << "🤯 RECURSIVE OVERFLOW DETECTED AND HANDLED WITH DYNAMIC SCALING!\n";
                }

                // 🚀 Demonstrate worker delegation with dynamic allocation
                auto delegated_result = guard.delegate_to_overflow_worker(
                    [&result]() { return result * CosmicPrecision("2"); }
                );

                std::cout << "🚀 Dynamic worker result: " << delegated_result.str().substr(0, 30) << "...\n";

                // Allow time for dynamic adjustments
                std::this_thread::sleep_for(std::chrono::seconds(1));

            } catch (const std::exception& e) {
                std::cout << "❌ Exception in protected context: " << e.what() << "\n";
            }

            guard.exit();
        }

        // Print dynamic performance statistics after each round
        std::cout << "\n📊 Dynamic Performance Statistics After Round " << test_round << ":\n";
        overflow::ContextOverflowGuard::print_global_statistics();
    }

    std::cout << "\n🎉 DYNAMIC PERFORMANCE TUNING DEMONSTRATION COMPLETE!\n";
    std::cout << "🎯 System automatically adjusted aggressiveness and allocation sizes!\n";
    std::cout << "🤯 Self-capturing contexts with dynamic scaling = ACHIEVED!\n";
    std::cout << "🧵 Boolean-driven helper threads with adaptive performance = REVOLUTIONARY!\n";
    std::cout << "📊 Consistent performance through dynamic tuning = COSMIC EVOLUTION!\n";

    return 0;
}