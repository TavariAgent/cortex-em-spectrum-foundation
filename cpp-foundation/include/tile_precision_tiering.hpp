#pragma once
#include <vector>
#include <cstdint>
#include <algorithm>

namespace cortex {

enum class TilePrecision : uint8_t { LOW8, MID16, HIGH32 };

struct TilePrecisionState {
    TilePrecision tier{TilePrecision::MID16};
    float recent_diff{0.f};
    uint32_t stable_frames{0};
};

class TilePrecisionRegistry {
public:
    void resize(size_t tiles) { state_.assign(tiles, {}); }

    TilePrecision update(size_t idx, float diff_value,
                         float promote_thr_high,
                         float demote_thr_low,
                         uint32_t stable_promote_frames = 4,
                         uint32_t stable_demote_frames  = 30)
    {
        auto& s = state_[idx];
        s.recent_diff = diff_value;

        switch (s.tier) {
            case TilePrecision::LOW8:
                if (diff_value > promote_thr_high) {
                    s.tier = TilePrecision::MID16;
                    s.stable_frames = 0;
                }
                break;
            case TilePrecision::MID16:
                if (diff_value > promote_thr_high) {
                    s.tier = TilePrecision::HIGH32;
                    s.stable_frames = 0;
                } else if (diff_value < demote_thr_low) {
                    if (++s.stable_frames >= stable_promote_frames) {
                        s.tier = TilePrecision::LOW8;
                        s.stable_frames = 0;
                    }
                } else {
                    s.stable_frames = 0;
                }
                break;
            case TilePrecision::HIGH32:
                if (diff_value < demote_thr_low) {
                    if (++s.stable_frames >= stable_demote_frames) {
                        s.tier = TilePrecision::MID16;
                        s.stable_frames = 0;
                    }
                } else {
                    s.stable_frames = 0;
                }
                break;
        }
        return s.tier;
    }

    const std::vector<TilePrecisionState>& data() const { return state_; }

private:
    std::vector<TilePrecisionState> state_;
};

} // namespace cortex