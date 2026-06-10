#include "entropy/premerge/merge.hpp"
#include "entropy/premerge/partition_entropy.hpp"
#include "entropy/premerge/h1.hpp"
#include "entropy/premerge/types.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>

using namespace entropy::premerge;

// ---------------------------------------------------------------------------
// hp_merged: compute H^{P_M} for a fixed G_A and a partner EB/partB.
//
// Parameters:
//   EA, partA  : anchor-side graph and its partition (map<Node,Node>)
//   S          : anchor set (shared nodes)
//   EB         : partner edge list
//   partB      : B-private partition (must NOT contain anchors)
//
// Algorithm:
//   1. VB = all nodes appearing in EB, union S
//   2. VA = all nodes in EA union partA.keys
//   3. M  = merge(EA, VA, EB, VB)
//   4. pM: A-node n -> ("A", partA[n]); B-private n -> ("B", partB[n]);
//          anchor -> ("A", partA[n]) (folds onto A side)
//   5. Relabel distinct (side, id) pairs to consecutive ints
//   6. Return h_partition(M.EM, M.VM, pM_int)
// ---------------------------------------------------------------------------
static double hp_merged(
    const EdgeList& EA, const Partition& partA, const NodeSet& S,
    const EdgeList& EB, const Partition& partB)
{
    // --- build VA ---
    std::set<Node> vaSet;
    for (auto& [u, v] : EA) { vaSet.insert(u); vaSet.insert(v); }
    for (auto& [n, m] : partA) { vaSet.insert(n); (void)m; }
    NodeSet VA(vaSet.begin(), vaSet.end());

    // --- build VB = nodes in EB union S ---
    std::set<Node> vbSet;
    for (auto& [u, v] : EB) { vbSet.insert(u); vbSet.insert(v); }
    for (Node s : S) { vbSet.insert(s); }
    NodeSet VB(vbSet.begin(), vbSet.end());

    // --- merge ---
    auto M = merge(EA, VA, EB, VB);

    // --- set of anchors for quick lookup ---
    std::set<Node> anchorSet(S.begin(), S.end());

    // --- build merged partition using (side, module-id) labels ---
    // side: 0 = A side, 1 = B side
    std::map<std::pair<int, Node>, Node> labelMap;
    Node nextLabel = 0;
    auto getLabel = [&](int side, Node mod) -> Node {
        auto key = std::make_pair(side, mod);
        auto it = labelMap.find(key);
        if (it != labelMap.end()) return it->second;
        labelMap[key] = nextLabel;
        return nextLabel++;
    };

    Partition pM;
    for (Node n : M.VM) {
        if (anchorSet.count(n)) {
            // anchor folds onto A side
            Node mod = partA.at(n);
            pM[n] = getLabel(0, mod);
        } else if (partA.count(n)) {
            // A-private node
            Node mod = partA.at(n);
            pM[n] = getLabel(0, mod);
        } else {
            // B-private node
            Node mod = partB.at(n);
            pM[n] = getLabel(1, mod);
        }
    }

    return h_partition(M.EM, M.VM, pM);
}

// ---------------------------------------------------------------------------
// Fixed G_A used by M1, M3, M4int, M4vol
// ---------------------------------------------------------------------------
static const EdgeList GA_EA = {
    canon(0,1), canon(0,100), canon(1,101), canon(100,101), canon(0,101),
    canon(2,3), canon(2,102), canon(3,103), canon(102,103), canon(2,103),
    canon(1,2)
};
static const Partition GA_partA = {
    {0,0},{1,0},{100,0},{101,0},
    {2,1},{3,1},{102,1},{103,1}
};
static const NodeSet GA_S = {0, 1, 2, 3};

// ---------------------------------------------------------------------------
// Separate 6-anchor G_A used ONLY by M2
// ---------------------------------------------------------------------------
static const EdgeList GA6_EA = {
    canon(0,1), canon(1,2), canon(0,2), canon(0,100), canon(1,100),
    canon(3,4), canon(4,5), canon(3,5), canon(3,101), canon(4,101),
    canon(2,3)
};
static const Partition GA6_partA = {
    {0,0},{1,0},{2,0},{100,0},
    {3,1},{4,1},{5,1},{101,1}
};
static const NodeSet GA6_S = {0, 1, 2, 3, 4, 5};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
TEST_CASE("E4 irredundancy witnesses", "[witnesses]") {

    SECTION("M1: single B-node attaches to different anchors") {
        EdgeList EB_v1 = { canon(200, 0) };
        EdgeList EB_v2 = { canon(200, 2) };
        Partition partB = { {200, 5} };

        double h1 = hp_merged(GA_EA, GA_partA, GA_S, EB_v1, partB);
        double h2 = hp_merged(GA_EA, GA_partA, GA_S, EB_v2, partB);

        REQUIRE(std::abs(h1 - h2) > 1e-6);
        REQUIRE(h1 == Catch::Approx(2.188297).margin(1e-5));
        REQUIRE(h2 == Catch::Approx(2.173108).margin(1e-5));
    }

    SECTION("M2: 6-cycle vs two triangles on 6-anchor graph") {
        EdgeList EB_v1 = {
            canon(0,1), canon(1,2), canon(2,3), canon(3,4), canon(4,5), canon(5,0)
        };
        EdgeList EB_v2 = {
            canon(0,1), canon(1,2), canon(0,2), canon(3,4), canon(4,5), canon(3,5)
        };
        Partition partB = {};  // empty: all nodes are anchors

        double h1 = hp_merged(GA6_EA, GA6_partA, GA6_S, EB_v1, partB);
        double h2 = hp_merged(GA6_EA, GA6_partA, GA6_S, EB_v2, partB);

        REQUIRE(std::abs(h1 - h2) > 1e-6);
        REQUIRE(h1 == Catch::Approx(2.125815).margin(1e-5));
        REQUIRE(h2 == Catch::Approx(2.049452).margin(1e-5));
    }

    SECTION("M3: two B-nodes, different attachment patterns") {
        EdgeList EB_v1 = { canon(200,0), canon(201,0), canon(201,1), canon(201,2) };
        EdgeList EB_v2 = { canon(200,0), canon(200,1), canon(201,0), canon(201,2) };
        Partition partB = { {200, 5}, {201, 5} };

        double h1 = hp_merged(GA_EA, GA_partA, GA_S, EB_v1, partB);
        double h2 = hp_merged(GA_EA, GA_partA, GA_S, EB_v2, partB);

        REQUIRE(std::abs(h1 - h2) > 1e-6);
        REQUIRE(h1 == Catch::Approx(2.383605).margin(1e-5));
        REQUIRE(h2 == Catch::Approx(2.408767).margin(1e-5));
    }

    SECTION("M4int: internal B-subgraph topology change") {
        EdgeList EB_v1 = {
            canon(200,201), canon(202,203),
            canon(200,202), canon(201,203)
        };
        EdgeList EB_v2 = {
            canon(200,202), canon(200,203),
            canon(201,202), canon(201,203)
        };
        Partition partB = { {200, 5}, {201, 5}, {202, 6}, {203, 6} };

        double h1 = hp_merged(GA_EA, GA_partA, GA_S, EB_v1, partB);
        double h2 = hp_merged(GA_EA, GA_partA, GA_S, EB_v2, partB);

        REQUIRE(std::abs(h1 - h2) > 1e-6);
        REQUIRE(h1 == Catch::Approx(2.187014).margin(1e-5));
        REQUIRE(h2 == Catch::Approx(2.574600).margin(1e-5));
    }

    SECTION("M4vol: same EB structure, different B-partition assignment") {
        EdgeList EB_w = {
            canon(201,202), canon(201,0), canon(202,1),
            canon(203,204), canon(204,205), canon(203,205),
            canon(203,2),   canon(204,3),  canon(205,0),
            canon(200,0),   canon(200,1)
        };
        Partition partB_v1 = {
            {200, 5}, {201, 5}, {202, 5},
            {203, 6}, {204, 6}, {205, 6}
        };
        Partition partB_v2 = {
            {200, 6}, {201, 5}, {202, 5},
            {203, 6}, {204, 6}, {205, 6}
        };

        double h1 = hp_merged(GA_EA, GA_partA, GA_S, EB_w, partB_v1);
        double h2 = hp_merged(GA_EA, GA_partA, GA_S, EB_w, partB_v2);

        REQUIRE(std::abs(h1 - h2) > 1e-6);
        REQUIRE(h1 == Catch::Approx(2.529236).margin(1e-5));
        REQUIRE(h2 == Catch::Approx(2.542125).margin(1e-5));
    }
}
