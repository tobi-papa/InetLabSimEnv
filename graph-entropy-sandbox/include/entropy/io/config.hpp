#pragma once
#include "entropy/core/experiment_runner.hpp"
#include <string>

namespace entropy::io {

ExperimentConfig parse_config(const std::string& path);

} // namespace entropy::io
