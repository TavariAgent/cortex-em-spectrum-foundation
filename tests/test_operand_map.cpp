#include "test_harness.hpp"
#include "test_utils.hpp"
#include "operand_map.hpp"

using namespace cortex;

TEST_CASE(OperandMap_identical_and_different) {
    auto a = test_utils::make_bgra(8, 8, 10, 20, 30, 255);
    auto b = a; // identical copy
    REQUIRE(a.ok());
    REQUIRE(b.ok());

    auto sa = sig::compute_operand_map(a);
    auto sb = sig::compute_operand_map(b);
    REQUIRE(sig::same_signature(sa, sb));
    REQUIRE(sig::frames_identical(a, b, sa, sb));

    // Change one pixel
    test_utils::set_pixel(b, 3, 4, 11, 20, 30, 255);
    auto sb2 = sig::compute_operand_map(b);
    CHECK(!sig::same_signature(sa, sb2) || !sig::frames_identical(a, b, sa, sb2));
}