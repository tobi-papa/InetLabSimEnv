#include <catch2/catch_test_macros.hpp>
#include "entropy/exp_a/trial.hpp"
#include <sstream>
#include <string>
#include <vector>

using namespace entropy::exp_a;

TEST_CASE("CSV header contains all required fields", "[csv_schema][test14]") {
    const std::string header = TrialRecord::to_csv_header();

    // All 87 required fields from spec §15
    const std::vector<std::string> required_fields = {
        "trial_id", "trial_seed", "mu_A", "mu_B", "n_shared", "n_A", "n_B",
        "k_A", "k_B", "avg_degree_A", "avg_degree_B", "p_in_A", "p_out_A",
        "p_in_B", "p_out_B",
        "rejected_draws_before_acceptance",
        "rejected_A_disconnected",
        "rejected_B_disconnected", "rejected_merge_disconnected",
        "nodes_A", "nodes_B", "nodes_merge",
        "edges_A", "edges_B", "edges_merge", "edge_overlap_AB", "shared_shared_edge_overlap",
        "actual_avg_degree_A", "actual_avg_degree_B", "actual_avg_degree_merge",
        "actual_mu_realized_B",
        "connected_A", "connected_B", "connected_merge",
        "H1_A", "H1_B", "H1_merge",
        "H2_est_A", "H2_est_B", "H2_est_merge",
        "SMI_est", "gain_oracle_est",
        "H1_merge_compact", "gain_1D_compact",
        "error_compact_abs", "error_compact_rel", "error_compact_norm_B",
        "gain_1D_oracle_degree",
        "error_oracle_degree_abs", "error_oracle_degree_rel", "error_oracle_degree_norm_B",
        "modularity_B_planted", "modularity_B_H2_partition",
        "H2_n_candidates_A", "H2_n_candidates_B", "H2_n_candidates_merge",
        "H2_score_best_A", "H2_score_best_B", "H2_score_best_merge",
        "H2_score_second_best_A", "H2_score_second_best_B", "H2_score_second_best_merge",
        "H2_best_minus_second_A", "H2_best_minus_second_B", "H2_best_minus_second_merge",
        "H2_n_unique_scores_A", "H2_n_unique_scores_B", "H2_n_unique_scores_merge",
        "H2_n_within_1e_minus_6_A", "H2_n_within_1e_minus_6_B", "H2_n_within_1e_minus_6_merge",
        "H2_n_within_1e_minus_4_A", "H2_n_within_1e_minus_4_B", "H2_n_within_1e_minus_4_merge",
        "H2_method_A", "H2_method_B", "H2_method_merge",
        "H2_seed_A", "H2_seed_B", "H2_seed_merge",
        "n_communities_H2_A", "n_communities_H2_B", "n_communities_H2_merge",
        "summary_compact_size",
        "sanity_passed", "sanity_warnings", "oracle_fragility_warning"
    };

    for (const auto& field : required_fields) {
        // Check that each field appears as a comma-delimited token
        // by searching for ",field," or "field," at start or ",field" at end
        bool found = (header == field) ||
                     (header.find("," + field + ",") != std::string::npos) ||
                     (header.size() >= field.size() + 1 &&
                      header.substr(0, field.size() + 1) == field + ",") ||
                     (header.size() >= field.size() + 1 &&
                      header.substr(header.size() - field.size() - 1) == "," + field);
        INFO("Missing field: " << field);
        REQUIRE(found);
    }
}

// Test that to_csv_row produces same number of columns as header
TEST_CASE("CSV row has same column count as header", "[csv_schema]") {
    TrialRecord r;
    r.trial_id = 1;
    r.trial_seed = 42;
    r.mu_B = 0.2;
    r.H2_method_A = "greedy";
    r.H2_method_B = "greedy";
    r.H2_method_merge = "greedy";

    auto count_commas = [](const std::string& s) {
        return std::count(s.begin(), s.end(), ',');
    };

    std::string header = TrialRecord::to_csv_header();
    std::string row = r.to_csv_row();

    REQUIRE(count_commas(header) == count_commas(row));
}
