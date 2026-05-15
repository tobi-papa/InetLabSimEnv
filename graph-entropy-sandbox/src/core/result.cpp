#include "entropy/core/result.hpp"
#include <fstream>
#include <stdexcept>

namespace entropy {

void ResultSet::add(RunRecord r) {
    records_.push_back(std::move(r));
}

nlohmann::json ResultSet::to_json() const {
    auto arr = nlohmann::json::array();
    for (const auto& rec : records_) {
        nlohmann::json j;
        j["algorithm"]          = rec.algorithm_name;
        j["graph_label"]        = rec.graph_label;
        j["graph_content_hash"] = rec.graph_content_hash;
        j["parameters"]         = rec.parameters;
        j["output_metadata"]    = rec.output_metadata;
        j["value"]              = rec.value;
        j["value_repeats"]      = rec.value_repeats;
        j["wall_ms_repeats"]    = rec.wall_ms_repeats;
        j["wall_ms_median"]     = rec.wall_ms_median;
        j["wall_ms_p05"]        = rec.wall_ms_p05;
        j["wall_ms_p95"]        = rec.wall_ms_p95;
        j["skipped"]            = rec.skipped;
        j["skip_reason"]        = rec.skip_reason;
        arr.push_back(j);
    }
    return arr;
}

void ResultSet::write_json(const std::string& path) const {
    std::ofstream f(path);
    if (!f) throw std::runtime_error("Cannot write: " + path);
    f << to_json().dump(2) << "\n";
}

void ResultSet::write_csv(const std::string& path) const {
    std::ofstream f(path);
    if (!f) throw std::runtime_error("Cannot write: " + path);
    f << "algorithm,graph_label,repeat_idx,value,wall_ms\n";
    for (const auto& rec : records_) {
        if (rec.skipped) continue;
        for (std::size_t i = 0; i < rec.value_repeats.size(); ++i) {
            f << rec.algorithm_name << ","
              << '"' << rec.graph_label << '"' << ","
              << i << ","
              << rec.value_repeats[i] << ","
              << rec.wall_ms_repeats[i] << "\n";
        }
    }
}

} // namespace entropy
