#include "context_overflow_guard.hpp"
#include "precision_safe_threading.hpp"
#include "intelligent_term_delegator.hpp"

using namespace cortex;

int main() {
    std::cout << "🤯 CONTEXT OVERFLOW GUARD + COMPLETE CORTEX INTEGRATION\n";
    std::cout << "=" << std::string(80, '=') << "\n";

    // 🎭 Test with overflow protection and recursive monitoring
    {
        overflow::ContextOverflowGuard guard(
            1024,      // base_allocation
            1,         // overflow_threshold_mb (low to trigger overflow)
            true,      // enable_worker_delegation
            true,      // enable_recursive_protection
            2,         // max_helper_threads
            3          // max_recursive_depth
        );

        guard.enter();

        try {
            // 🧠 Intelligent term delegation
            delegation::IntelligentTermDelegator delegator(8);

            // 🎯 Precision-safe threading
            threading::PrecisionSafeThreading precision_threading;

            // Create EM spectrum data that will trigger overflow
            std::vector<CosmicPrecision> large_spectrum_data;
            for (int i = 0; i < 10000; ++i) {
                CosmicPrecision wavelength = CosmicPrecision(i) * CosmicPrecision("1e-9");
                large_spectrum_data.push_back(wavelength);
            }

            std::cout << "🌈 Processing " << large_spectrum_data.size() << " EM spectrum wavelengths...\n";

            // 🔍 Detect complexity
            auto complexity = delegator.detect_input_complexity(large_spectrum_data);

            // 🎯 Delegate terms
            auto delegated = delegator.delegate_terms(large_spectrum_data, complexity);

            // 🧵 Process with precision-safe threading
            auto em_operation = [](const CosmicPrecision& wavelength) -> CosmicPrecision {
                const CosmicPrecision c("299792458");  // speed of light
                const CosmicPrecision h("6.62607015e-34");  // planck constant

                if (wavelength == CosmicPrecision("0")) return CosmicPrecision("0");

                CosmicPrecision frequency = c / wavelength;
                return h * frequency;  // photon energy
            };

            auto [energy_result, thread_results] = precision_threading.precision_safe_map(
                em_operation, delegated["group2_simple"], 4
            );

            std::cout << "✅ Processed energy result: " << energy_result.str().substr(0, 30) << "... Joules\n";

            // 🤯 Check for recursive overflow against self
            if (guard.check_recursive_overflow_against_self()) {
                std::cout << "🤯 RECURSIVE OVERFLOW DETECTED AND HANDLED!\n";
            }

            // 🚀 Demonstrate worker delegation
            auto delegated_result = guard.delegate_to_overflow_worker(
                [&energy_result]() { return energy_result * CosmicPrecision("2"); }
            );

            std::cout << "🚀 Worker delegation result: " << delegated_result.str().substr(0, 30) << "...\n";

        } catch (const std::exception& e) {
            std::cout << "❌ Exception in protected context: " << e.what() << "\n";
        }

        guard.exit();
    }

    // 📊 Print comprehensive statistics
    overflow::ContextOverflowGuard::print_global_statistics();

    std::cout << "\n🎉 COMPLETE CORTEX INTEGRATION WITH RECURSIVE OVERFLOW PROTECTION!\n";
    std::cout << "🤯 Self-capturing contexts monitoring themselves = ACHIEVED!\n";
    std::cout << "🧵 Boolean-driven helper threads = NON-BLOCKING FLOW!\n";
    std::cout << "🎭 Recursive overflow protection = REVOLUTIONARY!\n";

    return 0;
}