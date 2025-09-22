#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <cstdio>
#include <sstream>
#include <iomanip>
#include "mirror_cache.hpp"
#include "frame_recorder.hpp"
#include "cochat_bridge_win.hpp"

namespace cortex {

// Simple “speak” JSON generator
inline std::string make_speak_json(const std::string& role,
                                   const std::string& title,
                                   const std::string& message,
                                   const std::vector<std::string>& attachments = {}) {
    std::ostringstream ss;
    ss << "{"
       << "\"type\":\"speak\","
       << "\"role\":\"" << role << "\","
       << "\"title\":" << "\"" << title << "\","
       << "\"message\":" << "\"" << message << "\"";
    if (!attachments.empty()) {
        ss << ",\"attachments\":[";
        for (size_t i=0;i<attachments.size();++i) {
            if (i) ss << ",";
            ss << "\"" << attachments[i] << "\"";
        }
        ss << "]";
    }
    ss << "}";
    return ss.str();
}

// Save a short clip (sequence of BMPs) from the mirror cache and return paths
inline std::vector<std::string> save_clip_sequence(const MirrorCache& cache,
                                                   const std::string& base_dir,
                                                   const std::string& base_name,
                                                   int max_frames = 60) {
    std::filesystem::create_directories(base_dir);
    auto frames = cache.last_n((size_t)max_frames);
    std::vector<std::string> paths;
    paths.reserve(frames.size());
    int idx = 0;
    for (const auto& f : frames) {
        if (!f.ok()) continue;
        auto path = std::filesystem::path(base_dir) / make_numbered(base_name, idx++, ".bmp", 6);
        RawImageBMPView view{ f.bgra.data(), (size_t)f.width, (size_t)f.height };
        if (write_bmp32(path.string(), view)) paths.push_back(path.string());
    }
    return paths;
}

// Policy-driven trigger: call when you compute dynamic tile ratio or detect errors.
inline void on_scene_change(double dynamic_ratio, double k_percent_threshold,
                            const MirrorCache& cache, CoChatBridgeWin& bridge,
                            const std::string& clip_dir = "cochat_clips") {
    // If above threshold, speak with a 2-second clip (assuming ~30 FPS => 60 frames)
    if (dynamic_ratio > k_percent_threshold / 100.0) {
        auto attachments = save_clip_sequence(cache, clip_dir, "scene_change", 60);
        auto msg = make_speak_json(
            "system",
            "Scene updating",
            "Dynamic tiles exceeded K threshold; routing GPU chunks. Clip attached.",
            attachments
        );
        bridge.send_json(msg);
    }
}

// Generic error notifier (compiler/build/runtime hooks call this)
inline void on_error_detected(const std::string& err_text,
                              const MirrorCache& cache, CoChatBridgeWin& bridge,
                              const std::string& clip_dir = "cochat_clips") {
    auto attachments = save_clip_sequence(cache, clip_dir, "error_context", 30);
    auto msg = make_speak_json(
        "assistant",
        "Error detected",
        err_text,
        attachments
    );
    bridge.send_json(msg);
}

} // namespace cortex