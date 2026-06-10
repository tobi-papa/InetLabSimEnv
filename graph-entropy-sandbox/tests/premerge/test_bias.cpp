#include "entropy/premerge/bias.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
using namespace entropy::premerge;
TEST_CASE("bias bounds: sigma and seam bound", "[bias]") {
    // vol(S)=9, W=84 -> sigma=0.107..., seam=2*sigma*log2(84)
    std::map<Node,long> dM = {{1,9}}; NodeSet S={1}; long W=84;
    auto b = bias_bounds(S, dM, W);
    REQUIRE(b.sigma == Catch::Approx(9.0/84.0));
    REQUIRE(b.seam_bound == Catch::Approx(2.0*(9.0/84.0)*std::log2(84.0)));
    REQUIRE(b.renorm_cap == Catch::Approx(1.06).margin(0.01));
}
