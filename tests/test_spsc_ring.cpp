#include "test_harness.hpp"
#include "spsc_ring.hpp"
#include <memory>

using namespace cortex;

TEST_CASE(SpscRing_push_pop_basic) {
    SpscRing<int> q(8);
    auto v1 = std::make_shared<int>(42);
    auto v2 = std::make_shared<int>(99);
    CHECK(q.push(v1));
    CHECK(q.push(v2));
    std::shared_ptr<int> out;
    CHECK(q.pop(out));
    REQUIRE(out && *out == 42);
    CHECK(q.pop(out));
    REQUIRE(out && *out == 99);
    CHECK(!q.pop(out)); // empty now
}