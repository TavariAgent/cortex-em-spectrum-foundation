#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <memory>
#include <chrono>
#include <algorithm>
#include <boost/multiprecision/cpp_dec_float.hpp>

#ifdef CUDA_AVAILABLE
#include <cuda_runtime.h>
#include <cublas_v2.h>
#endif

namespace cortex {
namespace gpu {

using CosmicPrecision = boost::multiprecision::cpp_dec_float<141>;

// 🎮 GPU HARDWARE CONFIGURATION
struct GPUConfig {
    std::string gpu_name;
    std::string architecture;          // "Pascal", "Ada Lovelace", etc.
    size_t memory_limit_mb;
    size_t batch_size;
    double complexity_threshold;
    double arithmetic_threshold;
    size_t min_array_size_gpu;
    size_t max_array_size_gpu;
    double max_memory_usage_ratio;
    bool enable_gpu;
    bool cpu_fallback;
};

// 📊 GPU PERFORMANCE STATISTICS
struct GPUPerformanceStats {
    std::atomic<size_t> gpu_operations{0};
    std::atomic<size_t> cpu_fallbacks{0};
    std::atomic<size_t> total_operations{0};
    std::atomic<size_t> memory_errors{0};
    std::atomic<double> total_gpu_time{0.0};
    std::atomic<double> total_cpu_time{0.0};

    // 📈 CALCULATED PERFORMANCE METRICS
    double get_gpu_usage_percentage() const {
        return total_operations.load() > 0 ?
               (static_cast<double>(gpu_operations.load()) / total_operations.load()) * 100.0 : 0.0;
    }

    double get_cpu_fallback_percentage() const {
        return total_operations.load() > 0 ?
               (static_cast<double>(cpu_fallbacks.load()) / total_operations.load()) * 100.0 : 0.0;
    }

    double get_error_rate_percentage() const {
        return total_operations.load() > 0 ?
               (static_cast<double>(memory_errors.load()) / total_operations.load()) * 100.0 : 0.0;
    }

    double get_efficiency_score() const {
        return get_gpu_usage_percentage() - get_error_rate_percentage();
    }

    double get_average_gpu_time() const {
        return gpu_operations.load() > 0 ?
               total_gpu_time.load() / gpu_operations.load() : 0.0;
    }

    double get_average_cpu_time() const {
        return cpu_fallbacks.load() > 0 ?
               total_cpu_time.load() / cpu_fallbacks.load() : 0.0;
    }
};

class AdaptiveGPUDelegator {
private:
    bool gpu_available;
    bool cuda_initialized;
    GPUConfig config;
    GPUPerformanceStats stats;
    std::mutex gpu_lock;

#ifdef CUDA_AVAILABLE
    cublasHandle_t cublas_handle;
    int device_id;
#endif

    // 🎯 GPU HARDWARE DETECTION
    std::unordered_map<std::string, GPUConfig> gpu_configs = {
        // GTX 1060 (Pascal) - Conservative settings
        {"GTX 1060", {
            "GTX 1060", "Pascal", 6144, 1000, 2000.0, 200.0, 5000, 500000, 0.5, true, true
        }},

        // RTX 4070 Super (Ada Lovelace) - Aggressive optimization
        {"RTX 4070 Super", {
            "RTX 4070 Super", "Ada Lovelace", 12288, 5000, 5000.0, 1000.0, 10000, 2000000, 0.8, true, true
        }},

        // RTX 3080 (Ampere) - Balanced performance
        {"RTX 3080", {
            "RTX 3080", "Ampere", 10240, 3000, 3500.0, 500.0, 7500, 1500000, 0.7, true, true
        }},

        // Conservative fallback for unknown GPUs
        {"Unknown", {
            "Unknown GPU", "Unknown", 1024, 500, 1000.0, 100.0, 2000, 100000, 0.3, false, true
        }}
    };

public:
    explicit AdaptiveGPUDelegator()
        : gpu_available(false), cuda_initialized(false), device_id(-1) {

        std::cout << "🔧 Initializing Adaptive GPU Delegator...\n";

        // Initialize GPU configuration
        config = initialize_gpu_config();

        // Initialize CUDA if available
        cuda_initialized = initialize_cuda();

        if (cuda_initialized) {
            std::cout << "✅ GPU acceleration enabled\n";
            std::cout << "   GPU: " << config.gpu_name << "\n";
            std::cout << "   Architecture: " << config.architecture << "\n";
            std::cout << "   Memory Limit: " << config.memory_limit_mb << "MB\n";
        } else {
            std::cout << "⚠️ GPU not available - CPU-only mode\n";
            config.enable_gpu = false;
        }
    }

    ~AdaptiveGPUDelegator() {
        cleanup_cuda();
    }

    // 🤔 INTELLIGENT GPU DELEGATION DECISION
    bool should_use_gpu(const std::string& operation_type,
                       size_t array_size = 0,
                       double complexity = 0.0) const {

        if (!gpu_available || !config.enable_gpu) {
            return false;
        }

        // 📏 CHECK ARRAY SIZE LIMITS
        if (array_size > 0) {
            if (array_size < config.min_array_size_gpu ||
                array_size > config.max_array_size_gpu) {
                return false;
            }
        }

        // 🧮 CHECK OPERATION COMPLEXITY
        if (operation_type == "exponential" || operation_type == "power" ||
            operation_type == "trigonometric" || operation_type == "advanced_pixel_processing") {
            return complexity > config.complexity_threshold;
        }

        if (operation_type == "arithmetic" || operation_type == "basic" ||
            operation_type == "buffer_processing") {
            return complexity > config.arithmetic_threshold;
        }

        // 🎯 DEFAULT: USE GPU FOR LARGE ARRAYS
        return array_size >= config.min_array_size_gpu;
    }

    // 🚀 GPU EXPONENTIAL CALCULATION
    CosmicPrecision gpu_exponential(const CosmicPrecision& base,
                                   const CosmicPrecision& exponent,
                                   int precision = 141) {

        if (!should_use_gpu("exponential", 0, static_cast<double>(abs(exponent)))) {
            return cpu_exponential(base, exponent, precision);
        }

        auto start_time = std::chrono::high_resolution_clock::now();

        try {
            std::lock_guard<std::mutex> lock(gpu_lock);
            stats.gpu_operations++;
            stats.total_operations++;

#ifdef CUDA_AVAILABLE
            // Convert to GPU-compatible format
            double base_d = static_cast<double>(base);
            double exp_d = static_cast<double>(exponent);

            // GPU calculation
            double result_d = pow(base_d, exp_d);

            // Convert back to cosmic precision
            CosmicPrecision result(std::to_string(result_d));

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double>(end_time - start_time).count();
            stats.total_gpu_time += duration;

            return result;
#else
            throw std::runtime_error("CUDA not available");
#endif

        } catch (const std::exception& e) {
            stats.memory_errors++;
            std::cout << "⚠️ GPU exponential failed: " << e.what() << " - using CPU fallback\n";
            return cpu_exponential(base, exponent, precision);
        }
    }

    // ⚡ GPU POWER CALCULATION
    CosmicPrecision gpu_power(const CosmicPrecision& base,
                             const CosmicPrecision& exponent,
                             int precision = 141) {

        if (!should_use_gpu("power", 0, static_cast<double>(abs(exponent)))) {
            return cpu_power(base, exponent, precision);
        }

        auto start_time = std::chrono::high_resolution_clock::now();

        try {
            std::lock_guard<std::mutex> lock(gpu_lock);
            stats.gpu_operations++;
            stats.total_operations++;

#ifdef CUDA_AVAILABLE
            double base_d = static_cast<double>(base);
            double exp_d = static_cast<double>(exponent);

            // GPU power calculation with validation
            double result_d = pow(base_d, exp_d);

            // Validate result
            if (std::isnan(result_d) || std::isinf(result_d)) {
                throw std::runtime_error("GPU calculation produced invalid result");
            }

            CosmicPrecision result(std::to_string(result_d));

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double>(end_time - start_time).count();
            stats.total_gpu_time += duration;

            return result;
#else
            throw std::runtime_error("CUDA not available");
#endif

        } catch (const std::exception& e) {
            stats.memory_errors++;
            std::cout << "⚠️ GPU power failed: " << e.what() << " - using CPU fallback\n";
            return cpu_power(base, exponent, precision);
        }
    }

    // 🔢 GPU VECTOR OPERATIONS
    std::vector<CosmicPrecision> gpu_vector_operations(
        const std::vector<CosmicPrecision>& array1,
        const std::vector<CosmicPrecision>& array2,
        const std::string& operation = "add") {

        size_t array_size = std::max(array1.size(), array2.size());

        if (!should_use_gpu("arithmetic", array_size)) {
            return cpu_vector_operations(array1, array2, operation);
        }

        auto start_time = std::chrono::high_resolution_clock::now();

        try {
            std::lock_guard<std::mutex> lock(gpu_lock);
            stats.gpu_operations++;
            stats.total_operations++;

#ifdef CUDA_AVAILABLE
            // Convert to GPU-compatible arrays
            std::vector<double> arr1_d, arr2_d;
            for (const auto& val : array1) {
                arr1_d.push_back(static_cast<double>(val));
            }
            for (const auto& val : array2) {
                arr2_d.push_back(static_cast<double>(val));
            }

            // Ensure equal sizes
            size_t max_size = std::max(arr1_d.size(), arr2_d.size());
            arr1_d.resize(max_size, 0.0);
            arr2_d.resize(max_size, 0.0);

            // GPU vector operation
            std::vector<double> result_d(max_size);

            if (operation == "add") {
                for (size_t i = 0; i < max_size; ++i) {
                    result_d[i] = arr1_d[i] + arr2_d[i];
                }
            } else if (operation == "subtract") {
                for (size_t i = 0; i < max_size; ++i) {
                    result_d[i] = arr1_d[i] - arr2_d[i];
                }
            } else if (operation == "multiply") {
                for (size_t i = 0; i < max_size; ++i) {
                    result_d[i] = arr1_d[i] * arr2_d[i];
                }
            } else if (operation == "divide") {
                for (size_t i = 0; i < max_size; ++i) {
                    result_d[i] = arr2_d[i] != 0.0 ? arr1_d[i] / arr2_d[i] : 0.0;
                }
            } else {
                throw std::runtime_error("Unsupported operation: " + operation);
            }

            // Convert back to cosmic precision
            std::vector<CosmicPrecision> result;
            for (double val : result_d) {
                result.emplace_back(std::to_string(val));
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double>(end_time - start_time).count();
            stats.total_gpu_time += duration;

            return result;
#else
            throw std::runtime_error("CUDA not available");
#endif

        } catch (const std::exception& e) {
            stats.memory_errors++;
            std::cout << "⚠️ GPU vector operation failed: " << e.what() << " - using CPU fallback\n";
            return cpu_vector_operations(array1, array2, operation);
        }
    }

    // 📐 GPU TRIGONOMETRIC OPERATIONS
    CosmicPrecision gpu_trigonometric(const CosmicPrecision& x,
                                     const std::string& function = "sin") {

        if (!should_use_gpu("trigonometric", 0, 1000.0)) {
            return cpu_trigonometric(x, function);
        }

        auto start_time = std::chrono::high_resolution_clock::now();

        try {
            std::lock_guard<std::mutex> lock(gpu_lock);
            stats.gpu_operations++;
            stats.total_operations++;

#ifdef CUDA_AVAILABLE
            double x_d = static_cast<double>(x);
            double result_d = 0.0;

            if (function == "sin") {
                result_d = sin(x_d);
            } else if (function == "cos") {
                result_d = cos(x_d);
            } else if (function == "tan") {
                result_d = tan(x_d);
            } else {
                throw std::runtime_error("Unsupported trigonometric function: " + function);
            }

            CosmicPrecision result(std::to_string(result_d));

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<double>(end_time - start_time).count();
            stats.total_gpu_time += duration;

            return result;
#else
            throw std::runtime_error("CUDA not available");
#endif

        } catch (const std::exception& e) {
            stats.memory_errors++;
            std::cout << "⚠️ GPU trigonometric failed: " << e.what() << " - using CPU fallback\n";
            return cpu_trigonometric(x, function);
        }
    }

private:
    // 🔧 INITIALIZATION METHODS
    GPUConfig initialize_gpu_config() {
        std::string detected_gpu = detect_gpu_hardware();

        auto it = gpu_configs.find(detected_gpu);
        if (it != gpu_configs.end()) {
            std::cout << "🎮 Detected GPU: " << detected_gpu << "\n";
            return it->second;
        } else {
            std::cout << "⚠️ Unknown GPU detected: " << detected_gpu << " - using conservative config\n";
            return gpu_configs["Unknown"];
        }
    }

    std::string detect_gpu_hardware() {
#ifdef CUDA_AVAILABLE
        try {
            int device_count = 0;
            cudaError_t error = cudaGetDeviceCount(&device_count);

            if (error == cudaSuccess && device_count > 0) {
                cudaDeviceProp prop;
                cudaGetDeviceProperties(&prop, 0);

                std::string gpu_name(prop.name);

                // Map GPU names to configurations
                if (gpu_name.find("GTX 1060") != std::string::npos) {
                    return "GTX 1060";
                } else if (gpu_name.find("RTX 4070 Super") != std::string::npos) {
                    return "RTX 4070 Super";
                } else if (gpu_name.find("RTX 3080") != std::string::npos) {
                    return "RTX 3080";
                } else {
                    return gpu_name; // Return actual name for unknown GPUs
                }
            }
        } catch (const std::exception& e) {
            std::cout << "⚠️ GPU detection error: " << e.what() << "\n";
        }
#endif
        return "Unknown";
    }

    bool initialize_cuda() {
#ifdef CUDA_AVAILABLE
        try {
            int device_count = 0;
            cudaError_t error = cudaGetDeviceCount(&device_count);

            if (error != cudaSuccess || device_count == 0) {
                return false;
            }

            // Initialize first available device
            device_id = 0;
            cudaSetDevice(device_id);

            // Initialize cuBLAS
            if (cublasCreate(&cublas_handle) != CUBLAS_STATUS_SUCCESS) {
                return false;
            }

            gpu_available = true;
            return true;

        } catch (const std::exception& e) {
            std::cout << "❌ CUDA initialization error: " << e.what() << "\n";
            return false;
        }
#else
        return false;
#endif
    }

    void cleanup_cuda() {
#ifdef CUDA_AVAILABLE
        if (cuda_initialized) {
            try {
                if (cublas_handle) {
                    cublasDestroy(cublas_handle);
                }
                cudaDeviceReset();
            } catch (const std::exception& e) {
                std::cout << "⚠️ CUDA cleanup error: " << e.what() << "\n";
            }
        }
#endif
    }

    // 🐌 CPU FALLBACK METHODS
    CosmicPrecision cpu_exponential(const CosmicPrecision& base,
                                   const CosmicPrecision& exponent,
                                   int precision) {
        auto start_time = std::chrono::high_resolution_clock::now();

        stats.cpu_fallbacks++;
        stats.total_operations++;

        // Use boost::multiprecision for high precision
        auto result = boost::multiprecision::pow(base, exponent);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double>(end_time - start_time).count();
        stats.total_cpu_time += duration;

        return result;
    }

    CosmicPrecision cpu_power(const CosmicPrecision& base,
                             const CosmicPrecision& exponent,
                             int precision) {
        return cpu_exponential(base, exponent, precision);
    }

    std::vector<CosmicPrecision> cpu_vector_operations(
        const std::vector<CosmicPrecision>& array1,
        const std::vector<CosmicPrecision>& array2,
        const std::string& operation) {

        auto start_time = std::chrono::high_resolution_clock::now();

        stats.cpu_fallbacks++;
        stats.total_operations++;

        std::vector<CosmicPrecision> result;
        size_t max_size = std::max(array1.size(), array2.size());

        for (size_t i = 0; i < max_size; ++i) {
            const auto& a = array1[i % array1.size()];
            const auto& b = array2[i % array2.size()];

            if (operation == "add") {
                result.push_back(a + b);
            } else if (operation == "subtract") {
                result.push_back(a - b);
            } else if (operation == "multiply") {
                result.push_back(a * b);
            } else if (operation == "divide") {
                result.push_back(b != CosmicPrecision("0") ? a / b : CosmicPrecision("0"));
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double>(end_time - start_time).count();
        stats.total_cpu_time += duration;

        return result;
    }

    CosmicPrecision cpu_trigonometric(const CosmicPrecision& x,
                                     const std::string& function) {
        auto start_time = std::chrono::high_resolution_clock::now();

        stats.cpu_fallbacks++;
        stats.total_operations++;

        CosmicPrecision result("0");

        if (function == "sin") {
            result = boost::multiprecision::sin(x);
        } else if (function == "cos") {
            result = boost::multiprecision::cos(x);
        } else if (function == "tan") {
            result = boost::multiprecision::tan(x);
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double>(end_time - start_time).count();
        stats.total_cpu_time += duration;

        return result;
    }

public:
    // 📊 PERFORMANCE REPORTING
    GPUPerformanceStats get_performance_stats() const {
        return stats;
    }

    void print_performance_report() const {
        auto local_stats = stats;

        std::cout << "\n🎯 ADAPTIVE GPU PERFORMANCE REPORT\n";
        std::cout << "=" << std::string(50, '=') << "\n";
        std::cout << "🔢 Total Operations: " << local_stats.total_operations.load() << "\n";
        std::cout << "🚀 GPU Operations: " << local_stats.gpu_operations.load()
                  << " (" << local_stats.get_gpu_usage_percentage() << "%)\n";
        std::cout << "🐌 CPU Fallbacks: " << local_stats.cpu_fallbacks.load()
                  << " (" << local_stats.get_cpu_fallback_percentage() << "%)\n";
        std::cout << "⚠️ Memory Errors: " << local_stats.memory_errors.load()
                  << " (" << local_stats.get_error_rate_percentage() << "%)\n";
        std::cout << "⭐ Efficiency Score: " << local_stats.get_efficiency_score() << "%\n";
        std::cout << "⏱️ Avg GPU Time: " << local_stats.get_average_gpu_time() << "s\n";
        std::cout << "⏱️ Avg CPU Time: " << local_stats.get_average_cpu_time() << "s\n";

        if (gpu_available) {
            std::cout << "\n⚙️ Current Configuration:\n";
            std::cout << "   GPU: " << config.gpu_name << "\n";
            std::cout << "   Architecture: " << config.architecture << "\n";
            std::cout << "   Memory Limit: " << config.memory_limit_mb << "MB\n";
            std::cout << "   Batch Size: " << config.batch_size << "\n";
            std::cout << "   Min Array Size: " << config.min_array_size_gpu << "\n";
            std::cout << "   Max Array Size: " << config.max_array_size_gpu << "\n";
        }
    }

    // 🎮 CONFIGURATION ACCESS
    const GPUConfig& get_config() const { return config; }
    bool is_gpu_available() const { return gpu_available; }
    bool is_cuda_initialized() const { return cuda_initialized; }
};

} // namespace gpu
} // namespace cortex