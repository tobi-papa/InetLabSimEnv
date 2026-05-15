#pragma once
#include "i_entropy_algorithm.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace entropy {

class AlgorithmRegistry {
public:
    using Factory = std::function<std::unique_ptr<IEntropyAlgorithm>()>;

    static AlgorithmRegistry& instance();

    bool register_factory(std::string name, Factory f);

    std::unique_ptr<IEntropyAlgorithm> create(const std::string& name) const;
    std::vector<std::string>           list_names()                     const;

private:
    std::unordered_map<std::string, Factory> factories_;
};

} // namespace entropy

#define ENTROPY_REGISTER_ALGORITHM(KEY, CLASS)                                      \
    namespace {                                                                     \
        const bool _reg_##CLASS =                                                   \
            ::entropy::AlgorithmRegistry::instance()                                \
                .register_factory(KEY, [] { return std::make_unique<CLASS>(); });   \
    }
