#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "entropy/exp_a/entropy2d.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

using namespace entropy;
using namespace entropy::exp_a;
using Catch::Approx;
using json = nlohmann::json;

static Graph build_named_graph(const std::string& name) {
    GraphBuilder b(GraphDirectedness::Undirected, GraphWeighting::Unweighted);
    if (name == "path_4") {
        b.reserve_nodes(4);
        b.add_edge(0,1); b.add_edge(1,2); b.add_edge(2,3);
    } else if (name == "cycle_4") {
        b.reserve_nodes(4);
        b.add_edge(0,1); b.add_edge(1,2); b.add_edge(2,3); b.add_edge(3,0);
    } else if (name == "K4") {
        b.reserve_nodes(4);
        for (int i = 0; i < 4; ++i) for (int j = i+1; j < 4; ++j) b.add_edge(i,j);
    } else if (name == "two_triangles_bridge") {
        // 0-1-2-0, 2-3, 3-4-5-3
        b.reserve_nodes(6);
        b.add_edge(0,1); b.add_edge(1,2); b.add_edge(0,2);
        b.add_edge(2,3);
        b.add_edge(3,4); b.add_edge(4,5); b.add_edge(3,5);
    } else {
        throw std::invalid_argument("Unknown graph: " + name);
    }
    return std::move(b).build(name);
}

TEST_CASE("C++ entropy values match Python golden reference", "[golden]") {
    // Reference file is copied to CMAKE_BINARY_DIR/tests/golden/ by CMake
    std::ifstream f("tests/golden/reference_values.json");
    if (!f.is_open()) {
        FAIL("Could not open tests/golden/reference_values.json — run generate_reference.py first");
    }
    json ref = json::parse(f);

    for (const auto& [graph_name, vals] : ref.items()) {
        CAPTURE(graph_name);
        Graph g = build_named_graph(graph_name);

        if (vals.contains("H1")) {
            double expected_H1 = vals["H1"].get<double>();
            REQUIRE(compute_H1(g) == Approx(expected_H1).epsilon(1e-9));
        }

        if (vals.contains("HP_single_block")) {
            // Single-block partition
            std::vector<int> part(g.num_nodes(), 0);
            double expected = vals["HP_single_block"].get<double>();
            REQUIRE(compute_HP(g, part) == Approx(expected).epsilon(1e-9));
        }

        if (vals.contains("H2_upper_bound")) {
            // H2 from single-block = H1 = upper bound
            double expected = vals["H2_upper_bound"].get<double>();
            H2Result result = compute_H2(g, 42);
            REQUIRE(result.h2_est <= expected + 1e-9);
        }
    }
}
