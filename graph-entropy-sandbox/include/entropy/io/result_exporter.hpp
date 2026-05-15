#pragma once
#include "entropy/core/result.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace entropy::io {

void write_run_output(const std::string&    run_dir,
                      const nlohmann::json& manifest,
                      const ResultSet&      results);

} // namespace entropy::io
