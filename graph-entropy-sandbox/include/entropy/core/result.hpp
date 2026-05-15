#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace entropy {

struct AlgorithmOutput {
    double         value = 0.0;
    std::optional<std::vector<double>> aux;
    nlohmann::json metadata = nlohmann::json::object();
};

struct RunRecord {
    std::string   algorithm_name;
    std::string   graph_label;
    std::uint64_t graph_content_hash = 0;

    nlohmann::json parameters;
    nlohmann::json output_metadata;

    double              value = 0.0;
    std::vector<double> value_repeats;
    std::vector<double> wall_ms_repeats;

    double wall_ms_median = 0.0;
    double wall_ms_p05    = 0.0;
    double wall_ms_p95    = 0.0;

    bool        skipped     = false;
    std::string skip_reason;
};

class ResultSet {
public:
    void           add(RunRecord r);
    nlohmann::json to_json() const;
    void           write_json(const std::string& path) const;
    void           write_csv (const std::string& path) const;

    const std::vector<RunRecord>& records() const noexcept { return records_; }

private:
    std::vector<RunRecord> records_;
};

} // namespace entropy
