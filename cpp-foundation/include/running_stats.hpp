#pragma once
#include <cstdint>
#include <cmath>

namespace cortex {

    struct RunningStats {
        uint64_t n{0};
        double mean{0.0};
        double m2{0.0};

        void add(double x) {
            ++n;
            double delta = x - mean;
            mean += delta / static_cast<double>(n);
            double delta2 = x - mean;
            m2 += delta * delta2;
        }

        double variance() const {
            return (n > 1) ? (m2 / static_cast<double>(n - 1)) : 0.0;
        }

        double stddev() const {
            double v = variance();
            return v > 0.0 ? std::sqrt(v) : 0.0;
        }

        void reset() { n = 0; mean = 0.0; m2 = 0.0; }
    };

} // namespace cortex