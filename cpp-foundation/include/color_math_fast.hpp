#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

namespace cortex {

struct RGBd { double r, g, b; };

// Small gamma LUT to avoid pow in the hot loop
class GammaLUT {
public:
    explicit GammaLUT(const double inv_gamma = 1.0/2.2, const int size = 4096)
        : inv_gamma_(inv_gamma), size_(std::max(16, size)), table_(size_ + 1, 0.0) {
        for (int i = 0; i <= size_; ++i) {
            const double x = static_cast<double>(i) / static_cast<double>(size_);
            table_[i] = std::pow(x, inv_gamma_);
        }
    }

    double apply(double v) const {
        if (v <= 0.0) return 0.0;
        if (v >= 1.0) return 1.0;
        const double f = v * size_;
        const int i = static_cast<int>(f);
        const double t = f - i;
        // Linear interpolate between two LUT entries
        return table_[i] + (table_[i+1] - table_[i]) * t;
    }

    void set_inv_gamma(double inv_gamma) {
        if (inv_gamma == inv_gamma_) return;
        inv_gamma_ = inv_gamma;
        for (int i = 0; i <= size_; ++i) {
            double x = static_cast<double>(i) / static_cast<double>(size_);
            table_[i] = std::pow(x, inv_gamma_);
        }
    }

private:
    double inv_gamma_;
    int size_;
    std::vector<double> table_;
};

// Fast wavelength->RGB in doubles, intensity + gamma via LUT
struct ColorMathFast {
    // piecewise base RGB (0..1) before intensity/gamma
    static void base_rgb(const double wl, double& r, double& g, double& b) {
        r = g = b = 0.0;
        if (wl >= 380.0 && wl < 440.0) { r = -(wl - 440.0) / 60.0; b = 1.0; }
        else if (wl >= 440.0 && wl < 490.0) { g = (wl - 440.0) / 50.0; b = 1.0; }
        else if (wl >= 490.0 && wl < 510.0) { g = 1.0; b = -(wl - 510.0) / 20.0; }
        else if (wl >= 510.0 && wl < 580.0) { r = (wl - 510.0) / 70.0; g = 1.0; }
        else if (wl >= 580.0 && wl < 645.0) { r = 1.0; g = -(wl - 645.0) / 65.0; }
        else if (wl >= 645.0 && wl <= 750.0) { r = 1.0; }
        r = std::clamp(r, 0.0, 1.0);
        g = std::clamp(g, 0.0, 1.0);
        b = std::clamp(b, 0.0, 1.0);
    }

    // intensity term from wavelength
    static double intensity(double wl) {
        double I = 1.0;
        if (wl >= 380.0 && wl < 420.0) {
            I = 0.3 + 0.7 * (wl - 380.0) / 40.0;
        } else if (wl >= 701.0 && wl <= 750.0) {
            I = 0.3 + 0.7 * (750.0 - wl) / 49.0;
        }
        return I;
    }

    static RGBd shade(double wl, const GammaLUT& gamma) {
        double r, g, b;
        base_rgb(wl, r, g, b);
        const double I = intensity(wl);
        r = gamma.apply(r * I);
        g = gamma.apply(g * I);
        b = gamma.apply(b * I);
        return { r, g, b };
    }
};

} // namespace cortex