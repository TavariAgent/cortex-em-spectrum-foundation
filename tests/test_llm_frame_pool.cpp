#include "test_harness.hpp"
#include "llm_frame_pool.hpp"
#include "test_utils.hpp"

using namespace cortex;

static std::shared_ptr<RawImage> mk_frame(size_t w, size_t h, uint8_t b, uint8_t g, uint8_t r) {
    auto img = std::make_shared<RawImage>(test_utils::make_bgra(w, h, b, g, r, 255));
    return img;
}

TEST_CASE(LLMFramePool_coalesce_identicals_and_quicklane) {
    LLMFramePool pool(300.0, 256, 30, 64);
    pool.set_single_static_mode(true, 0.1); // grace 100ms

    // Push initial frame at t=0
    auto f0 = mk_frame(8, 8, 10, 20, 30);
    pool.push(f0, 0, 0.0);

    // Push identical frames within grace: should extend run, no quick-lane pop
    for (int i = 1; i <= 3; ++i) {
        auto fi = mk_frame(8, 8, 10, 20, 30);
        pool.push(fi, i, i * 0.05); // 50ms apart
    }

    std::shared_ptr<LLMFramePool::Frame> out;
    // Only the first (unique) should be in quick-lane so far
    CHECK(pool.pop_quick(out) == true);
    CHECK(pool.pop_quick(out) == false);

    // After grace, continue static frames; still no quick-lane pushes
    for (int i = 4; i <= 8; ++i) {
        auto fi = mk_frame(8, 8, 10, 20, 30);
        pool.push(fi, i, 0.5 + i * 0.05);
    }
    CHECK(pool.pop_quick(out) == false);

    // Change content -> should push to quick-lane
    auto f1 = mk_frame(8, 8, 11, 20, 30);
    pool.push(f1, 9, 1.5);
    REQUIRE(pool.pop_quick(out) == true);
    REQUIRE(out && out->index == 9);

    // Snapshot recent should return at least one frame
    auto snap = pool.snapshot_recent(10.0);
    REQUIRE(!snap.empty());
    // The last item should reflect the change
    REQUIRE(snap.back().index == 9);
}

TEST_CASE(LLMFramePool_dynamic_retention_scrub_window) {
    // Keep only 0.4s of changes; budget sufficient
    LLMFramePool pool(0.4, 512, 30, 64);
    pool.set_single_static_mode(true, 0.0);

    // Unique frame at 0.0, another at 0.2, third at 0.6
    pool.push(mk_frame(4,4,1,2,3), 0, 0.0);
    pool.push(mk_frame(4,4,2,2,3), 1, 0.2);
    pool.push(mk_frame(4,4,3,2,3), 2, 0.6);

    // Recent 10s snapshot should include 0.2 and 0.6 (0.0 should be evicted by scrub window)
    auto snap = pool.snapshot_recent(10.0);
    REQUIRE(!snap.empty());
    CHECK(snap.front().index != 0);
}