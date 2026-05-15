#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <iostream>
#include "entropy/core/algorithm_registry.hpp"
#include "entropy/core/graph_registry.hpp"
#include "entropy/core/experiment_runner.hpp"
#include "entropy/io/config.hpp"

int main(int argc, char* argv[]) {
    CLI::App app{"entropy-cli — graph entropy research sandbox"};
    app.set_version_flag("--version", "0.1.0-skeleton");

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

    CLI11_PARSE(app, argc, argv);

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
