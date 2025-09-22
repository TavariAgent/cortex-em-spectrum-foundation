#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <optional>
#include "calibration_10frame.hpp"

namespace cortex {

    // Very small JSON-ish writer/reader (no external deps)
    inline bool save_calibration_json(const std::string& path, const CalibrationParams& p) {
        std::ofstream out(path, std::ios::binary);
        if (!out) return false;
        out << "{\n";
        out << "  \"gain_r\": " << std::setprecision(10) << p.gain_r << ",\n";
        out << "  \"gain_g\": " << std::setprecision(10) << p.gain_g << ",\n";
        out << "  \"gain_b\": " << std::setprecision(10) << p.gain_b << ",\n";
        out << "  \"gamma\": "  << std::setprecision(10) << p.gamma  << ",\n";
        out << "  \"avg_luma\": " << std::setprecision(10) << p.avg_luma << "\n";
        out << "}\n";
        return true;
    }

    inline std::optional<CalibrationParams> load_calibration_json(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return std::nullopt;
        std::stringstream buf; buf << in.rdbuf();
        std::string s = buf.str();

        auto find_val = [&](const char* key)->std::optional<double> {
            auto pos = s.find(key);
            if (pos == std::string::npos) return std::nullopt;
            pos = s.find(':', pos);
            if (pos == std::string::npos) return std::nullopt;
            size_t end = s.find_first_of(",}\n", pos+1);
            if (end == std::string::npos) end = s.size();
            try {
                double v = std::stod(s.substr(pos+1, end - (pos+1)));
                return v;
            } catch (...) { return std::nullopt; }
        };

        CalibrationParams p;
        auto gr = find_val("\"gain_r\"");
        auto gg = find_val("\"gain_g\"");
        auto gb = find_val("\"gain_b\"");
        auto gm = find_val("\"gamma\"");
        auto al = find_val("\"avg_luma\"");
        if (!gr || !gg || !gb || !gm || !al) return std::nullopt;
        p.gain_r = *gr; p.gain_g = *gg; p.gain_b = *gb; p.gamma = *gm; p.avg_luma = *al;
        return p;
    }

} // namespace cortex