#include "entropy/premerge/merge.hpp"
#include "entropy/premerge/h1.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace entropy::premerge;

TEST_CASE("simple union dedups shared edges; W = 2|E_A union E_B|", "[merge]") {
    EdgeList EA = {{0,1},{1,2}}; NodeSet VA = {0,1,2};
    EdgeList EB = {{1,2},{2,3}}; NodeSet VB = {1,2,3};   // edge {1,2} shared
    auto M = merge(EA, VA, EB, VB);
    REQUIRE(M.EM.size() == 3);                            // {0,1},{1,2},{2,3}
    long W = 0; for (auto& [n,dn] : degrees(M.EM, M.VM)) W += dn;
    REQUIRE(W == 2 * static_cast<long>(M.EM.size()));     // volume-sum invariant
}
TEST_CASE("overlaps o_i counts edges present in BOTH among anchors", "[merge]") {
    EdgeList EA = {{0,1},{1,2}}; EdgeList EB = {{1,2},{2,3}};
    NodeSet S = {1,2};
    auto o = overlaps(EA, EB, S);
    REQUIRE(o[1] == 1); REQUIRE(o[2] == 1);               // shared edge {1,2}
}
