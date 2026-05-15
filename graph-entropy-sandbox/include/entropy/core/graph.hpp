#pragma once
#include <cstdint>
#include <mutex>
#include <span>
#include <string>
#include <vector>

namespace entropy {

using NodeId = std::uint32_t;
using EdgeId = std::uint64_t;

enum class GraphDirectedness : std::uint8_t { Undirected, Directed };
enum class GraphWeighting    : std::uint8_t { Unweighted, Weighted  };

struct Edge {
    NodeId u;
    NodeId v;
    double weight = 1.0;
};

// Immutable CSR (Compressed Sparse Row) graph. Built via GraphBuilder.
// For undirected graphs every undirected edge is stored as two directed entries.
class Graph {
public:
    Graph(std::vector<std::uint64_t> row_ptr,
          std::vector<NodeId>         col_idx,
          std::vector<double>         weights,
          GraphDirectedness           directedness,
          GraphWeighting              weighting,
          std::string                 origin_label);

    // once_flag is not movable, so we define move operations explicitly.
    Graph(Graph&&) noexcept;
    Graph& operator=(Graph&&) noexcept;
    Graph(const Graph&) = delete;
    Graph& operator=(const Graph&) = delete;

    NodeId num_nodes()    const noexcept;
    EdgeId num_edges()    const noexcept; // undirected edges counted once
    GraphDirectedness directedness() const noexcept;
    GraphWeighting    weighting()    const noexcept;

    // Zero-allocation neighbour views via std::span.
    std::span<const NodeId> neighbors   (NodeId v) const noexcept;
    std::span<const double> edge_weights(NodeId v) const noexcept;

    // xxh3 content hash of (sorted edge list + weights + flags).
    std::uint64_t      content_hash()  const;
    const std::string& origin_label()  const noexcept;

private:
    std::vector<std::uint64_t> row_ptr_;
    std::vector<NodeId>         col_idx_;
    std::vector<double>         weights_;
    GraphDirectedness           directedness_;
    GraphWeighting              weighting_;
    std::string                 origin_label_;
    EdgeId                      num_edges_;
    mutable std::once_flag      hash_flag_;
    mutable std::uint64_t       cached_hash_ = 0;
};

class GraphBuilder {
public:
    GraphBuilder(GraphDirectedness d, GraphWeighting w);
    void  reserve_nodes(NodeId n);
    void  add_edge(NodeId u, NodeId v, double w = 1.0);
    Graph build(std::string origin_label) &&; // consumes the builder
private:
    GraphDirectedness directedness_;
    GraphWeighting    weighting_;
    NodeId            max_node_   = 0;
    bool              has_nodes_  = false;
    std::vector<Edge> edges_;
};

} // namespace entropy
