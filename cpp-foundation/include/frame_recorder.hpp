#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <cstdint>
#include <iomanip>
#include <sstream>

namespace cortex {

    struct RawImageBMPView {
        const uint8_t* bgra{nullptr};
        size_t width{0}, height{0};
        [[nodiscard]] bool ok() const { return bgra && width && height; }
    };

    inline bool write_bmp32(const std::string& filename, const RawImageBMPView& img) {
        if (!img.ok()) return false;
        const auto W = static_cast<uint32_t>(img.width);
        const auto H = static_cast<uint32_t>(img.height);
        const uint32_t stride = W * 4;
        const uint32_t pixelBytes = stride * H;
        const uint32_t fileSize = 14 + 40 + pixelBytes;

        std::ofstream out(filename, std::ios::binary);
        if (!out) return false;

        auto w16 = [&](uint16_t v){ out.write(reinterpret_cast<const char*>(&v), 2); };
        auto w32 = [&](uint32_t v){ out.write(reinterpret_cast<const char*>(&v), 4); };

        out.put('B'); out.put('M');
        w32(fileSize);
        w16(0); w16(0);
        w32(14 + 40);

        w32(40);
        w32(W);
        w32(H); // bottom-up
        w16(1);
        w16(32);
        w32(0);
        w32(pixelBytes);
        w32(2835);
        w32(2835);
        w32(0); w32(0);

        for (int y = static_cast<int>(H) - 1; y >= 0; --y) {
            const uint8_t* row = img.bgra + y * stride;
            out.write(reinterpret_cast<const char*>(row), stride);
        }
        return true;
    }

    inline std::string make_numbered(const std::string& base, int index, const char* ext = ".bmp", int pad = 6) {
        std::ostringstream ss;
        ss << base << "_" << std::setw(pad) << std::setfill('0') << index << ext;
        return ss.str();
    }

} // namespace cortex