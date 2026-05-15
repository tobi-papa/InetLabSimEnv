#include "entropy/core/experiment_runner.hpp"
#include "entropy/core/algorithm_registry.hpp"
#include "entropy/core/graph_registry.hpp"
#include "entropy/core/graph_cache.hpp"
#include "entropy/core/requirements.hpp"
#include "entropy/io/result_exporter.hpp"
#include "entropy/util/version.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <nlohmann/json.hpp>
#include <xxhash.h>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <memory>
#include <stdexcept>

namespace entropy {

namespace {

std::string utc_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H-%M-%SZ", std::gmtime(&t));
    return buf;
}

} // namespace

ExperimentRunner::ExperimentRunner(ExperimentConfig cfg) : cfg_(std::move(cfg)) {}

ResultSet ExperimentRunner::run() {
    const std::string run_dir =
        cfg_.output_dir + "/" + cfg_.name + "/" + utc_timestamp();
    std::filesystem::create_directories(run_dir);

    auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file    = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                       run_dir + "/run.log", true);
    console->set_level(spdlog::level::info);
    file->set_level(spdlog::level::debug);
    auto logger = std::make_shared<spdlog::logger>(
        "entropy", spdlog::sinks_init_list{console, file});
    spdlog::set_default_logger(logger);

    auto& algo_reg   = AlgorithmRegistry::instance();
    auto& loader_reg = GraphRegistry::instance();

    ResultSet result_set;
    auto      graphs_meta = nlohmann::json::array();

    for (const auto& graph_spec : cfg_.graphs) {
        std::unique_ptr<IGraphLoader> loader;
        try {
            loader = loader_reg.create(graph_spec.loader_name);
        } catch (const std::exception& e) {
            spdlog::error("Loader error: {}", e.what());
            continue;
        }

        std::unique_ptr<Graph> graph_ptr;
        try {
            graph_ptr = std::make_unique<Graph>(loader->load(graph_spec.loader_params));
        } catch (const std::exception& e) {
            spdlog::error("Load error ('{}': {})", graph_spec.loader_name, e.what());
            continue;
        }
        Graph& graph = *graph_ptr;
        GraphCache cache(graph);

        nlohmann::json g_meta;
        g_meta["origin_label"] = graph.origin_label();
        g_meta["content_hash"] = graph.content_hash();
        g_meta["num_nodes"]    = graph.num_nodes();
        g_meta["num_edges"]    = graph.num_edges();
        g_meta["directedness"] = (graph.directedness() == GraphDirectedness::Undirected)
                                     ? "undirected" : "directed";
        g_meta["weighting"]    = (graph.weighting() == GraphWeighting::Unweighted)
                                     ? "unweighted" : "weighted";
        graphs_meta.push_back(g_meta);

        spdlog::info("Graph '{}': {} nodes, {} edges",
                     graph.origin_label(), graph.num_nodes(), graph.num_edges());

        for (const auto& algo_spec : cfg_.algorithms) {
            std::unique_ptr<IEntropyAlgorithm> algo;
            try {
                algo = algo_reg.create(algo_spec.name);
            } catch (const std::exception& e) {
                spdlog::error("Algorithm error: {}", e.what());
                continue;
            }

            Parameters params = algo->default_parameters();
            params.merge_from(algo_spec.params);

            const auto req = check(graph, algo->requirements());
            if (!req.ok) {
                spdlog::warn("Skip ({}, '{}'): {}",
                             algo_spec.name, graph.origin_label(), req.reason);
                RunRecord rec;
                rec.algorithm_name = algo_spec.name;
                rec.graph_label    = graph.origin_label();
                rec.skipped        = true;
                rec.skip_reason    = req.reason;
                result_set.add(rec);
                continue;
            }

            // TODO Phase 3: implement warmup + repeat loop here.
            const auto t0 = std::chrono::steady_clock::now();
            AlgorithmOutput output;
            try {
                output = algo->compute(graph, cache, params);
            } catch (const std::exception& e) {
                spdlog::error("Compute exception ({}, '{}'): {}",
                              algo_spec.name, graph.origin_label(), e.what());
                RunRecord rec;
                rec.algorithm_name = algo_spec.name;
                rec.graph_label    = graph.origin_label();
                rec.skipped        = true;
                rec.skip_reason    = std::string("exception: ") + e.what();
                result_set.add(rec);
                continue;
            }
            const auto   t1      = std::chrono::steady_clock::now();
            const double wall_ms =
                std::chrono::duration<double, std::milli>(t1 - t0).count();

            RunRecord rec;
            rec.algorithm_name     = std::string(algo->name());
            rec.graph_label        = graph.origin_label();
            rec.graph_content_hash = graph.content_hash();
            rec.parameters         = params.to_json();
            rec.output_metadata    = output.metadata;
            rec.value              = output.value;
            rec.value_repeats      = {output.value};
            rec.wall_ms_repeats    = {wall_ms};
            rec.wall_ms_median     = wall_ms;
            rec.wall_ms_p05        = wall_ms;
            rec.wall_ms_p95        = wall_ms;
            result_set.add(rec);

            spdlog::info("  {} -> value={:.6f}  ({:.3f} ms)",
                         algo->name(), output.value, wall_ms);
        }
    }

    const std::uint64_t cfg_hash =
        XXH3_64bits(cfg_.config_verbatim.data(), cfg_.config_verbatim.size());

    nlohmann::json manifest;
    manifest["experiment_name"]  = cfg_.name;
    manifest["git_commit"]       = std::string(version::git_commit);
    manifest["git_dirty"]        = version::git_dirty;
    manifest["build_type"]       = std::string(version::build_type);
    manifest["seed"]             = cfg_.seed;
    manifest["config_path"]      = cfg_.config_path;
    manifest["config_hash_xxh3"] = cfg_hash;
    manifest["config_verbatim"]  = cfg_.config_verbatim;
    manifest["graphs"]           = graphs_meta;
    manifest["runs"]             = result_set.to_json();

    entropy::io::write_run_output(run_dir, manifest, result_set);
    spdlog::info("Output written to {}", run_dir);

    return result_set;
}

} // namespace entropy
