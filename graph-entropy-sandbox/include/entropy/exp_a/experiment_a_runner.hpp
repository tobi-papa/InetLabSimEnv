#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace entropy::exp_a {

struct ExperimentAConfig {
    uint64_t    base_seed     = 42;
    std::string output_dir    = "results/exp_a";
    bool        smoke         = false;

    // Grid overrides (0 = use defaults from trial.hpp)
    std::vector<double> mu_B_values;
    std::vector<int>    n_shared_values;
    int    reps       = 0;
    int    n_A        = 0;
    int    n_B        = 0;
    int    k_A        = 0;
    int    k_B        = 0;
    double avg_deg_A  = 0.0;
    double avg_deg_B  = 0.0;
    double mu_A       = 0.0;
};

void run_experiment_a(const ExperimentAConfig& cfg);

} // namespace entropy::exp_a
