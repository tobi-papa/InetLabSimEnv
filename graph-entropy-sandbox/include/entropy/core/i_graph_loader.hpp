#pragma once
#include "graph.hpp"
#include "parameters.hpp"
#include <string_view>

namespace entropy {

class IGraphLoader {
public:
    virtual ~IGraphLoader() = default;

    virtual std::string_view name() const noexcept = 0;
    virtual Graph load(const Parameters& params) const = 0;
};

} // namespace entropy
