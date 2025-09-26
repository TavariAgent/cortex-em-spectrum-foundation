#pragma once
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <thread>
#include <mutex>

namespace cortex {

    struct TuningResult {
        double score{ -std::numeric_limits<double>::infinity() };
        std::string label;
        std::vector<double> metrics; // user-defined meaning
    };

    class MicroParamTuner {
    public:
        using TrialFn = std::function<std::vector<double>(const std::string& cfg_label)>;
        using ScoreFn = std::function<double(const std::string&, const std::vector<double>&)>;

        static std::vector<TuningResult> run(const std::vector<std::string>& configs,
                                             TrialFn trial,
                                             ScoreFn score,
                                             bool parallel = false)
        {
            std::vector<TuningResult> out;
            out.reserve(configs.size());

            if (!parallel) {
                for (auto& cfg : configs) {
                    auto metrics = trial(cfg);
                    double s = score(cfg, metrics);
                    out.push_back({s, cfg, metrics});
                }
            } else {
                std::mutex mu;
                std::vector<std::thread> pool;
                for (auto& cfg : configs) {
                    pool.emplace_back([&out,&mu,&cfg,&trial,&score](){
                        auto metrics = trial(cfg);
                        double s = score(cfg, metrics);
                        std::lock_guard<std::mutex> lk(mu);
                        out.push_back({s, cfg, metrics});
                    });
                }
                for (auto& th : pool) th.join();
            }

            std::sort(out.begin(), out.end(),
                      [](const TuningResult& a, const TuningResult& b){
                          return a.score > b.score;
                      });
            return out;
        }
    };

} // namespace cortex