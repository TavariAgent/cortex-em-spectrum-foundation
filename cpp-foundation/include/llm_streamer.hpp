#pragma once
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <system_error>

#include "screen_capture_win.hpp" // RawImage

namespace cortex {
namespace fs = std::filesystem;

class LLMStreamer {
public:
    // If out_dir is empty: create llm_frames/YYYYMMDD_HHMMSS/
    explicit LLMStreamer(fs::path manifest_jsonl, fs::path out_dir = {})
        : manifest_path_(std::move(manifest_jsonl)) {

        if (out_dir.empty()) {
            out_dir_ = fs::path("llm_frames") / timestamp_now();
        } else {
            out_dir_ = std::move(out_dir);
        }
        std::error_code ec;
        fs::create_directories(out_dir_, ec);
        manifest_.open(manifest_path_, std::ios::out | std::ios::binary);
    }

    bool good() const { return manifest_.is_open(); }
    fs::path out_dir() const { return out_dir_; }

    // Write one BGRA8 frame losslessly and append a JSONL record
    bool push_bgra(const RawImage& img, int64_t frame_index, double ts_seconds, const std::string& tag = "") {
        if (!good() || !img.ok()) return false;

        const std::string base = "f" + pad6(frame_index);
        const fs::path raw_path = out_dir_ / (base + ".bgra");

        // Write raw bytes
        {
            std::ofstream raw(raw_path, std::ios::out | std::ios::binary);
            if (!raw) return false;
            raw.write(reinterpret_cast<const char*>(img.bgra.data()), static_cast<std::streamsize>(img.bgra.size()));
            raw.close();
        }

        // Append JSON line (path relative to manifest)
        manifest_ << "{"
                  << "\"path\":\"" << escape_json(fs::relative(raw_path, out_dir_.parent_path()).generic_string()) << "\","
                  << "\"w\":" << img.width << ","
                  << "\"h\":" << img.height << ","
                  << "\"format\":\"BGRA8\","
                  << "\"t\":" << ts_seconds << ","
                  << "\"tag\":\"" << escape_json(tag) << "\""
                  << "}\n";
        ++count_;
        return true;
    }

    size_t count() const { return count_; }

private:
    fs::path manifest_path_;
    fs::path out_dir_;
    std::ofstream manifest_;
    size_t count_{0};

    static std::string timestamp_now() {
        auto t = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(t);
        std::tm tm{};
    #ifdef _WIN32
        localtime_s(&tm, &tt);
    #else
        localtime_r(&tt, &tm);
    #endif
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
        return oss.str();
    }

    static std::string pad6(int64_t n) {
        std::ostringstream oss; oss << std::setfill('0') << std::setw(6) << n; return oss.str();
    }

    static std::string escape_json(const std::string& s) {
        std::string o; o.reserve(s.size()+8);
        for (char c : s) {
            switch (c) {
                case '\\': o += "\\\\"; break;
                case '"':  o += "\\\""; break;
                case '\n': o += "\\n";  break;
                case '\r': o += "\\r";  break;
                case '\t': o += "\\t";  break;
                default:   o += c;      break;
            }
        }
        return o;
    }
};

} // namespace cortex