#include "entropy/io/config.hpp"
#include <toml++/toml.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace entropy::io {

namespace {

std::string slurp(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open config: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

void fill_params(Parameters& p, const toml::table& tbl) {
    for (auto& [k, v] : tbl) {
        std::string key{k.str()};
        if (v.is_string())              p.set(key, std::string{v.as_string()->get()});
        else if (v.is_boolean())        p.set(key, v.as_boolean()->get());
        else if (v.is_integer())        p.set(key, static_cast<std::int64_t>(v.as_integer()->get()));
        else if (v.is_floating_point()) p.set(key, v.as_floating_point()->get());
    }
}

} // namespace

ExperimentConfig parse_config(const std::string& path) {
    const std::string verbatim = slurp(path);
    auto tbl = toml::parse(verbatim);

    ExperimentConfig cfg;
    cfg.config_verbatim = verbatim;
    cfg.config_path     = path;

    cfg.name       = tbl["name"].value_or(std::string{"experiment"});
    cfg.repeats    = tbl["repeats"].value_or(5);
    cfg.warmups    = tbl["warmups"].value_or(1);
    cfg.seed       = tbl["seed"].value_or(std::uint64_t{0});
    cfg.output_dir = tbl["output_dir"].value_or(std::string{"results"});

    if (auto* graphs = tbl["graphs"].as_array()) {
        for (auto& elem : *graphs) {
            auto* gt = elem.as_table();
            if (!gt) continue;
            GraphSpec spec;
            spec.loader_name = (*gt)["loader"].value_or(std::string{""});
            if (auto* pt = (*gt)["params"].as_table())
                fill_params(spec.loader_params, *pt);
            cfg.graphs.push_back(std::move(spec));
        }
    }

    if (auto* algos = tbl["algorithms"].as_array()) {
        for (auto& elem : *algos) {
            auto* at = elem.as_table();
            if (!at) continue;
            AlgorithmSpec spec;
            spec.name = (*at)["name"].value_or(std::string{""});
            if (auto* pt = (*at)["params"].as_table())
                fill_params(spec.params, *pt);
            cfg.algorithms.push_back(std::move(spec));
        }
    }

    return cfg;
}

} // namespace entropy::io
