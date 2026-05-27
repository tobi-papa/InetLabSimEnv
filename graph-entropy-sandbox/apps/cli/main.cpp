#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <iostream>
#include "entropy/core/algorithm_registry.hpp"
#include "entropy/core/graph_registry.hpp"
#include "entropy/core/experiment_runner.hpp"
#include "entropy/exp_a/experiment_a_runner.hpp"
#include "entropy/io/config.hpp"

int main(int argc, char* argv[]) {
    CLI::App app{"entropy-cli — graph entropy research sandbox"};
    app.set_version_flag("--version", "0.1.0-skeleton");

    // -----------------------------------------------------------------------
    // Legacy top-level flags (skeleton experiment runner)
    // -----------------------------------------------------------------------
    std::string config_path;
    bool        list_algorithms = false;
    bool        list_loaders    = false;
    int         repeats_override = -1;
    std::string output_dir_override;

    app.add_option("-c,--config",       config_path,         "Path to TOML experiment config");
    app.add_flag  ("--list-algorithms", list_algorithms,     "List registered algorithms and exit");
    app.add_flag  ("--list-loaders",    list_loaders,        "List registered loaders and exit");
    app.add_option("--repeats",         repeats_override,    "Override repeats from config");
    app.add_option("--output-dir",      output_dir_override, "Override output_dir from config");

    // -----------------------------------------------------------------------
    // Experiment A subcommand
    // -----------------------------------------------------------------------
    entropy::exp_a::ExperimentAConfig exp_a_cfg;
    bool exp_a_smoke = false;

    auto* exp_a_sub = app.add_subcommand("exp-a",
        "Run Experiment A: Does Community Structure Matter for Gain?");
    exp_a_sub->add_option("--config", config_path,
        "Path to TOML config (overrides built-in defaults)");
    exp_a_sub->add_option("--output-dir", exp_a_cfg.output_dir,
        "Output directory (default: results/exp_a)");
    exp_a_sub->add_option("--seed", exp_a_cfg.base_seed,
        "Base random seed (default: 42)");
    exp_a_sub->add_flag("--smoke", exp_a_smoke,
        "Smoke run: use a tiny grid for quick validation");

    CLI11_PARSE(app, argc, argv);

    // -----------------------------------------------------------------------
    // Dispatch: exp-a subcommand
    // -----------------------------------------------------------------------
    if (*exp_a_sub) {
        if (exp_a_smoke) {
            exp_a_cfg.smoke       = true;
            exp_a_cfg.mu_B_values = {0.10, 0.30, 0.75};
            exp_a_cfg.n_shared_values = {15};
            exp_a_cfg.reps        = 2;
        }
        entropy::exp_a::run_experiment_a(exp_a_cfg);
        return 0;
    }

    // -----------------------------------------------------------------------
    // Legacy skeleton experiment runner
    // -----------------------------------------------------------------------
    auto& algo_reg   = entropy::AlgorithmRegistry::instance();
    auto& loader_reg = entropy::GraphRegistry::instance();

    const auto algo_names   = algo_reg.list_names();
    const auto loader_names = loader_reg.list_names();

    spdlog::info("Loaded {} algorithm(s), {} loader(s)",
                 algo_names.size(), loader_names.size());

    if (algo_names.empty() || loader_names.empty()) {
        spdlog::error(
            "Registry is empty — likely a linker issue. "
            "Check that entropy_plugins is linked with --whole-archive in CMakeLists.txt.");
        return 1;
    }

    if (list_algorithms) {
        for (const auto& n : algo_names) std::cout << n << "\n";
        return 0;
    }

    if (list_loaders) {
        for (const auto& n : loader_names) std::cout << n << "\n";
        return 0;
    }

    if (config_path.empty()) {
        std::cerr << "Error: --config <path> is required.\n";
        std::cerr << app.help() << "\n";
        return 1;
    }

    entropy::ExperimentConfig cfg = entropy::io::parse_config(config_path);
    if (repeats_override >= 0)        cfg.repeats    = repeats_override;
    if (!output_dir_override.empty()) cfg.output_dir = output_dir_override;

    entropy::ExperimentRunner runner(std::move(cfg));
    runner.run();

    return 0;
}
