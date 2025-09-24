#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "screen_capture_win.hpp" // RawImage

namespace cortex {
namespace sig { // nested namespace to avoid collision with static_frame_parallel.hpp

// Light-weight frame fingerprint built with "binary expression" style ops:
// - per-channel sums (add)
// - 32-bit word XOR (xor)
// - 64-bit FNV-1a rolling hash (xor+mul) over 32-bit words
struct OperandMap {
    uint64_t sumB{0}, sumG{0}, sumR{0}, sumA{0};
    uint64_t xor32{0};
    uint64_t fnv1a64{0};
    size_t   width{0}, height{0};
};

inline constexpr uint64_t fnv64_offset = 1469598103934665603ull;
inline constexpr uint64_t fnv64_prime  = 1099511628211ull;

// Compute an operand map for BGRA8 RawImage
inline OperandMap compute_operand_map(const RawImage& img) {
    OperandMap m{};
    if (!img.ok()) return m;
    m.width = img.width; m.height = img.height;

    // Walk 32-bit pixels (BGRA packed)
    const uint32_t* p32 = reinterpret_cast<const uint32_t*>(img.bgra.data());
    const size_t n32 = (img.bgra.size() / 4);

    uint64_t f = fnv64_offset;
    for (size_t i = 0; i < n32; ++i) {
        const uint32_t px = p32[i];
        m.xor32 ^= px;
        // FNV-1a over 4 bytes of px
        uint32_t v = px;
        for (int k = 0; k < 4; ++k) {
            const uint8_t b = static_cast<uint8_t>(v & 0xFFu);
            f ^= b;
            f *= fnv64_prime;
            v >>= 8;
        }
    }
    m.fnv1a64 = f;

    // Channel sums (8-bit)
    const size_t npx = img.width * img.height;
    for (size_t i = 0; i < npx; ++i) {
        const uint8_t* p = &img.bgra[i*4];
        m.sumB += p[0];
        m.sumG += p[1];
        m.sumR += p[2];
        m.sumA += p[3];
    }
    return m;
}

// Fast equality check on signatures
inline bool same_signature(const OperandMap& a, const OperandMap& b) {
    return a.width == b.width && a.height == b.height &&
           a.sumB == b.sumB && a.sumG == b.sumG && a.sumR == b.sumR && a.sumA == b.sumA &&
           a.xor32 == b.xor32 && a.fnv1a64 == b.fnv1a64;
}

// Confirm exact equality by byte-compare if signatures match
inline bool frames_identical(const RawImage& a, const RawImage& b,
                             const OperandMap& ma, const OperandMap& mb) {
    if (!a.ok() || !b.ok()) return false;
    if (!same_signature(ma, mb)) return false;
    if (a.width != b.width || a.height != b.height) return false;
    if (a.bgra.size() != b.bgra.size()) return false;
    return std::memcmp(a.bgra.data(), b.bgra.data(), a.bgra.size()) == 0;
}

} // namespace sig
} // namespace cortex