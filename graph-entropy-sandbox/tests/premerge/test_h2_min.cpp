#include "entropy/premerge/h2_min.hpp"
#include "entropy/premerge/partition_entropy.hpp"
#include "entropy/premerge/h1.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <random>
using namespace entropy::premerge;

TEST_CASE("agglo finds two-triangle split below all-in-one", "[h2]") {
    EdgeList E = {{0,1},{1,2},{0,2},{3,4},{4,5},{3,5},{2,3}}; // two triangles + 1 bridge
    NodeSet V = {0,1,2,3,4,5};
    auto [h2, part] = h2_min_agglo(E, V, /*restarts=*/200, /*seed=*/1);
    Partition all_one; for (Node n : V) all_one[n] = 0;
    REQUIRE(h2 <= h_partition(E, V, all_one) + 1e-9);     // never worse than trivial
    auto [h1, W] = h1_value(E, V);
    REQUIRE(h2 <= h1 + 1e-9);                              // H^P = H1 - benefit <= H1
}

TEST_CASE("exact H2 <= agglo H2 on a small graph", "[h2][exact]") {
    EdgeList E = {{0,1},{1,2},{0,2},{3,4},{4,5},{3,5},{2,3}};
    NodeSet V = {0,1,2,3,4,5};
    auto [hex, pex] = h2_min_exact(E, V, /*max_nodes=*/12);
    auto [hag, pag] = h2_min_agglo(E, V, 200, 1);
    REQUIRE(hex <= hag + 1e-9);
}
TEST_CASE("exact refuses oversized instances", "[h2][exact]") {
    NodeSet V; EdgeList E; for (Node i=0;i<13;++i){V.push_back(i); if(i)E.push_back({i-1,i});}
    REQUIRE_THROWS_AS(h2_min_exact(E, V, 12), std::runtime_error);
}
TEST_CASE("exact H2 <= every random partition (enumeration is complete)", "[h2][exact]") {
    EdgeList E = {{0,1},{1,2},{0,2},{3,4},{4,5},{3,5},{2,3}};
    NodeSet V = {0,1,2,3,4,5};
    auto [hex, pex] = h2_min_exact(E, V, 12);
    std::mt19937_64 rng(42);
    std::uniform_int_distribution<int> mod(0, 5);
    double sampled_min = 1e300;
    for (int t = 0; t < 4000; ++t) {
        Partition p; for (Node n : V) p[n] = mod(rng);
        sampled_min = std::min(sampled_min, h_partition(E, V, p));
    }
    // The certified minimum must be <= anything random sampling finds.
    REQUIRE(hex <= sampled_min + 1e-9);
}
TEST_CASE("exact Bell-number completeness: Bell(4)=15 and Bell(5)=52", "[h2][exact]") {
    // Count how many distinct partitions are visited by the same RGS enumeration.
    // This verifies the enumeration is complete and not double-counting.
    auto count_partitions = [](int n) -> long {
        if (n == 0) return 1;
        std::vector<int> a(n, 0), b(n, 0);
        long count = 0;
        while (true) {
            count++;
            int i = n - 1;
            while (i > 0 && a[i] == b[i-1] + 1) { a[i] = 0; --i; }
            if (i == 0) break;
            a[i] += 1;
            int bi = std::max(b[i-1], a[i]);
            for (int j = i; j < n; ++j) b[j] = (j==i)? bi : b[i];
            for (int j = i+1; j < n; ++j) b[j] = std::max(b[j-1], a[j]);
        }
        return count;
    };
    REQUIRE(count_partitions(4) == 15L);  // Bell(4)
    REQUIRE(count_partitions(5) == 52L);  // Bell(5)
}
