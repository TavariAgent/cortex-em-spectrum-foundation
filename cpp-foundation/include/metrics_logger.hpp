#pragma once
#include <fstream>
#include <mutex>
#include <string>
#include <chrono>
#include <iomanip>

namespace cortex {

    class MetricsLogger {
    public:
        explicit MetricsLogger(const std::string& path = "")
        {
            if (!path.empty()) {
                file_.open(path, std::ios::out | std::ios::binary);
            }
            t0_ = std::chrono::steady_clock::now();
        }

        bool ok() const { return file_.is_open(); }

        double wall_seconds() const {
            auto now = std::chrono::steady_clock::now();
            return std::chrono::duration<double>(now - t0_).count();
        }

        // Writes a raw JSON line (assumed already valid)
        void write_raw(const std::string& json_line) {
            if (!ok()) return;
            std::lock_guard<std::mutex> lk(mu_);
            file_ << json_line << "\n";
        }

        template<class Fn>
        void write_object(Fn&& fn) {
            if (!ok()) return;
            std::lock_guard<std::mutex> lk(mu_);
            file_ << "{";
            fn(file_);
            file_ << "}\n";
        }

    private:
        std::ofstream file_;
        mutable std::mutex mu_;
        std::chrono::steady_clock::time_point t0_;
    };

} // namespace cortex