#pragma once
#include <memory>
#include <vector>
#include <mutex>
#include "packed_frame_store.hpp"
#include "operand_map.hpp"

namespace cortex {

// A lightweight “parallel” timeline that decides: base vs patch.
// Not a drop-in replacement for LLMFramePool yet; keeps minimal logic.
class PackedFrameTimeline {
public:
    explicit PackedFrameTimeline(PackedFrameStore store,
                                 double fps_hint = 30.0)
        : store_(std::move(store)), fps_hint_(fps_hint) {}

    // Push a frame; returns store entry id
    uint32_t push(const std::shared_ptr<RawImage>& img,
                  int64_t index,
                  double tsec) {
        if (!img || !img->ok()) return UINT32_MAX;
        auto sig = sig::compute_operand_map(*img);

        std::lock_guard<std::mutex> lk(mu_);
        if (last_id_ == UINT32_MAX) {
            last_id_ = store_.add_base(img, sig, tsec, 1.0);
            last_sig_ = sig;
            prev_full_ = img;
            return last_id_;
        }

        // identical?
        if (sig::frames_identical(*img, *prev_full_, sig, last_sig_)) {
            // We treat identical as “no new entry” (coalescing can happen externally if needed)
            last_identical_run_++;
            return last_id_;
        }

        // Attempt patch vs previous base (NOT previous patch to keep chain length = 1)
        uint32_t base_id = last_base_id_;
        if (base_id == UINT32_MAX) base_id = last_id_; // fallback
        auto base_ptr = (base_id == last_id_) ? prev_full_ : prev_full_; // For Phase 3 base==prev_full_

        std::vector<TilePatch> patches;
        double coverage = store_.diff_and_patch(*prev_full_, *img, patches,
                                                /*allow_rgb565*/ true);

        size_t full_bytes = img->bgra.size();
        size_t patch_bytes = 0;
        for (auto& p : patches) patch_bytes += p.data.size();

        bool big_change = (coverage >= store_.cfg().big_change_cutoff);
        bool patch_ok   = !big_change &&
                          (patches.size() > 0) &&
                          (static_cast<double>(patch_bytes) / full_bytes <= store_.cfg().patch_coverage_cutoff);

        uint32_t id;
        if (patch_ok) {
            id = store_.add_patched(last_base_id_, std::move(patches), sig, tsec, coverage,
                                    img->width, img->height);
        } else {
            id = store_.add_base(img, sig, tsec, coverage);
            last_base_id_ = id;
        }

        last_id_ = id;
        last_sig_ = sig;
        prev_full_ = img;
        last_identical_run_ = 0;
        return id;
    }

    uint32_t last_id() const { return last_id_; }
    uint32_t last_base_id() const { return last_base_id_; }
    size_t total_bytes() const { return store_.total_bytes(); }
    const PackedFrameStore& store() const { return store_; }

private:
    mutable std::mutex mu_;
    PackedFrameStore store_;
    double fps_hint_;
    uint32_t last_id_{UINT32_MAX};
    uint32_t last_base_id_{UINT32_MAX};
    sig::OperandMap last_sig_{};
    std::shared_ptr<RawImage> prev_full_;
    uint64_t last_identical_run_{0};
};

} // namespace cortex