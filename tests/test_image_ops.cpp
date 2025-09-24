#include "test_harness.hpp"
#include "image_ops.hpp"
#include "test_utils.hpp"

using namespace cortex;

TEST_CASE(Resize_bgra_bilinear_dims) {
    auto src = test_utils::make_bgra(16, 16, 0, 0, 0, 255);
    auto dst = resize_bgra_bilinear(src, 8, 8);
    REQUIRE(dst.ok());
    REQUIRE(dst.width == 8);
    REQUIRE(dst.height == 8);
}

TEST_CASE(Resize_bgra_bilinear_content_smoke) {
    auto src = test_utils::make_bgra(4, 4, 0, 0, 0, 255);
    // Paint a white 2x2 block at top-left
    for (size_t y = 0; y < 2; ++y) {
        for (size_t x = 0; x < 2; ++x) {
            test_utils::set_pixel(src, x, y, 255, 255, 255, 255);
        }
    }
    auto dst = resize_bgra_bilinear(src, 2, 2);
    REQUIRE(dst.ok());
    // Expect top-left stays bright-ish
    const uint8_t b = dst.bgra[0];
    const uint8_t g = dst.bgra[1];
    const uint8_t r = dst.bgra[2];
    CHECK(b >= 128 && g >= 128 && r >= 128);
}