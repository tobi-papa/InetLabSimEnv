#include "entropy/core/graph.hpp"
#include <algorithm>
#include <mutex>
#include <utility>
#define XXH_STATIC_LINKING_ONLY
#include <xxhash.h>

namespace entropy {

Graph::Graph(std::vector<std::uint64_t> row_ptr,
             std::vector<NodeId>         col_idx,
             std::vector<double>         weights,
             GraphDirectedness           directedness,
             GraphWeighting              weighting,
             std::string                 origin_label)
    : row_ptr_(std::move(row_ptr))
    , col_idx_(std::move(col_idx))
    , weights_(std::move(weights))
    , directedness_(directedness)
    , weighting_(weighting)
    , origin_label_(std::move(origin_label))
{
    const EdgeId directed_count = static_cast<EdgeId>(col_idx_.size());
    num_edges_ = (directedness_ == GraphDirectedness::Undirected)
                     ? directed_count / 2
                     : directed_count;
}

Graph::Graph(Graph&& other) noexcept
    : row_ptr_(std::move(other.row_ptr_))
    , col_idx_(std::move(other.col_idx_))
    , weights_(std::move(other.weights_))
    , directedness_(other.directedness_)
    , weighting_(other.weighting_)
    , origin_label_(std::move(other.origin_label_))
    , num_edges_(other.num_edges_)
    // hash_flag_ default-constructs to "not called"; hash will be recomputed lazily.
    , cached_hash_(0)
{}

Graph& Graph::operator=(Graph&& other) noexcept {
    if (this != &other) {
        row_ptr_      = std::move(other.row_ptr_);
        col_idx_      = std::move(other.col_idx_);
        weights_      = std::move(other.weights_);
        directedness_ = other.directedness_;
        weighting_    = other.weighting_;
        origin_label_ = std::move(other.origin_label_);
        num_edges_    = other.num_edges_;
        cached_hash_  = 0;
        // Reset hash_flag_ to "not called" by placement-new.
        hash_flag_.~once_flag();
        new (&hash_flag_) std::once_flag{};
    }
    return *this;
}

NodeId Graph::num_nodes() const noexcept {
    return row_ptr_.empty() ? 0u
                            : static_cast<NodeId>(row_ptr_.size() - 1);
}

EdgeId Graph::num_edges() const noexcept { return num_edges_; }

GraphDirectedness Graph::directedness() const noexcept { return directedness_; }
GraphWeighting    Graph::weighting()    const noexcept { return weighting_; }

std::span<const NodeId> Graph::neighbors(NodeId v) const noexcept {
    const auto start = row_ptr_[v];
    const auto end   = row_ptr_[v + 1];
    return {col_idx_.data() + start, static_cast<std::size_t>(end - start)};
}

std::span<const double> Graph::edge_weights(NodeId v) const noexcept {
    const auto start = row_ptr_[v];
    const auto end   = row_ptr_[v + 1];
    return {weights_.data() + start, static_cast<std::size_t>(end - start)};
}

std::uint64_t Graph::content_hash() const {
    std::call_once(hash_flag_, [this] {
        XXH3_state_t state;
        XXH3_64bits_reset(&state);
        XXH3_64bits_update(&state, col_idx_.data(), col_idx_.size() * sizeof(NodeId));
        XXH3_64bits_update(&state, weights_.data(), weights_.size() * sizeof(double));
        const std::uint8_t flags[2] = {
            static_cast<std::uint8_t>(directedness_),
            static_cast<std::uint8_t>(weighting_)
        };
        XXH3_64bits_update(&state, flags, sizeof(flags));
        cached_hash_ = XXH3_64bits_digest(&state);
    });
    return cached_hash_;
}

const std::string& Graph::origin_label() const noexcept { return origin_label_; }

GraphBuilder::GraphBuilder(GraphDirectedness d, GraphWeighting w)
    : directedness_(d), weighting_(w) {}

void GraphBuilder::reserve_nodes(NodeId n) {
    if (n > 0) {
        has_nodes_ = true;
        if (n - 1 > max_node_) max_node_ = n - 1;
    }
}

void GraphBuilder::add_edge(NodeId u, NodeId v, double w) {
    edges_.push_back({u, v, w});
    has_nodes_ = true;
    if (u > max_node_) max_node_ = u;
    if (v > max_node_) max_node_ = v;
}

Graph GraphBuilder::build(std::string origin_label) && {
    const NodeId n = has_nodes_ ? max_node_ + 1 : 0;

    std::vector<Edge> all_edges = edges_;
    if (directedness_ == GraphDirectedness::Undirected) {
        for (const auto& e : edges_)
            if (e.u != e.v) all_edges.push_back({e.v, e.u, e.weight});
    }

    std::sort(all_edges.begin(), all_edges.end(), [](const Edge& a, const Edge& b) {
        return a.u < b.u || (a.u == b.u && a.v < b.v);
    });

    std::vector<std::uint64_t> row_ptr(n + 1, 0);
    std::vector<NodeId>         col_idx;
    std::vector<double>         weights;
    col_idx.reserve(all_edges.size());
    weights.reserve(all_edges.size());

    for (const auto& e : all_edges) row_ptr[e.u + 1]++;
    for (NodeId i = 0; i < n; ++i)  row_ptr[i + 1] += row_ptr[i];

    for (const auto& e : all_edges) {
        col_idx.push_back(e.v);
        weights.push_back(e.weight);
    }

    return Graph(std::move(row_ptr), std::move(col_idx), std::move(weights),
                 directedness_, weighting_, std::move(origin_label));
}

} // namespace entropy
