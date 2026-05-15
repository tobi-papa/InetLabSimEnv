#include "entropy/core/graph_cache.hpp"
#include <numeric>
#include <stdexcept>

namespace entropy {

GraphCache::GraphCache(const Graph& g) noexcept : g_(g) {}

const std::vector<std::uint32_t>& GraphCache::degrees() const {
    std::call_once(degrees_flag_, [this] {
        const NodeId n = g_.num_nodes();
        degrees_.resize(n);
        for (NodeId v = 0; v < n; ++v)
            degrees_[v] = static_cast<std::uint32_t>(g_.neighbors(v).size());
    });
    return degrees_;
}

const std::vector<double>& GraphCache::degree_distribution() const {
    std::call_once(dist_flag_, [this] {
        const auto& degs = degrees();
        const double vol = static_cast<double>(
            std::accumulate(degs.begin(), degs.end(), std::uint64_t{0}));
        degree_distribution_.resize(degs.size());
        for (std::size_t i = 0; i < degs.size(); ++i)
            degree_distribution_[i] = (vol > 0.0) ? degs[i] / vol : 0.0;
    });
    return degree_distribution_;
}

const Eigen::SparseMatrix<double>& GraphCache::adjacency_matrix() const {
    throw std::runtime_error("GraphCache::adjacency_matrix: not yet implemented");
}

const Eigen::SparseMatrix<double>& GraphCache::laplacian() const {
    throw std::runtime_error("GraphCache::laplacian: not yet implemented");
}

const Eigen::SparseMatrix<double>& GraphCache::normalized_laplacian() const {
    throw std::runtime_error("GraphCache::normalized_laplacian: not yet implemented");
}

const Eigen::VectorXd& GraphCache::normalized_laplacian_spectrum() const {
    throw std::runtime_error("GraphCache::normalized_laplacian_spectrum: not yet implemented");
}

const Eigen::VectorXd& GraphCache::random_walk_stationary() const {
    throw std::runtime_error("GraphCache::random_walk_stationary: not yet implemented");
}

} // namespace entropy
