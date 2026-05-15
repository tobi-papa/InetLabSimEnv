#pragma once
#include "i_graph_loader.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace entropy {

class GraphRegistry {
public:
    using Factory = std::function<std::unique_ptr<IGraphLoader>()>;

    static GraphRegistry& instance();

    bool register_factory(std::string name, Factory f);

    std::unique_ptr<IGraphLoader> create(const std::string& name) const;
    std::vector<std::string>      list_names()                    const;

private:
    std::unordered_map<std::string, Factory> factories_;
};

} // namespace entropy

#define ENTROPY_REGISTER_LOADER(KEY, CLASS)                                         \
    namespace {                                                                     \
        const bool _reg_##CLASS =                                                   \
            ::entropy::GraphRegistry::instance()                                    \
                .register_factory(KEY, [] { return std::make_unique<CLASS>(); });   \
    }
