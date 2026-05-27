#include "entropy/exp_a/trial.hpp"
#include <cmath>
#include <sstream>
#include <string>

namespace entropy::exp_a {

std::string TrialRecord::to_csv_header() {
    return
        "trial_id,trial_seed,"
        "mu_A,mu_B,n_shared,n_A,n_B,k_A,k_B,avg_degree_A,avg_degree_B,"
        "p_in_A,p_out_A,p_in_B,p_out_B,"
        "rejected_draws_before_acceptance,rejected_A_disconnected,"
        "rejected_B_disconnected,rejected_merge_disconnected,"
        "nodes_A,nodes_B,nodes_merge,"
        "edges_A,edges_B,edges_merge,"
        "edge_overlap_AB,shared_shared_edge_overlap,"
        "actual_avg_degree_A,actual_avg_degree_B,actual_avg_degree_merge,"
        "actual_mu_realized_B,"
        "connected_A,connected_B,connected_merge,"
        "H1_A,H1_B,H1_merge,"
        "H2_est_A,H2_est_B,H2_est_merge,"
        "SMI_est,gain_oracle_est,"
        "H1_merge_compact,gain_1D_compact,"
        "error_compact_abs,error_compact_rel,error_compact_norm_B,"
        "gain_1D_oracle_degree,"
        "error_oracle_degree_abs,error_oracle_degree_rel,error_oracle_degree_norm_B,"
        "modularity_B_planted,modularity_B_H2_partition,"
        "H2_n_candidates_A,H2_n_candidates_B,H2_n_candidates_merge,"
        "H2_score_best_A,H2_score_best_B,H2_score_best_merge,"
        "H2_score_second_best_A,H2_score_second_best_B,H2_score_second_best_merge,"
        "H2_best_minus_second_A,H2_best_minus_second_B,H2_best_minus_second_merge,"
        "H2_n_unique_scores_A,H2_n_unique_scores_B,H2_n_unique_scores_merge,"
        "H2_n_within_1e_minus_6_A,H2_n_within_1e_minus_6_B,H2_n_within_1e_minus_6_merge,"
        "H2_n_within_1e_minus_4_A,H2_n_within_1e_minus_4_B,H2_n_within_1e_minus_4_merge,"
        "H2_method_A,H2_method_B,H2_method_merge,"
        "H2_seed_A,H2_seed_B,H2_seed_merge,"
        "n_communities_H2_A,n_communities_H2_B,n_communities_H2_merge,"
        "summary_compact_size,"
        "sanity_passed,sanity_warnings,oracle_fragility_warning";
}

std::string TrialRecord::to_csv_row() const {
    std::ostringstream ss;
    ss.precision(15);

    // Helper: format a double with 15 sig digits, or "nan" for NaN
    auto d = [](double v) -> std::string {
        if (std::isnan(v)) return "nan";
        std::ostringstream t;
        t.precision(15);
        t << v;
        return t.str();
    };

    // Sanitize sanity_warnings: replace any commas with semicolons
    std::string sw = sanity_warnings;
    for (char& c : sw) {
        if (c == ',') c = ';';
    }

    // --- Identifiers ---
    ss << trial_id << "," << trial_seed << ",";

    // --- Nominal parameters ---
    ss << d(mu_A) << "," << d(mu_B) << ","
       << n_shared << "," << n_A << "," << n_B << ","
       << k_A << "," << k_B << ","
       << d(avg_degree_A) << "," << d(avg_degree_B) << ","
       << d(p_in_A) << "," << d(p_out_A) << ","
       << d(p_in_B) << "," << d(p_out_B) << ",";

    // --- Rejection counts ---
    ss << rejected_draws_before_acceptance << ","
       << rejected_A_disconnected << ","
       << rejected_B_disconnected << ","
       << rejected_merge_disconnected << ",";

    // --- Graph sizes ---
    ss << nodes_A << "," << nodes_B << "," << nodes_merge << ","
       << edges_A << "," << edges_B << "," << edges_merge << ","
       << edge_overlap_AB << "," << shared_shared_edge_overlap << ",";

    // --- Realized stats ---
    ss << d(actual_avg_degree_A) << ","
       << d(actual_avg_degree_B) << ","
       << d(actual_avg_degree_merge) << ","
       << d(actual_mu_realized_B) << ",";

    // --- Connectivity ---
    ss << (connected_A     ? "1" : "0") << ","
       << (connected_B     ? "1" : "0") << ","
       << (connected_merge ? "1" : "0") << ",";

    // --- 1D entropy ---
    ss << d(H1_A) << "," << d(H1_B) << "," << d(H1_merge) << ",";

    // --- 2D entropy estimates ---
    ss << d(H2_est_A) << "," << d(H2_est_B) << "," << d(H2_est_merge) << ",";

    // --- Target ---
    ss << d(SMI_est) << "," << d(gain_oracle_est) << ",";

    // --- Compact estimator ---
    ss << d(H1_merge_compact) << "," << d(gain_1D_compact) << ","
       << d(error_compact_abs) << "," << d(error_compact_rel) << "," << d(error_compact_norm_B) << ",";

    // --- Oracle 1D degree ---
    ss << d(gain_1D_oracle_degree) << ","
       << d(error_oracle_degree_abs) << "," << d(error_oracle_degree_rel) << "," << d(error_oracle_degree_norm_B) << ",";

    // --- Modularity ---
    ss << d(modularity_B_planted) << "," << d(modularity_B_H2_partition) << ",";

    // --- H2 oracle diagnostics for A ---
    ss << H2_n_candidates_A << ",";

    // --- H2 oracle diagnostics for B ---
    ss << H2_n_candidates_B << ",";

    // --- H2 oracle diagnostics for merge ---
    ss << H2_n_candidates_merge << ",";

    // score_best
    ss << d(H2_score_best_A) << "," << d(H2_score_best_B) << "," << d(H2_score_best_merge) << ",";

    // score_second_best
    ss << d(H2_score_second_best_A) << "," << d(H2_score_second_best_B) << "," << d(H2_score_second_best_merge) << ",";

    // best_minus_second
    ss << d(H2_best_minus_second_A) << "," << d(H2_best_minus_second_B) << "," << d(H2_best_minus_second_merge) << ",";

    // n_unique_scores
    ss << H2_n_unique_scores_A << "," << H2_n_unique_scores_B << "," << H2_n_unique_scores_merge << ",";

    // n_within_1e_minus_6
    ss << H2_n_within_1e_minus_6_A << "," << H2_n_within_1e_minus_6_B << "," << H2_n_within_1e_minus_6_merge << ",";

    // n_within_1e_minus_4
    ss << H2_n_within_1e_minus_4_A << "," << H2_n_within_1e_minus_4_B << "," << H2_n_within_1e_minus_4_merge << ",";

    // H2 method/seed/community count
    ss << H2_method_A << "," << H2_method_B << "," << H2_method_merge << ","
       << H2_seed_A << "," << H2_seed_B << "," << H2_seed_merge << ","
       << n_communities_H2_A << "," << n_communities_H2_B << "," << n_communities_H2_merge << ",";

    // --- Compact summary size ---
    ss << summary_compact_size << ",";

    // --- Sanity flags ---
    ss << (sanity_passed ? "1" : "0") << ","
       << sw << ","
       << (oracle_fragility_warning ? "1" : "0");

    return ss.str();
}

} // namespace entropy::exp_a
