#include "entropy/core/parameters.hpp"

namespace entropy {

void Parameters::set(std::string key, ParamValue v) {
    kv_[std::move(key)] = std::move(v);
}

bool Parameters::contains(const std::string& key) const noexcept {
    return kv_.find(key) != kv_.end();
}

nlohmann::json Parameters::to_json() const {
    nlohmann::json j = nlohmann::json::object();
    for (const auto& [k, v] : kv_) {
        std::visit([&](const auto& val) { j[k] = val; }, v);
    }
    return j;
}

void Parameters::merge_from(const Parameters& other) {
    for (const auto& [k, v] : other.kv_)
        kv_[k] = v;
}

} // namespace entropy
