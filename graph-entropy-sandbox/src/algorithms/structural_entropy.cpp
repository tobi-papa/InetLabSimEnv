#include "entropy/core/i_entropy_algorithm.hpp"
#include "entropy/core/algorithm_registry.hpp"
#include "entropy/util/numerics.hpp"

namespace entropy {

class StructuralEntropy final : public IEntropyAlgorithm {
public:
    std::string_view name() const noexcept override {
        return "structural_entropy";
    }

    std::string_view description() const noexcept override {
        return "1-D structural entropy (Li & Pan): Shannon entropy of the "
               "degree-based random-walk stationary distribution. "
               "H(G) = -sum_i p_i * log2(p_i), p_i = d_i / vol(G). "
               "Result in bits.";
    }

    Requirements requirements() const noexcept override {
        Requirements r;
        r.undirected = true;
        return r;
    }

    Parameters default_parameters() const override {
        return Parameters{};
    }

    AlgorithmOutput compute(const Graph& /*g*/,
                            GraphCache&       cache,
                            const Parameters& /*params*/) const override {
        const auto& dist = cache.degree_distribution();

        double H = 0.0;
        for (const double p : dist)
            H -= util::xlog2x(p);

        AlgorithmOutput out;
        out.value = H;
        out.metadata["formula"] = "-sum(p_i * log2(p_i))";
        out.metadata["unit"]    = "bits";
        return out;
    }
};

ENTROPY_REGISTER_ALGORITHM("structural_entropy", StructuralEntropy)

} // namespace entropy
