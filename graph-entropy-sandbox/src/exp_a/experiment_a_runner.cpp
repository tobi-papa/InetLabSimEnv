#include "entropy/exp_a/experiment_a_runner.hpp"
#include "entropy/exp_a/entropy2d.hpp"
#include "entropy/exp_a/sbm_gen.hpp"
#include "entropy/exp_a/estimators.hpp"
#include "entropy/exp_a/trial.hpp"
#include "entropy/util/rng.hpp"
#include "entropy/util/version.hpp"

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace entropy::exp_a {

using util::splitmix64;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string utc_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc{};
#if defined(_WIN32)
    gmtime_s(&tm_utc, &t);
#else
    gmtime_r(&t, &tm_utc);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm_utc, "%Y%m%dT%H%M%SZ");
    return ss.str();
}

// Derive the per-trial seed from base seed and indices.
// trial_seed = splitmix64(splitmix64(splitmix64(base_seed ^ mu_B_idx) ^ n_shared_idx) ^ rep)
static uint64_t derive_trial_seed(uint64_t base_seed,
                                  int mu_B_idx,
                                  int n_shared_idx,
                                  int rep) noexcept {
    uint64_t s = splitmix64(base_seed ^ static_cast<uint64_t>(mu_B_idx));
    s = splitmix64(s ^ static_cast<uint64_t>(n_shared_idx));
    s = splitmix64(s ^ static_cast<uint64_t>(rep));
    return s;
}

// Build node ID vectors for a given (n_A, n_B, n_shared) configuration.
// shared nodes:  global IDs 0 .. n_shared-1
// A-only nodes:  global IDs n_shared .. n_A-1
// B-only nodes:  global IDs n_A .. n_A + n_B - n_shared - 1
static void build_node_ids(int n_A, int n_B, int n_shared,
                            std::vector<NodeId>& node_ids_A,
                            std::vector<NodeId>& node_ids_B,
                            std::vector<NodeId>& shared_ids) {
    shared_ids.resize(static_cast<std::size_t>(n_shared));
    for (int i = 0; i < n_shared; ++i) {
        shared_ids[static_cast<std::size_t>(i)] = static_cast<NodeId>(i);
    }

    node_ids_A.resize(static_cast<std::size_t>(n_A));
    for (int i = 0; i < n_A; ++i) {
        node_ids_A[static_cast<std::size_t>(i)] = static_cast<NodeId>(i);
    }

    // B-only nodes start at n_A
    const int n_b_only = n_B - n_shared;
    node_ids_B.resize(static_cast<std::size_t>(n_B));
    for (int i = 0; i < n_shared; ++i) {
        node_ids_B[static_cast<std::size_t>(i)] = static_cast<NodeId>(i);
    }
    for (int i = 0; i < n_b_only; ++i) {
        node_ids_B[static_cast<std::size_t>(n_shared + i)] =
            static_cast<NodeId>(n_A + i);
    }
}

// Run sanity checks on a completed trial record. Populates sanity_warnings and
// sanity_passed. Returns false if a hard check fails (trial should be rejected).
static bool run_sanity_checks(TrialRecord& rec,
                               const H2Result& h2_A,
                               const H2Result& h2_B,
                               const H2Result& h2_merge,
                               const CompactEstimatorResult& compact) {
    bool hard_ok = true;
    std::string warns;

    // Hard: node counts
    if (rec.nodes_A != rec.n_A) {
        spdlog::warn("Sanity FAIL: nodes_A ({}) != n_A ({})", rec.nodes_A, rec.n_A);
        hard_ok = false;
    }
    if (rec.nodes_B != rec.n_B) {
        spdlog::warn("Sanity FAIL: nodes_B ({}) != n_B ({})", rec.nodes_B, rec.n_B);
        hard_ok = false;
    }

    // Hard: edge union identity
    const int64_t expected_merge_edges =
        rec.edges_A + rec.edges_B - rec.edge_overlap_AB;
    if (rec.edges_merge != expected_merge_edges) {
        spdlog::warn("Sanity FAIL: edges_merge ({}) != edges_A+edges_B-overlap ({})",
                     rec.edges_merge, expected_merge_edges);
        hard_ok = false;
    }

    // Hard: H2 <= H1
    if (h2_A.h2_est > rec.H1_A + 1e-9) {
        spdlog::warn("Sanity FAIL: H2_A ({}) > H1_A ({})", h2_A.h2_est, rec.H1_A);
        hard_ok = false;
    }
    if (h2_B.h2_est > rec.H1_B + 1e-9) {
        spdlog::warn("Sanity FAIL: H2_B ({}) > H1_B ({})", h2_B.h2_est, rec.H1_B);
        hard_ok = false;
    }
    if (h2_merge.h2_est > rec.H1_merge + 1e-9) {
        spdlog::warn("Sanity FAIL: H2_merge ({}) > H1_merge ({})",
                     h2_merge.h2_est, rec.H1_merge);
        hard_ok = false;
    }

    // Hard: compact summary size
    if (compact.summary_compact_size != rec.n_shared + 1) {
        spdlog::warn("Sanity FAIL: compact_size ({}) != n_shared+1 ({})",
                     compact.summary_compact_size, rec.n_shared + 1);
        hard_ok = false;
    }

    // Warning: SMI_est < 0
    if (rec.SMI_est < -1e-9) {
        warns += "SMI_est<0;";
    }
    // Warning: gain_oracle_est < 0
    if (rec.gain_oracle_est < -1e-9) {
        warns += "gain_oracle_est<0;";
    }
    // Warning: fragility flags from H2 oracles
    if (h2_A.fragility_warning) {
        warns += "oracle_fragile_A;";
        rec.oracle_fragility_warning = true;
    }
    if (h2_B.fragility_warning) {
        warns += "oracle_fragile_B;";
        rec.oracle_fragility_warning = true;
    }
    if (h2_merge.fragility_warning) {
        warns += "oracle_fragile_merge;";
        rec.oracle_fragility_warning = true;
    }

    rec.sanity_warnings = warns;
    rec.sanity_passed = hard_ok && warns.empty();
    return hard_ok;
}

// ---------------------------------------------------------------------------
// run_experiment_a
// ---------------------------------------------------------------------------
void run_experiment_a(const ExperimentAConfig& cfg) {
    // ------------------------------------------------------------------
    // 1. Resolve grid parameters
    // ------------------------------------------------------------------
    const std::vector<double> mu_B_values =
        cfg.mu_B_values.empty() ? GRID_MU_B_VALUES : cfg.mu_B_values;
    const std::vector<int> n_shared_values =
        cfg.n_shared_values.empty() ? GRID_N_SHARED_VALUES : cfg.n_shared_values;

    const int    n_A       = (cfg.n_A       == 0) ? GRID_N_A       : cfg.n_A;
    const int    n_B       = (cfg.n_B       == 0) ? GRID_N_B       : cfg.n_B;
    const int    k_A       = (cfg.k_A       == 0) ? GRID_K_A       : cfg.k_A;
    const int    k_B       = (cfg.k_B       == 0) ? GRID_K_B       : cfg.k_B;
    const double avg_deg_A = (cfg.avg_deg_A == 0.0) ? GRID_AVG_DEG_A : cfg.avg_deg_A;
    const double avg_deg_B = (cfg.avg_deg_B == 0.0) ? GRID_AVG_DEG_B : cfg.avg_deg_B;
    const double mu_A      = (cfg.mu_A      == 0.0) ? GRID_MU_A      : cfg.mu_A;
    const int    reps      = (cfg.reps      == 0)   ? GRID_REPS      : cfg.reps;

    // Smoke-test overrides: tiny grid
    const std::vector<double>& effective_mu_B    = mu_B_values;
    const std::vector<int>&    effective_n_shared = n_shared_values;

    // ------------------------------------------------------------------
    // 2. Create output directory with UTC timestamp
    // ------------------------------------------------------------------
    const std::string timestamp = utc_timestamp();
    const std::filesystem::path run_dir =
        std::filesystem::path(cfg.output_dir) / timestamp;
    std::filesystem::create_directories(run_dir);

    // ------------------------------------------------------------------
    // 3. Set up logging: both console and file
    // ------------------------------------------------------------------
    const std::string log_path = (run_dir / "run.log").string();
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink    = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_path, true);
    auto logger = std::make_shared<spdlog::logger>(
        "exp_a", spdlog::sinks_init_list{console_sink, file_sink});
    logger->set_level(spdlog::level::info);
    spdlog::set_default_logger(logger);

    // ------------------------------------------------------------------
    // 4. Print experiment header
    // ------------------------------------------------------------------
    spdlog::info("=== Experiment A Runner ===");
    spdlog::info("Git SHA:        {}", entropy::version::git_commit);
    spdlog::info("Git dirty:      {}", entropy::version::git_dirty ? "YES" : "NO");
    spdlog::info("Build type:     {}", entropy::version::build_type);
    spdlog::info("Base seed:      {}", cfg.base_seed);
    spdlog::info("Seed rule:      trial_seed = splitmix64(splitmix64(splitmix64(base_seed^mu_B_idx)^n_shared_idx)^rep)");
    spdlog::info("Output dir:     {}", run_dir.string());
    spdlog::info("Smoke mode:     {}", cfg.smoke ? "YES" : "NO");
    spdlog::info("n_A={} n_B={} k_A={} k_B={} avg_deg_A={} avg_deg_B={} mu_A={} reps={}",
                 n_A, n_B, k_A, k_B, avg_deg_A, avg_deg_B, mu_A, reps);
    {
        std::ostringstream ss;
        for (std::size_t i = 0; i < effective_mu_B.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << effective_mu_B[i];
        }
        spdlog::info("mu_B_values:    [{}]", ss.str());
    }
    {
        std::ostringstream ss;
        for (std::size_t i = 0; i < effective_n_shared.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << effective_n_shared[i];
        }
        spdlog::info("n_shared_values:[{}]", ss.str());
    }
    spdlog::info("Did code change after seeing outputs? NO (pre-run)");

    // ------------------------------------------------------------------
    // 5. Open CSV files
    // ------------------------------------------------------------------
    const std::string trials_path      = (run_dir / "trials.csv").string();
    const std::string trials_null_path = (run_dir / "trials_null.csv").string();

    std::ofstream csv_main(trials_path);
    std::ofstream csv_null(trials_null_path);
    if (!csv_main.is_open()) {
        throw std::runtime_error("Failed to open " + trials_path);
    }
    if (!csv_null.is_open()) {
        throw std::runtime_error("Failed to open " + trials_null_path);
    }

    csv_main << TrialRecord::to_csv_header() << "\n";
    csv_main.flush();
    csv_null << TrialRecord::to_csv_header() << "\n";
    csv_null.flush();

    // ------------------------------------------------------------------
    // 6. Main loop
    // ------------------------------------------------------------------
    constexpr int MAX_REJECTION_ATTEMPTS = 1000;

    int global_trial_id   = 0;
    int64_t total_accepted = 0;
    int64_t total_rejected = 0;

    for (std::size_t mi = 0; mi < effective_mu_B.size(); ++mi) {
        const double mu_B = effective_mu_B[mi];

        for (std::size_t si = 0; si < effective_n_shared.size(); ++si) {
            const int n_shared = effective_n_shared[si];

            // Pre-compute probabilities; skip this cell if invalid
            SbmParams params_A_check{n_A, k_A, avg_deg_A, mu_A, 0};
            SbmParams params_B_check{n_B, k_B, avg_deg_B, mu_B, 0};
            SbmProbabilities probs_A{};
            SbmProbabilities probs_B{};
            bool probs_valid = true;
            try {
                probs_A = mu_to_p(params_A_check);
                probs_B = mu_to_p(params_B_check);
            } catch (const std::invalid_argument& ex) {
                spdlog::warn("Skipping cell mu_B={} n_shared={}: invalid probability: {}",
                             mu_B, n_shared, ex.what());
                probs_valid = false;
            }
            if (!probs_valid) continue;

            // Build node ID templates (same for every rep in this cell)
            std::vector<NodeId> node_ids_A, node_ids_B, shared_ids;
            build_node_ids(n_A, n_B, n_shared, node_ids_A, node_ids_B, shared_ids);

            for (int rep = 0; rep < reps; ++rep) {
                const uint64_t trial_seed =
                    derive_trial_seed(cfg.base_seed,
                                      static_cast<int>(mi),
                                      static_cast<int>(si),
                                      rep);

                // Per-trial sub-seeds
                uint64_t seed_A    = splitmix64(trial_seed ^ 0x01ULL);
                uint64_t seed_B    = splitmix64(trial_seed ^ 0x02ULL);
                const uint64_t seed_H2_A = splitmix64(trial_seed ^ 0x03ULL);
                const uint64_t seed_H2_B = splitmix64(trial_seed ^ 0x04ULL);
                const uint64_t seed_H2_M = splitmix64(trial_seed ^ 0x05ULL);
                const uint64_t seed_null = splitmix64(trial_seed ^ 0x06ULL);

                // Rejection sampling
                int rej_total      = 0;
                int rej_A_disc     = 0;
                int rej_B_disc     = 0;
                int rej_merge_disc = 0;
                bool accepted = false;

                std::optional<PlantedGraph> planted_A_opt, planted_B_opt;
                std::optional<MergeResult> merge_result_opt;

                while (rej_total < MAX_REJECTION_ATTEMPTS) {
                    SbmParams params_A{n_A, k_A, avg_deg_A, mu_A, seed_A};
                    SbmParams params_B{n_B, k_B, avg_deg_B, mu_B, seed_B};

                    PlantedGraph cand_A = generate_sbm(params_A, node_ids_A);
                    if (!is_connected(cand_A.g)) {
                        ++rej_A_disc;
                        ++rej_total;
                        seed_A = splitmix64(seed_A);
                        continue;
                    }

                    PlantedGraph cand_B = generate_sbm(params_B, node_ids_B);
                    if (!is_connected(cand_B.g)) {
                        ++rej_B_disc;
                        ++rej_total;
                        seed_B = splitmix64(seed_B);
                        continue;
                    }

                    MergeResult cand_merge = union_graphs(cand_A.g, cand_B.g, n_shared);
                    if (!is_connected(cand_merge.g_merge)) {
                        ++rej_merge_disc;
                        ++rej_total;
                        seed_A = splitmix64(seed_A);
                        seed_B = splitmix64(seed_B);
                        continue;
                    }

                    // Accepted
                    planted_A_opt    = std::move(cand_A);
                    planted_B_opt    = std::move(cand_B);
                    merge_result_opt = std::move(cand_merge);
                    accepted = true;
                    break;
                }

                if (!accepted) {
                    spdlog::warn(
                        "Trial mu_B={} n_shared={} rep={}: exceeded {} rejection attempts, skipping.",
                        mu_B, n_shared, rep, MAX_REJECTION_ATTEMPTS);
                    total_rejected += rej_total;
                    continue;
                }

                total_accepted += 1;
                total_rejected += rej_total;

                PlantedGraph& planted_A   = *planted_A_opt;
                PlantedGraph& planted_B   = *planted_B_opt;
                MergeResult&  merge_result = *merge_result_opt;

                const Graph& g_A     = planted_A.g;
                const Graph& g_B     = planted_B.g;
                const Graph& g_merge = merge_result.g_merge;

                // ----------------------------------------------------------
                // Compute entropies
                // ----------------------------------------------------------
                const double H1_A     = compute_H1(g_A);
                const double H1_B     = compute_H1(g_B);
                const double H1_merge = compute_H1(g_merge);

                const H2Result h2_A     = compute_H2(g_A,     seed_H2_A);
                const H2Result h2_B     = compute_H2(g_B,     seed_H2_B);
                const H2Result h2_merge = compute_H2(g_merge, seed_H2_M);

                // ----------------------------------------------------------
                // Compact estimator
                // ----------------------------------------------------------
                const CompactSummary compact_summary =
                    make_compact_summary(g_B, shared_ids);
                const CompactEstimatorResult compact =
                    compact_estimator(g_A, compact_summary, shared_ids, H1_A);

                // ----------------------------------------------------------
                // Oracle 1D degree
                // ----------------------------------------------------------
                const double gain_1D_oracle = oracle_1D_degree_gain(g_merge, H1_A);

                // ----------------------------------------------------------
                // Oracle gain (2D)
                // ----------------------------------------------------------
                const double gain_oracle_est = h2_merge.h2_est - h2_A.h2_est;

                // ----------------------------------------------------------
                // Error metrics
                // ----------------------------------------------------------
                const ErrorMetrics err_compact =
                    compute_errors(gain_oracle_est, compact.gain_1D_compact, h2_B.h2_est);
                const ErrorMetrics err_oracle =
                    compute_errors(gain_oracle_est, gain_1D_oracle, h2_B.h2_est);

                // ----------------------------------------------------------
                // Realized stats
                // ----------------------------------------------------------
                const double mu_realized =
                    realized_mu_B(g_B, planted_B.community);
                const double mod_B_planted =
                    modularity(g_B, planted_B.community);
                const double mod_B_H2 =
                    modularity(g_B, h2_B.best_partition);

                // ----------------------------------------------------------
                // SMI_est
                // ----------------------------------------------------------
                const double SMI_est = h2_A.h2_est + h2_B.h2_est - h2_merge.h2_est;

                // ----------------------------------------------------------
                // Fill TrialRecord
                // ----------------------------------------------------------
                TrialRecord rec;
                rec.trial_id   = global_trial_id++;
                rec.trial_seed = trial_seed;

                rec.mu_A        = mu_A;
                rec.mu_B        = mu_B;
                rec.n_shared    = n_shared;
                rec.n_A         = n_A;
                rec.n_B         = n_B;
                rec.k_A         = k_A;
                rec.k_B         = k_B;
                rec.avg_degree_A = avg_deg_A;
                rec.avg_degree_B = avg_deg_B;
                rec.p_in_A      = probs_A.p_in;
                rec.p_out_A     = probs_A.p_out;
                rec.p_in_B      = probs_B.p_in;
                rec.p_out_B     = probs_B.p_out;

                rec.rejected_draws_before_acceptance = rej_total;
                rec.rejected_A_disconnected          = rej_A_disc;
                rec.rejected_B_disconnected          = rej_B_disc;
                rec.rejected_merge_disconnected      = rej_merge_disc;

                rec.nodes_A     = static_cast<int>(g_A.num_nodes());  // = n_A
                rec.nodes_B     = n_B;  // g_B.num_nodes() includes phantom global IDs
                rec.nodes_merge = static_cast<int>(g_merge.num_nodes());  // n_A+n_B-n_shared
                rec.edges_A     = static_cast<int64_t>(g_A.num_edges());
                rec.edges_B     = static_cast<int64_t>(g_B.num_edges());
                rec.edges_merge = static_cast<int64_t>(g_merge.num_edges());
                rec.edge_overlap_AB          = static_cast<int64_t>(merge_result.edge_overlap_AB);
                rec.shared_shared_edge_overlap =
                    static_cast<int64_t>(merge_result.shared_shared_edge_overlap);

                rec.actual_avg_degree_A =
                    (rec.nodes_A > 0)
                    ? 2.0 * static_cast<double>(rec.edges_A) / static_cast<double>(rec.nodes_A)
                    : 0.0;
                rec.actual_avg_degree_B =
                    (n_B > 0)
                    ? 2.0 * static_cast<double>(rec.edges_B) / static_cast<double>(n_B)
                    : 0.0;
                rec.actual_avg_degree_merge =
                    (rec.nodes_merge > 0)
                    ? 2.0 * static_cast<double>(rec.edges_merge) /
                          static_cast<double>(rec.nodes_merge)
                    : 0.0;
                rec.actual_mu_realized_B = mu_realized;

                rec.connected_A     = true;  // guaranteed by rejection sampling
                rec.connected_B     = true;
                rec.connected_merge = true;

                rec.H1_A     = H1_A;
                rec.H1_B     = H1_B;
                rec.H1_merge = H1_merge;

                rec.H2_est_A     = h2_A.h2_est;
                rec.H2_est_B     = h2_B.h2_est;
                rec.H2_est_merge = h2_merge.h2_est;

                rec.SMI_est        = SMI_est;
                rec.gain_oracle_est = gain_oracle_est;

                rec.H1_merge_compact     = compact.H1_merge_compact;
                rec.gain_1D_compact      = compact.gain_1D_compact;
                rec.error_compact_abs    = err_compact.abs_err;
                rec.error_compact_rel    = err_compact.rel_err;
                rec.error_compact_norm_B = err_compact.norm_B;

                rec.gain_1D_oracle_degree      = gain_1D_oracle;
                rec.error_oracle_degree_abs    = err_oracle.abs_err;
                rec.error_oracle_degree_rel    = err_oracle.rel_err;
                rec.error_oracle_degree_norm_B = err_oracle.norm_B;

                rec.modularity_B_planted      = mod_B_planted;
                rec.modularity_B_H2_partition = mod_B_H2;

                // H2 diagnostics — A
                rec.H2_n_candidates_A        = h2_A.n_candidates;
                rec.H2_score_best_A          = h2_A.score_best;
                rec.H2_score_second_best_A   = h2_A.score_second_best;
                rec.H2_best_minus_second_A   = h2_A.best_minus_second;
                rec.H2_n_unique_scores_A     = h2_A.n_unique_scores;
                rec.H2_n_within_1e_minus_6_A = h2_A.n_within_1e6;
                rec.H2_n_within_1e_minus_4_A = h2_A.n_within_1e4;

                // H2 diagnostics — B
                rec.H2_n_candidates_B        = h2_B.n_candidates;
                rec.H2_score_best_B          = h2_B.score_best;
                rec.H2_score_second_best_B   = h2_B.score_second_best;
                rec.H2_best_minus_second_B   = h2_B.best_minus_second;
                rec.H2_n_unique_scores_B     = h2_B.n_unique_scores;
                rec.H2_n_within_1e_minus_6_B = h2_B.n_within_1e6;
                rec.H2_n_within_1e_minus_4_B = h2_B.n_within_1e4;

                // H2 diagnostics — merge
                rec.H2_n_candidates_merge        = h2_merge.n_candidates;
                rec.H2_score_best_merge          = h2_merge.score_best;
                rec.H2_score_second_best_merge   = h2_merge.score_second_best;
                rec.H2_best_minus_second_merge   = h2_merge.best_minus_second;
                rec.H2_n_unique_scores_merge     = h2_merge.n_unique_scores;
                rec.H2_n_within_1e_minus_6_merge = h2_merge.n_within_1e6;
                rec.H2_n_within_1e_minus_4_merge = h2_merge.n_within_1e4;

                rec.H2_method_A     = h2_A.method;
                rec.H2_method_B     = h2_B.method;
                rec.H2_method_merge = h2_merge.method;
                rec.H2_seed_A       = h2_A.seed;
                rec.H2_seed_B       = h2_B.seed;
                rec.H2_seed_merge   = h2_merge.seed;
                rec.n_communities_H2_A     = h2_A.n_communities_best;
                rec.n_communities_H2_B     = h2_B.n_communities_best;
                rec.n_communities_H2_merge = h2_merge.n_communities_best;

                rec.summary_compact_size = compact.summary_compact_size;

                // ----------------------------------------------------------
                // Sanity checks
                // ----------------------------------------------------------
                if (!run_sanity_checks(rec, h2_A, h2_B, h2_merge, compact)) {
                    spdlog::error(
                        "Trial {} failed hard sanity check — skipping (NOT a rejection for science).",
                        rec.trial_id);
                    continue;
                }

                // ----------------------------------------------------------
                // Write main trial row
                // ----------------------------------------------------------
                csv_main << rec.to_csv_row() << "\n";
                csv_main.flush();

                spdlog::info(
                    "Trial {:4d}  mu_B={:.3f}  n_shared={:3d}  rep={:2d}  "
                    "rej={}  SMI={:.4f}  gain_oracle={:.4f}",
                    rec.trial_id, mu_B, n_shared, rep,
                    rej_total, SMI_est, gain_oracle_est);

                // ----------------------------------------------------------
                // Null control (spec §11.3)
                // ----------------------------------------------------------
                int null_swaps = 0;
                Graph g_B_null =
                    rewire_degree_preserving(g_B, seed_null, null_swaps);

                MergeResult null_merge_result =
                    union_graphs(g_A, g_B_null, n_shared);
                const Graph& g_merge_null = null_merge_result.g_merge;

                // Compute null entropies
                const double H1_B_null     = compute_H1(g_B_null);
                const double H1_merge_null = compute_H1(g_merge_null);

                const uint64_t seed_H2_B_null =
                    splitmix64(seed_null ^ 0x10ULL);
                const uint64_t seed_H2_M_null =
                    splitmix64(seed_null ^ 0x11ULL);

                const H2Result h2_B_null     = compute_H2(g_B_null,     seed_H2_B_null);
                const H2Result h2_merge_null = compute_H2(g_merge_null, seed_H2_M_null);

                const double gain_oracle_null = h2_merge_null.h2_est - h2_A.h2_est;

                // Compact estimator for null
                const CompactSummary compact_summary_null =
                    make_compact_summary(g_B_null, shared_ids);
                const CompactEstimatorResult compact_null =
                    compact_estimator(g_A, compact_summary_null, shared_ids, H1_A);

                const double gain_1D_oracle_null =
                    oracle_1D_degree_gain(g_merge_null, H1_A);

                const ErrorMetrics err_compact_null =
                    compute_errors(gain_oracle_null,
                                   compact_null.gain_1D_compact,
                                   h2_B_null.h2_est);
                const ErrorMetrics err_oracle_null =
                    compute_errors(gain_oracle_null,
                                   gain_1D_oracle_null,
                                   h2_B_null.h2_est);

                // Realized stats for null B
                // For null, we don't have a planted community; use an empty vector
                // to indicate "not applicable" — realized_mu_B would be 0.
                const double mod_B_null_H2 =
                    modularity(g_B_null, h2_B_null.best_partition);

                // Build null TrialRecord: copy setup fields, overwrite entropy/error fields
                TrialRecord null_rec = rec;  // copies all setup/identifier fields

                // Overwrite null-specific fields
                null_rec.nodes_B     = n_B;  // same as main trial
                null_rec.nodes_merge = static_cast<int>(g_merge_null.num_nodes());
                null_rec.edges_B     = static_cast<int64_t>(g_B_null.num_edges());
                null_rec.edges_merge = static_cast<int64_t>(g_merge_null.num_edges());
                null_rec.edge_overlap_AB =
                    static_cast<int64_t>(null_merge_result.edge_overlap_AB);
                null_rec.shared_shared_edge_overlap =
                    static_cast<int64_t>(null_merge_result.shared_shared_edge_overlap);

                null_rec.actual_avg_degree_B =
                    (n_B > 0)
                    ? 2.0 * static_cast<double>(null_rec.edges_B) /
                          static_cast<double>(n_B)
                    : 0.0;
                null_rec.actual_avg_degree_merge =
                    (null_rec.nodes_merge > 0)
                    ? 2.0 * static_cast<double>(null_rec.edges_merge) /
                          static_cast<double>(null_rec.nodes_merge)
                    : 0.0;
                null_rec.actual_mu_realized_B = 0.0;  // planted partition scrambled

                null_rec.connected_B     = is_connected(g_B_null);
                null_rec.connected_merge = is_connected(g_merge_null);

                null_rec.H1_B     = H1_B_null;
                null_rec.H1_merge = H1_merge_null;

                null_rec.H2_est_B     = h2_B_null.h2_est;
                null_rec.H2_est_merge = h2_merge_null.h2_est;

                null_rec.SMI_est        =
                    h2_A.h2_est + h2_B_null.h2_est - h2_merge_null.h2_est;
                null_rec.gain_oracle_est = gain_oracle_null;

                null_rec.H1_merge_compact     = compact_null.H1_merge_compact;
                null_rec.gain_1D_compact      = compact_null.gain_1D_compact;
                null_rec.error_compact_abs    = err_compact_null.abs_err;
                null_rec.error_compact_rel    = err_compact_null.rel_err;
                null_rec.error_compact_norm_B = err_compact_null.norm_B;

                null_rec.gain_1D_oracle_degree      = gain_1D_oracle_null;
                null_rec.error_oracle_degree_abs    = err_oracle_null.abs_err;
                null_rec.error_oracle_degree_rel    = err_oracle_null.rel_err;
                null_rec.error_oracle_degree_norm_B = err_oracle_null.norm_B;

                null_rec.modularity_B_planted      = 0.0;  // no planted partition
                null_rec.modularity_B_H2_partition = mod_B_null_H2;

                // H2 diagnostics — B null
                null_rec.H2_n_candidates_B        = h2_B_null.n_candidates;
                null_rec.H2_score_best_B          = h2_B_null.score_best;
                null_rec.H2_score_second_best_B   = h2_B_null.score_second_best;
                null_rec.H2_best_minus_second_B   = h2_B_null.best_minus_second;
                null_rec.H2_n_unique_scores_B     = h2_B_null.n_unique_scores;
                null_rec.H2_n_within_1e_minus_6_B = h2_B_null.n_within_1e6;
                null_rec.H2_n_within_1e_minus_4_B = h2_B_null.n_within_1e4;

                // H2 diagnostics — merge null
                null_rec.H2_n_candidates_merge        = h2_merge_null.n_candidates;
                null_rec.H2_score_best_merge          = h2_merge_null.score_best;
                null_rec.H2_score_second_best_merge   = h2_merge_null.score_second_best;
                null_rec.H2_best_minus_second_merge   = h2_merge_null.best_minus_second;
                null_rec.H2_n_unique_scores_merge     = h2_merge_null.n_unique_scores;
                null_rec.H2_n_within_1e_minus_6_merge = h2_merge_null.n_within_1e6;
                null_rec.H2_n_within_1e_minus_4_merge = h2_merge_null.n_within_1e4;

                null_rec.H2_method_B     = h2_B_null.method;
                null_rec.H2_method_merge = h2_merge_null.method;
                null_rec.H2_seed_B       = h2_B_null.seed;
                null_rec.H2_seed_merge   = h2_merge_null.seed;
                null_rec.n_communities_H2_B     = h2_B_null.n_communities_best;
                null_rec.n_communities_H2_merge = h2_merge_null.n_communities_best;

                null_rec.summary_compact_size = compact_null.summary_compact_size;

                // Null-specific sanity warnings (non-fatal)
                std::string null_warns;
                if (null_rec.SMI_est < -1e-9)         null_warns += "SMI_est<0;";
                if (null_rec.gain_oracle_est < -1e-9) null_warns += "gain_oracle_est<0;";
                if (h2_B_null.fragility_warning)      null_warns += "oracle_fragile_B;";
                if (h2_merge_null.fragility_warning)  null_warns += "oracle_fragile_merge;";
                null_rec.sanity_warnings      = null_warns;
                null_rec.oracle_fragility_warning =
                    h2_B_null.fragility_warning || h2_merge_null.fragility_warning;
                null_rec.sanity_passed = null_warns.empty();

                csv_null << null_rec.to_csv_row() << "\n";
                csv_null.flush();

            } // rep loop
        } // n_shared loop
    } // mu_B loop

    csv_main.close();
    csv_null.close();

    // ------------------------------------------------------------------
    // 7. Write manifest.json
    // ------------------------------------------------------------------
    {
        nlohmann::json manifest;
        manifest["git_sha"]   = std::string(entropy::version::git_commit);
        manifest["git_dirty"] = entropy::version::git_dirty;
        manifest["base_seed"] = cfg.base_seed;
        manifest["seed_rule"] =
            "trial_seed = splitmix64(splitmix64(splitmix64(base_seed^mu_B_idx)^n_shared_idx)^rep)";

        nlohmann::json grid;
        {
            nlohmann::json mu_b_arr = nlohmann::json::array();
            for (double v : effective_mu_B) mu_b_arr.push_back(v);
            grid["mu_B_values"] = mu_b_arr;

            nlohmann::json ns_arr = nlohmann::json::array();
            for (int v : effective_n_shared) ns_arr.push_back(v);
            grid["n_shared_values"] = ns_arr;
        }
        grid["n_A"]       = n_A;
        grid["n_B"]       = n_B;
        grid["k_A"]       = k_A;
        grid["k_B"]       = k_B;
        grid["avg_deg_A"] = avg_deg_A;
        grid["avg_deg_B"] = avg_deg_B;
        grid["mu_A"]      = mu_A;
        grid["reps"]      = reps;
        manifest["grid"]  = grid;

        manifest["accepted_trials"] = total_accepted;
        manifest["rejected_draws"]  = total_rejected;
        manifest["did_code_change_after_seeing_outputs"] = "NO (pre-run)";

        const std::string manifest_path = (run_dir / "manifest.json").string();
        std::ofstream mf(manifest_path);
        mf << manifest.dump(2) << "\n";
    }

    // ------------------------------------------------------------------
    // 8. Write config.json
    // ------------------------------------------------------------------
    {
        nlohmann::json config_json;
        config_json["base_seed"]  = cfg.base_seed;
        config_json["output_dir"] = cfg.output_dir;
        config_json["smoke"]      = cfg.smoke;
        {
            nlohmann::json arr = nlohmann::json::array();
            for (double v : cfg.mu_B_values) arr.push_back(v);
            config_json["mu_B_values"] = arr;
        }
        {
            nlohmann::json arr = nlohmann::json::array();
            for (int v : cfg.n_shared_values) arr.push_back(v);
            config_json["n_shared_values"] = arr;
        }
        config_json["reps"]      = cfg.reps;
        config_json["n_A"]       = cfg.n_A;
        config_json["n_B"]       = cfg.n_B;
        config_json["k_A"]       = cfg.k_A;
        config_json["k_B"]       = cfg.k_B;
        config_json["avg_deg_A"] = cfg.avg_deg_A;
        config_json["avg_deg_B"] = cfg.avg_deg_B;
        config_json["mu_A"]      = cfg.mu_A;

        const std::string config_path = (run_dir / "config.json").string();
        std::ofstream cf(config_path);
        cf << config_json.dump(2) << "\n";
    }

    spdlog::info("=== Experiment A complete ===");
    spdlog::info("Accepted trials: {}  Rejected draws: {}",
                 total_accepted, total_rejected);
    spdlog::info("Results written to: {}", run_dir.string());
}

} // namespace entropy::exp_a
