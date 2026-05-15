#pragma once
#include <map>
#include <string>
#include <variant>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace entropy {

using ParamValue = std::variant<bool, std::int64_t, double, std::string>;

class Parameters {
public:
    void set(std::string key, ParamValue v);
    bool contains(const std::string& key) const noexcept;

    template <typename T>
    T get(const std::string& key) const {
        auto it = kv_.find(key);
        if (it == kv_.end())
            throw std::out_of_range("Parameter not found: " + key);
        return std::get<T>(it->second);
    }

    template <typename T>
    T get_or(const std::string& key, T fallback) const noexcept {
        auto it = kv_.find(key);
        if (it == kv_.end()) return fallback;
        if (const T* val = std::get_if<T>(&it->second)) return *val;
        return fallback;
    }

    nlohmann::json to_json() const;

    // Merge all entries from other into this, overwriting on key conflict.
    void merge_from(const Parameters& other);

private:
    std::map<std::string, ParamValue> kv_;
};

} // namespace entropy
