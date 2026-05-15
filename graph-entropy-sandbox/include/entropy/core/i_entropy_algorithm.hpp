#pragma once
#include "graph.hpp"
#include "graph_cache.hpp"
#include "parameters.hpp"
#include "requirements.hpp"
#include "result.hpp"
#include <string_view>

namespace entropy {

class IEntropyAlgorithm {
public:
    virtual ~IEntropyAlgorithm() = default;

    virtual std::string_view name()        const noexcept = 0;
    virtual std::string_view description() const noexcept = 0;
    virtual Requirements requirements()    const noexcept = 0;
    virtual Parameters default_parameters() const = 0;

    virtual AlgorithmOutput compute(const Graph&      g,
                                    GraphCache&       cache,
                                    const Parameters& params) const = 0;
};

} // namespace entropy
