#include "entropy/core/i_graph_loader.hpp"
#include "entropy/core/graph_registry.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace entropy {

class EdgelistLoader final : public IGraphLoader {
public:
    std::string_view name() const noexcept override { return "edgelist"; }

    Graph load(const Parameters& params) const override {
        const auto path     = params.get<std::string>("path");
        const bool directed = params.get_or<bool>("directed", false);
        const bool weighted = params.get_or<bool>("weighted", false);

        const auto dir = directed ? GraphDirectedness::Directed
                                  : GraphDirectedness::Undirected;
        const auto wgt = weighted ? GraphWeighting::Weighted
                                  : GraphWeighting::Unweighted;

        GraphBuilder builder(dir, wgt);

        std::ifstream file(path);
        if (!file.is_open())
            throw std::runtime_error("EdgelistLoader: cannot open '" + path + "'");

        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream ss(line);
            NodeId u, v;
            if (!(ss >> u >> v)) continue;
            double w = 1.0;
            if (weighted) ss >> w;
            builder.add_edge(u, v, w);
        }

        return std::move(builder).build(path);
    }
};

ENTROPY_REGISTER_LOADER("edgelist", EdgelistLoader)

} // namespace entropy
