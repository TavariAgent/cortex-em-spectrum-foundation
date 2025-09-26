#pragma once
#include <mutex>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace cortex {

    struct GuardMetricEvent {
        double t_wall{0.0};
        int    context_id{0};
        double duration{0.0};
        size_t mem_growth_bytes{0};
        double aggressiveness{1.0};
        bool   overflow{false};
        size_t recursive_depth{0};
    };

    class MetricsBus {
    public:
        explicit MetricsBus(const std::string& jsonl_path = "")
        {
            if (!jsonl_path.empty()) {
                file_.open(jsonl_path, std::ios::out | std::ios::binary);
            }
            t0_ = std::chrono::steady_clock::now();
        }

        void push(const GuardMetricEvent& ev) {
            std::lock_guard<std::mutex> lk(mu_);
            if (file_.is_open()) {
                file_ << "{"
                      << "\"t\":" << ev.t_wall << ","
                      << "\"context\":" << ev.context_id << ","
                      << "\"dur\":" << ev.duration << ","
                      << "\"mem_growth\":" << ev.mem_growth_bytes << ","
                      << "\"aggr\":" << ev.aggressiveness << ","
                      << "\"overflow\":" << (ev.overflow ? 1 : 0) << ","
                      << "\"depth\":" << ev.recursive_depth
                      << "}\n";
            }
        }

        double wall_seconds() const {
            auto now = std::chrono::steady_clock::now();
            return std::chrono::duration<double>(now - t0_).count();
        }

    private:
        std::ofstream file_;
        mutable std::mutex mu_;
        std::chrono::steady_clock::time_point t0_;
    };

} // namespace cortex