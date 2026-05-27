#pragma once
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace entropy::exp_a {

// Grid constants (spec §6.5) — fixed before running.
constexpr int    GRID_N_A         = 300;
constexpr int    GRID_N_B         = 300;
constexpr int    GRID_K_A         = 4;
constexpr int    GRID_K_B         = 4;
constexpr double GRID_AVG_DEG_A   = 10.0;
constexpr double GRID_AVG_DEG_B   = 10.0;
constexpr double GRID_MU_A        = 0.20;
constexpr int    GRID_REPS        = 20;

inline const std::vector<double> GRID_MU_B_VALUES = {
    0.05, 0.075, 0.10, 0.15, 0.20, 0.30, 0.40, 0.50, 0.60, 0.70, 0.75
};
inline const std::vector<int> GRID_N_SHARED_VALUES = {15, 30, 90};

constexpr double NaN_d = std::numeric_limits<double>::quiet_NaN();

struct TrialRecord {
    // --- Identifiers ---
    int      trial_id      = 0;
    uint64_t trial_seed    = 0;

    // --- Nominal parameters ---
    double mu_A       = 0.0;
    double mu_B       = 0.0;
    int    n_shared   = 0;
    int    n_A        = 0;
    int    n_B        = 0;
    int    k_A        = 0;
    int    k_B        = 0;
    double avg_degree_A = 0.0;
    double avg_degree_B = 0.0;
    double p_in_A     = 0.0;
    double p_out_A    = 0.0;
    double p_in_B     = 0.0;
    double p_out_B    = 0.0;

    // --- Rejection counts ---
    int rejected_draws_before_acceptance = 0;
    int rejected_A_disconnected          = 0;
    int rejected_B_disconnected          = 0;
    int rejected_merge_disconnected      = 0;

    // --- Graph sizes ---
    int nodes_A     = 0;
    int nodes_B     = 0;
    int nodes_merge = 0;
    int64_t edges_A     = 0;
    int64_t edges_B     = 0;
    int64_t edges_merge = 0;
    int64_t edge_overlap_AB         = 0;
    int64_t shared_shared_edge_overlap = 0;

    // --- Realized stats ---
    double actual_avg_degree_A     = 0.0;
    double actual_avg_degree_B     = 0.0;
    double actual_avg_degree_merge = 0.0;
    double actual_mu_realized_B    = 0.0;

    // --- Connectivity ---
    bool connected_A     = false;
    bool connected_B     = false;
    bool connected_merge = false;

    // --- 1D entropy ---
    double H1_A     = 0.0;
    double H1_B     = 0.0;
    double H1_merge = 0.0;

    // --- 2D entropy estimates ---
    double H2_est_A     = 0.0;
    double H2_est_B     = 0.0;
    double H2_est_merge = 0.0;

    // --- Target ---
    double SMI_est        = 0.0;  // H2_est_A + H2_est_B - H2_est_merge
    double gain_oracle_est = 0.0; // H2_est_merge - H2_est_A

    // --- Compact estimator ---
    double H1_merge_compact    = 0.0;
    double gain_1D_compact     = 0.0;
    double error_compact_abs   = 0.0;
    double error_compact_rel   = NaN_d;
    double error_compact_norm_B = NaN_d;

    // --- Oracle 1D degree ---
    double gain_1D_oracle_degree      = 0.0;
    double error_oracle_degree_abs    = 0.0;
    double error_oracle_degree_rel    = NaN_d;
    double error_oracle_degree_norm_B = NaN_d;

    // --- Modularity ---
    double modularity_B_planted      = 0.0;
    double modularity_B_H2_partition = 0.0;

    // --- H2 oracle diagnostics for A ---
    int    H2_n_candidates_A     = 0;
    double H2_score_best_A       = 0.0;
    double H2_score_second_best_A = 0.0;
    double H2_best_minus_second_A = 0.0;
    int    H2_n_unique_scores_A  = 0;
    int    H2_n_within_1e_minus_6_A = 0;
    int    H2_n_within_1e_minus_4_A = 0;

    // --- H2 oracle diagnostics for B ---
    int    H2_n_candidates_B     = 0;
    double H2_score_best_B       = 0.0;
    double H2_score_second_best_B = 0.0;
    double H2_best_minus_second_B = 0.0;
    int    H2_n_unique_scores_B  = 0;
    int    H2_n_within_1e_minus_6_B = 0;
    int    H2_n_within_1e_minus_4_B = 0;

    // --- H2 oracle diagnostics for merge ---
    int    H2_n_candidates_merge     = 0;
    double H2_score_best_merge       = 0.0;
    double H2_score_second_best_merge = 0.0;
    double H2_best_minus_second_merge = 0.0;
    int    H2_n_unique_scores_merge  = 0;
    int    H2_n_within_1e_minus_6_merge = 0;
    int    H2_n_within_1e_minus_4_merge = 0;

    // --- H2 method/seed/community count ---
    std::string H2_method_A, H2_method_B, H2_method_merge;
    uint64_t    H2_seed_A = 0, H2_seed_B = 0, H2_seed_merge = 0;
    int         n_communities_H2_A = 0, n_communities_H2_B = 0, n_communities_H2_merge = 0;

    // --- Compact summary size ---
    int summary_compact_size = 0;

    // --- Sanity flags ---
    bool   sanity_passed          = true;
    std::string sanity_warnings;
    bool   oracle_fragility_warning = false;

    // CSV serialization
    static std::string to_csv_header();
    std::string        to_csv_row() const;
};

} // namespace entropy::exp_a
