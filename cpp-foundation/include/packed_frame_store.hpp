#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <string>
#include "screen_capture_win.hpp"
#include "operand_map.hpp"
#include "rgb565.hpp"

namespace cortex {

enum class PackedStorageMode : uint8_t {
    BASE_BGRA32,
    PATCHED,        // patches vector
    BASE_RGB565,    // (future: if we demote bases)
    BASE_COMPRESSED // placeholder for Phase 5 (Zstd)
};

enum class PatchPixelFormat : uint8_t {
    BGRA32,
    RGB565
};

struct TilePatch {
    uint32_t tile_index;
    uint16_t w;
    uint16_t h;
    PatchPixelFormat fmt;
    std::vector<uint8_t> data;   // BGRA or RGB565 tile
};

struct PackedEntry {
    PackedStorageMode mode{PackedStorageMode::BASE_BGRA32};
    uint32_t base_ref{0};            // valid if PATCHED
    std::shared_ptr<RawImage> full;  // only for base entries (BGRA32 or future demoted)
    std::vector<TilePatch> patches;  // only if PATCHED
    sig::OperandMap signature{};
    double tsec{0.0};
    double diff_coverage{0.0};
    size_t bytes{0};
};

struct PatchConfig {
    size_t tile_w{64};
    size_t tile_h{32};
    double patch_coverage_cutoff{0.55};  // if patch_bytes/full_bytes <= cutoff -> patch
    double big_change_cutoff{0.35};       // if diff_coverage >= this -> force new base
    bool   allow_rgb565{true};
    double rgb565_diff_coverage_promote{0.20}; // if diff_coverage <= this & allow_rgb565 -> tile(s) may use 565
};

class PackedFrameStore {
public:
    explicit PackedFrameStore(uint64_t budget_bytes = 512ull * 1024ull * 1024ull,
                              PatchConfig cfg = {})
        : budget_bytes_(budget_bytes), cfg_(cfg) {}

    uint32_t add_base(std::shared_ptr<RawImage> img,
                      const sig::OperandMap& sig,
                      double tsec,
                      double diff_coverage = 1.0) {
        if (!img || !img->ok()) return UINT32_MAX;
        std::lock_guard<std::mutex> lk(mu_);
        PackedEntry e;
        e.mode = PackedStorageMode::BASE_BGRA32;
        e.full = std::move(img);
        e.signature = sig;
        e.tsec = tsec;
        e.diff_coverage = diff_coverage;
        e.bytes = e.full->bgra.size();
        entries_.push_back(std::move(e));
        total_bytes_ += entries_.back().bytes;
        evict_if_needed_unlocked();
        return static_cast<uint32_t>(entries_.size() - 1);
    }

    uint32_t add_patched(uint32_t base_id,
                         std::vector<TilePatch>&& patches,
                         const sig::OperandMap& sig,
                         double tsec,
                         double diff_coverage,
                         size_t full_w,
                         size_t full_h) {
        if (base_id >= entries_.size()) return UINT32_MAX;
        size_t patch_bytes = 0;
        for (auto& p : patches) patch_bytes += p.data.size();
        PackedEntry e;
        e.mode = PackedStorageMode::PATCHED;
        e.base_ref = base_id;
        e.patches = std::move(patches);
        e.signature = sig;
        e.tsec = tsec;
        e.diff_coverage = diff_coverage;
        e.bytes = patch_bytes;
        std::lock_guard<std::mutex> lk(mu_);
        entries_.push_back(std::move(e));
        total_bytes_ += patch_bytes;
        evict_if_needed_unlocked();
        return static_cast<uint32_t>(entries_.size() - 1);
    }

    // Reconstruct as BGRA32 (scratch reused by caller).
    // Returns shared_ptr (may share underlying base image).
    std::shared_ptr<RawImage> reconstruct(uint32_t id, std::shared_ptr<RawImage> scratch = {}) {
        std::lock_guard<std::mutex> lk(mu_);
        if (id >= entries_.size()) return {};
        PackedEntry& e = entries_[id];
        if (e.mode == PackedStorageMode::BASE_BGRA32 && e.full) return e.full;

        // Only PATCHED supported now
        if (e.mode != PackedStorageMode::PATCHED) return {};
        if (e.base_ref >= entries_.size()) return {};

        PackedEntry& b = entries_[e.base_ref];
        if (!b.full) return {}; // (future: if compressed, decompress here)

        // Ensure scratch
        if (!scratch) scratch = std::make_shared<RawImage>(*b.full); // deep copy
        else {
            // Deep copy into provided scratch
            scratch->width = b.full->width;
            scratch->height = b.full->height;
            scratch->bgra = b.full->bgra;
        }

        // Apply patches
        size_t gx = (scratch->width  + cfg_.tile_w - 1) / cfg_.tile_w;
        for (auto& p : e.patches) {
            size_t tile_index = p.tile_index;
            size_t ty = tile_index / gx;
            size_t tx = tile_index % gx;
            size_t x = tx * cfg_.tile_w;
            size_t y = ty * cfg_.tile_h;
            if (x >= scratch->width || y >= scratch->height) continue;
            size_t w = std::min<size_t>(p.w, scratch->width - x);
            size_t h = std::min<size_t>(p.h, scratch->height - y);
            uint8_t* dst = &scratch->bgra[(y * scratch->width + x) * 4];
            if (p.fmt == PatchPixelFormat::BGRA32) {
                for (size_t row = 0; row < h; ++row) {
                    std::memcpy(&dst[row * scratch->width * 4],
                                &p.data[row * w * 4],
                                w * 4);
                }
            } else {
                for (size_t row = 0; row < h; ++row) {
                    rgb565_tile_to_bgra(&p.data[row * w * 2], w, h, dst, scratch->width * 4);
                    break; // we converted whole tile in one call above - restructure for clarity
                }
            }
        }
        return scratch;
    }

    size_t total_bytes() const {
        return total_bytes_.load();
    }

    size_t count() const {
        std::lock_guard<std::mutex> lk(mu_);
        return entries_.size();
    }

    const PatchConfig& cfg() const { return cfg_; }

    // Extract patches (public helper so caller can decide add_base vs add_patched)
    // Returns diff_coverage (changed_tiles/total_tiles).
    double diff_and_patch(const RawImage& prev,
                          const RawImage& curr,
                          std::vector<TilePatch>& out_patches,
                          bool allow_rgb565 = true) const {
        out_patches.clear();
        if (!prev.ok() || !curr.ok() ||
            prev.width != curr.width || prev.height != curr.height)
            return 1.0;

        const size_t tile_w = cfg_.tile_w;
        const size_t tile_h = cfg_.tile_h;
        size_t gx = (curr.width  + tile_w - 1) / tile_w;
        size_t gy = (curr.height + tile_h - 1) / tile_h;
        size_t changed_tiles = 0;

        for (size_t ty = 0; ty < gy; ++ty) {
            for (size_t tx = 0; tx < gx; ++tx) {
                size_t x = tx * tile_w;
                size_t y = ty * tile_h;
                size_t w = std::min(tile_w, curr.width  - x);
                size_t h = std::min(tile_h, curr.height - y);
                if (tile_changed(prev, curr, x, y, w, h)) {
                    ++changed_tiles;
                    TilePatch patch;
                    patch.tile_index = static_cast<uint32_t>(ty * gx + tx);
                    patch.w = static_cast<uint16_t>(w);
                    patch.h = static_cast<uint16_t>(h);

                    // Decide format
                    if (allow_rgb565 && cfg_.allow_rgb565) {
                        bgra_tile_to_rgb565(&curr.bgra[(y * curr.width + x) * 4],
                                            w, h, curr.width * 4, patch.data);
                        patch.fmt = PatchPixelFormat::RGB565;
                    } else {
                        patch.data.resize(w * h * 4);
                        for (size_t row = 0; row < h; ++row) {
                            std::memcpy(&patch.data[row * w * 4],
                                        &curr.bgra[((y + row) * curr.width + x) * 4],
                                        w * 4);
                        }
                        patch.fmt = PatchPixelFormat::BGRA32;
                    }
                    out_patches.push_back(std::move(patch));
                }
            }
        }
        double coverage = (gx * gy) ? static_cast<double>(changed_tiles) / static_cast<double>(gx * gy) : 1.0;
        return coverage;
    }

private:
    mutable std::mutex mu_;
    std::vector<PackedEntry> entries_;
    std::atomic<size_t> total_bytes_{0};
    uint64_t budget_bytes_;
    PatchConfig cfg_;

    static bool tile_changed(const RawImage& prev, const RawImage& curr,
                             size_t x, size_t y, size_t w, size_t h) {
        for (size_t yy = 0; yy < h; ++yy) {
            size_t row = y + yy;
            const uint8_t* a = &prev.bgra[(row * prev.width + x) * 4];
            const uint8_t* b = &curr.bgra[(row * curr.width + x) * 4];
            if (std::memcmp(a, b, w * 4) != 0) return true;
        }
        return false;
    }

    void evict_if_needed_unlocked() {
        while (entries_.size() > 2 && total_bytes_.load() > budget_bytes_) {
            // Drop oldest base or patch but keep at least one base + latest
            size_t remove_index = 0;
            total_bytes_ -= entries_[remove_index].bytes;
            entries_.erase(entries_.begin());
            // NOTE: This invalidates base_ref indices. Phase 3 simplification: we will
            // only run with moderate retention or not evict aggressively.
            // Phase 4: introduce stable ID indirection (index map) before real eviction.
            break; // avoid breaking refs for now
        }
    }
};

} // namespace cortex