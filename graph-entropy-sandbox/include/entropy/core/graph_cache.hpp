#pragma once
#include "graph.hpp"
#include <Eigen/Sparse>
#include <Eigen/Dense>
#include <mutex>
#include <vector>

namespace entropy {

class GraphCache {
public:
    explicit GraphCache(const Graph& g) noexcept;

    // Implemented in Phase 1:
    const std::vector<std::uint32_t>& degrees()            const;
    const std::vector<double>&        degree_distribution() const;

    // Stubbed — throws std::runtime_error("not yet implemented").
    const Eigen::SparseMatrix<double>& adjacency_matrix()             const;
    const Eigen::SparseMatrix<double>& laplacian()                     const;
    const Eigen::SparseMatrix<double>& normalized_laplacian()          const;
    const Eigen::VectorXd&             normalized_laplacian_spectrum()  const;
    const Eigen::VectorXd&             random_walk_stationary()        const;

private:
    const Graph& g_;

    mutable std::once_flag              degrees_flag_;
    mutable std::vector<std::uint32_t>  degrees_;

    mutable std::once_flag              dist_flag_;
    mutable std::vector<double>         degree_distribution_;

    mutable std::once_flag              adjacency_flag_;
    mutable Eigen::SparseMatrix<double> adjacency_matrix_;
    mutable std::once_flag              laplacian_flag_;
    mutable Eigen::SparseMatrix<double> laplacian_;
    mutable std::once_flag              norm_laplacian_flag_;
    mutable Eigen::SparseMatrix<double> normalized_laplacian_;
    mutable std::once_flag              spectrum_flag_;
    mutable Eigen::VectorXd             normalized_laplacian_spectrum_;
    mutable std::once_flag              stationary_flag_;
    mutable Eigen::VectorXd             random_walk_stationary_;
};

} // namespace entropy
