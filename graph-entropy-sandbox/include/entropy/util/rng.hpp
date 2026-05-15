#pragma once
#include <cstdint>
#include <random>

namespace entropy::util {

// splitmix64 — portable deterministic seed expansion.
// Used to derive per-(graph, algo, repeat) seeds from a single base seed.
// Unlike std::seed_seq, output is identical across platforms and compilers.
inline std::uint64_t splitmix64(std::uint64_t x) noexcept {
    x += 0x9e3779b97f4a7c15ULL;
    x  = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x  = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

class Rng {
public:
    explicit Rng(std::uint64_t seed) : engine_(seed) {}

    // Derive a child RNG seed deterministically from experiment parameters.
    static std::uint64_t derive_seed(std::uint64_t base,
                                     std::uint64_t graph_hash,
                                     std::uint64_t algo_hash,
                                     std::uint64_t repeat_idx) noexcept {
        return splitmix64(base ^ graph_hash ^ algo_hash ^ repeat_idx);
    }

    std::mt19937_64& engine() noexcept { return engine_; }

private:
    std::mt19937_64 engine_;
};

} // namespace entropy::util
