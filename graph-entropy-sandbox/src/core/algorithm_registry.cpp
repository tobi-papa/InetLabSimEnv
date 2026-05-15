#include "entropy/core/algorithm_registry.hpp"
#include <stdexcept>

namespace entropy {

AlgorithmRegistry& AlgorithmRegistry::instance() {
    static AlgorithmRegistry inst;
    return inst;
}

bool AlgorithmRegistry::register_factory(std::string name, Factory f) {
    return factories_.emplace(std::move(name), std::move(f)).second;
}

std::unique_ptr<IEntropyAlgorithm>
AlgorithmRegistry::create(const std::string& name) const {
    auto it = factories_.find(name);
    if (it == factories_.end())
        throw std::runtime_error("Unknown algorithm: '" + name
                                 + "'. Did you forget --whole-archive linkage?");
    return it->second();
}

std::vector<std::string> AlgorithmRegistry::list_names() const {
    std::vector<std::string> names;
    names.reserve(factories_.size());
    for (const auto& [k, _] : factories_) names.push_back(k);
    return names;
}

} // namespace entropy
