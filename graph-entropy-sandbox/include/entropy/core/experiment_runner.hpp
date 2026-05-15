#pragma once
#include "parameters.hpp"
#include "result.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace entropy {

struct AlgorithmSpec {
    std::string name;
    Parameters  params;
};

struct GraphSpec {
    std::string loader_name;
    Parameters  loader_params;
};

struct ExperimentConfig {
    std::string name;
    std::vector<GraphSpec>     graphs;
    std::vector<AlgorithmSpec> algorithms;

    int           repeats   = 5;
    int           warmups   = 1;
    std::uint64_t seed      = 0;
    std::string   output_dir;

    std::string config_verbatim;
    std::string config_path;
};

class ExperimentRunner {
public:
    explicit ExperimentRunner(ExperimentConfig cfg);
    ResultSet run();

private:
    ExperimentConfig cfg_;
};

} // namespace entropy
