# Graph Entropy Research Sandbox — Implementation Specification

> **Audience:** the implementing AI / developer.
> **Status:** authoritative. If anything below conflicts with prior conversation, this document wins.

---

## 0. Purpose & Non-Goals

This is a **C++ research sandbox** for computing, benchmarking, and analyzing entropy measures on graphs. It is a scientific instrument, not a product.

**Optimize for, in order:**
1. **Reproducibility.** Any logged result must be re-derivable from the manifest alone.
2. **Extensibility.** Adding a new entropy algorithm = adding one `.cpp` file in `src/algorithms/`. Zero changes to core code, zero changes to a central registry file. This is a hard requirement.
3. **Scientific traceability.** Every run produces a self-describing artifact (config + code version + graph hash + results).
4. **Performance, when free.** Use Eigen, avoid copies, use `std::span`. Do **not** prematurely parallelize core algorithms — make them correct, then profile.
5. **Python interop** for plotting and post-hoc analysis. C++ produces structured data; Python produces figures.

**Non-goals:**
- This is not a graph database. Don't build a query layer.
- This is not a visualization tool. Visualization lives in Python (matplotlib / NetworkX / seaborn) or, optionally, a separate Dear ImGui app — never inside the simulation core.
- No GPU acceleration in v1.
- No distributed execution in v1.

---

## 1. Architecture at a Glance

```
                ┌─────────────────────────────────────────┐
                │              apps/cli                   │
                │  parses TOML config, builds runner      │
                └────────────────┬────────────────────────┘
                                 │
                ┌────────────────▼────────────────────────┐
                │           ExperimentRunner              │
                │   - dispatch graphs × algorithms        │
                │   - benchmarking (warm-up + repeats)    │
                │   - emits ResultSet                     │
                └─────┬───────────────────────┬───────────┘
                      │                       │
        ┌─────────────▼──────────┐   ┌────────▼──────────┐
        │     GraphRegistry      │   │ AlgorithmRegistry │
        │ (factory + self-reg)   │   │ (factory + self-reg)│
        └─────────────┬──────────┘   └────────┬──────────┘
                      │                       │
              ┌───────▼────────┐      ┌───────▼─────────────┐
              │ IGraphLoader   │      │ IEntropyAlgorithm   │
              │ (interface)    │      │ (interface)         │
              └───────┬────────┘      └───────┬─────────────┘
                      │                       │
              ┌───────▼────────┐      ┌───────▼─────────────┐
              │ Concrete       │      │ Concrete            │
              │ loaders        │      │ algorithms          │
              │ (Edgelist,     │      │ (Shannon, Von       │
              │  SNAP, GraphML,│      │  Neumann, Rényi,    │
              │  Erdős-Rényi,  │      │  Structural, …)     │
              │  …)            │      │                     │
              └───────┬────────┘      └─────────────────────┘
                      │
              ┌───────▼────────┐
              │     Graph      │  ← in-house, immutable post-build
              │ + GraphCache   │  ← lazy precomputed properties
              └────────────────┘
```

Three core ideas drive the design:

1. **Self-registering plugins.** Concrete algorithms and loaders register themselves into a static registry at program startup via a constructor of a static object. The runner discovers them by name. Adding code never touches existing files.
2. **Algorithm requirement contracts.** Each algorithm declares — at compile time or via a `requirements()` method — what kind of graph it needs (undirected, connected, weighted, simple). The runner validates before invoking. No silent garbage results.
3. **Shared lazy cache on `Graph`.** Expensive derived quantities (degree distribution, normalized Laplacian spectrum, stationary distribution of the random walk, …) are computed once on first request and reused across algorithms in the same experiment.

---

## 2. Dependencies (pin major versions in CMake)

| Purpose | Library | Notes |
|---|---|---|
| Linear algebra / spectra | **Eigen 3.4** | header-only, vendored as submodule |
| JSON I/O | **nlohmann/json 3.11+** | header-only |
| TOML config | **toml++ 3.x** | header-only |
| CLI parsing | **CLI11 2.x** | header-only |
| Structured logging | **spdlog 1.x** | binary-friendly logging |
| Testing | **Catch2 v3** or **GoogleTest** | pick one and stick with it |
| Benchmarking primitives | **Google Benchmark** (optional) | for microbenchmarks only |
| Python bindings (optional) | **pybind11 2.x** | for `pyentropy_sandbox` extension |
| Hashing | **xxhash** | for graph content hashes |

**Do not** add Boost or NetworKit to the dependency list. They are large, force their abstractions onto algorithms, and the entropy use-case doesn't justify the cost. If a specific algorithm later needs e.g. matchings from Boost, vendor that one header.

**Toolchain:**
- C++20, `-Wall -Wextra -Wpedantic -Werror` in CI.
- Compilers: GCC 12+, Clang 15+, MSVC 19.36+.
- Sanitizers in CI: ASan, UBSan, and a separate TSan job.

---

## 3. Directory Layout

```
graph-entropy-sandbox/
├── CMakeLists.txt
├── cmake/
│   ├── CompilerWarnings.cmake
│   └── Sanitizers.cmake
├── apps/
│   ├── cli/
│   │   └── main.cpp                 # argument parsing → ExperimentRunner
│   └── bench/
│       └── main.cpp                 # microbenchmarks
├── include/entropy/                 # public headers
│   ├── core/
│   │   ├── graph.hpp
│   │   ├── graph_cache.hpp
│   │   ├── i_entropy_algorithm.hpp
│   │   ├── i_graph_loader.hpp
│   │   ├── algorithm_registry.hpp
│   │   ├── graph_registry.hpp
│   │   ├── parameters.hpp
│   │   ├── requirements.hpp
│   │   ├── result.hpp
│   │   └── experiment_runner.hpp
│   ├── io/
│   │   ├── config.hpp
│   │   └── result_exporter.hpp
│   └── util/
│       ├── numerics.hpp             # safe_log, normalize, etc.
│       ├── rng.hpp                  # deterministic seed handling
│       └── version.hpp              # git hash baked in at configure time
├── src/
│   ├── core/                        # implementations of core/*
│   ├── algorithms/                  # one file per algorithm — ADD HERE
│   │   ├── shannon_degree_entropy.cpp
│   │   ├── von_neumann_entropy.cpp
│   │   ├── renyi_spectral_entropy.cpp
│   │   └── structural_entropy.cpp
│   ├── loaders/                     # one file per loader — ADD HERE
│   │   ├── edgelist_loader.cpp
│   │   ├── graphml_loader.cpp
│   │   ├── snap_loader.cpp
│   │   ├── erdos_renyi_generator.cpp
│   │   └── barabasi_albert_generator.cpp
│   └── io/
├── bindings/python/                 # optional pybind11 module
│   └── module.cpp
├── data/                            # input graphs (gitignored, except a tiny sample)
│   └── samples/
├── configs/                         # experiment recipes (TOML)
│   ├── smoke.toml
│   └── benchmark_real_world.toml
├── results/                         # gitignored output root
├── notebooks/                       # Python analysis notebooks
├── tests/
│   ├── unit/
│   ├── property/                    # known-graph entropies (e.g. K_n)
│   └── golden/                      # snapshot regression
├── docs/
│   ├── adding_an_algorithm.md
│   ├── adding_a_loader.md
│   └── reproducibility.md
└── README.md
```

---

## 4. Core Abstractions — Exact Signatures

These are contracts. Implement them as written.

### 4.1 `Graph` (immutable after construction)

```cpp
// include/entropy/core/graph.hpp
#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace entropy {

using NodeId = std::uint32_t;
using EdgeId = std::uint64_t;

enum class GraphDirectedness : std::uint8_t { Undirected, Directed };
enum class GraphWeighting   : std::uint8_t { Unweighted, Weighted };

struct Edge {
    NodeId u;
    NodeId v;
    double weight = 1.0;          // 1.0 if unweighted
};

// CSR (compressed sparse row) storage. Immutable post-build.
// For an undirected graph each undirected edge is stored as two directed entries.
class Graph {
public:
    // Build via GraphBuilder; do not expose mutators on Graph itself.
    Graph(std::vector<std::uint64_t> row_ptr,
          std::vector<NodeId>         col_idx,
          std::vector<double>         weights,
          GraphDirectedness           directedness,
          GraphWeighting              weighting,
          std::string                 origin_label);

    NodeId num_nodes() const noexcept;
    EdgeId num_edges() const noexcept;   // counts undirected edges once
    GraphDirectedness directedness() const noexcept;
    GraphWeighting    weighting()    const noexcept;

    // Returns neighbours of v as a contiguous view. No allocation.
    std::span<const NodeId>  neighbors(NodeId v) const noexcept;
    std::span<const double>  edge_weights(NodeId v) const noexcept;

    // Stable content hash (xxh3) of (sorted edge list + weights + flags).
    // Used to fingerprint a graph in result manifests.
    std::uint64_t content_hash() const noexcept;

    // Human-readable origin: filename, generator parameters, etc.
    const std::string& origin_label() const noexcept;

private:
    /* CSR arrays, flags, cached hash */
};

class GraphBuilder {
public:
    GraphBuilder(GraphDirectedness, GraphWeighting);
    void reserve_nodes(NodeId n);
    void add_edge(NodeId u, NodeId v, double w = 1.0);
    Graph build(std::string origin_label) &&;  // consumes
};

} // namespace entropy
```

**Notes:**
- CSR was chosen because every entropy algorithm we care about traverses neighbours; adjacency matrix is built on demand via `GraphCache` only when a spectral method needs it.
- `Graph` is immutable. This is non-negotiable; it makes the cache trivially correct and parallelism easy later.
- `content_hash` is what gets logged. Two graphs with the same hash are byte-identical in topology + weights.

### 4.2 `GraphCache` — lazy shared precomputation

```cpp
// include/entropy/core/graph_cache.hpp
#pragma once
#include "graph.hpp"
#include <Eigen/Sparse>
#include <Eigen/Dense>
#include <mutex>
#include <optional>

namespace entropy {

class GraphCache {
public:
    explicit GraphCache(const Graph& g) noexcept;

    const std::vector<std::uint32_t>& degrees() const;
    const std::vector<double>&        degree_distribution() const; // normalized
    const Eigen::SparseMatrix<double>& adjacency_matrix() const;
    const Eigen::SparseMatrix<double>& laplacian() const;
    const Eigen::SparseMatrix<double>& normalized_laplacian() const;

    // Eigenvalues are expensive; only request if you need them.
    // Returns ascending eigenvalues of the normalized Laplacian.
    const Eigen::VectorXd& normalized_laplacian_spectrum() const;

    // Stationary distribution of the simple random walk (if it exists).
    const Eigen::VectorXd& random_walk_stationary() const;

private:
    const Graph& g_;
    // mutable members + std::once_flag per cached quantity
};

} // namespace entropy
```

**Why this matters:** Running Von Neumann + Rényi-spectral + spectral-radius-based algorithms in the same experiment recomputes the same spectrum three times without this. The cache lives for the duration of one `Graph`'s appearance in the experiment.

### 4.3 `Parameters` — algorithm hyperparameters

```cpp
// include/entropy/core/parameters.hpp
#pragma once
#include <map>
#include <string>
#include <variant>
#include <stdexcept>

namespace entropy {

using ParamValue = std::variant<bool, std::int64_t, double, std::string>;

class Parameters {
public:
    void set(std::string key, ParamValue v);
    bool contains(const std::string& key) const noexcept;

    template <typename T>
    T get(const std::string& key) const;     // throws if missing or wrong type

    template <typename T>
    T get_or(const std::string& key, T fallback) const noexcept;

    // For logging / manifest serialization
    nlohmann::json to_json() const;

private:
    std::map<std::string, ParamValue> kv_;
};

} // namespace entropy
```

### 4.4 `Requirements` — what an algorithm needs from a graph

```cpp
// include/entropy/core/requirements.hpp
#pragma once
#include <cstdint>
#include <string>
#include <optional>

namespace entropy {

struct Requirements {
    bool undirected     = false;  // require GraphDirectedness::Undirected
    bool simple         = false;  // no self-loops, no parallel edges
    bool connected      = false;  // require connectivity (one component)
    bool unweighted_ok  = true;   // is unweighted input acceptable?
    bool weighted_ok    = true;   // is weighted input acceptable?
    std::optional<NodeId> max_nodes;  // skip if larger (e.g. dense spectral)
};

struct RequirementCheck {
    bool ok;
    std::string reason; // empty iff ok
};

RequirementCheck check(const Graph& g, const Requirements& r);

} // namespace entropy
```

The runner calls `check(...)` before invoking the algorithm. If it fails, the algorithm is skipped and the skip is logged in the manifest with the reason. No silent failures.

### 4.5 `IEntropyAlgorithm` — the algorithm interface

```cpp
// include/entropy/core/i_entropy_algorithm.hpp
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

    // Stable identifier used in configs, results, and Python. Must be unique.
    // Convention: snake_case, e.g. "von_neumann", "renyi_spectral".
    virtual std::string_view name() const noexcept = 0;

    // Free-form human-readable description for the manifest.
    virtual std::string_view description() const noexcept = 0;

    // What this algorithm needs from a graph.
    virtual Requirements requirements() const noexcept = 0;

    // Hyperparameters with their defaults and types. Used to validate configs
    // and to auto-fill the manifest.
    virtual Parameters default_parameters() const = 0;

    // The core computation. Algorithms MUST be pure: no global state,
    // no mutation of g or cache (cache members may be lazily populated -
    // that's why cache is non-const above; conceptually it's still pure).
    virtual AlgorithmOutput compute(const Graph& g,
                                    GraphCache&  cache,
                                    const Parameters& params) const = 0;
};

} // namespace entropy
```

### 4.6 `AlgorithmOutput` and `ResultSet`

```cpp
// include/entropy/core/result.hpp
#pragma once
#include <chrono>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace entropy {

struct AlgorithmOutput {
    double value;                              // primary scalar result
    std::optional<std::vector<double>> aux;    // optional auxiliary data
    nlohmann::json metadata = nlohmann::json::object();  // numerical diagnostics
};

struct RunRecord {
    std::string algorithm_name;
    std::string graph_label;
    std::uint64_t graph_content_hash;

    nlohmann::json parameters;
    nlohmann::json output_metadata;

    double value;
    std::vector<double> value_repeats;         // per-repeat values (should match modulo determinism)
    std::vector<double> wall_ms_repeats;       // one entry per timed run

    // Computed statistics
    double wall_ms_median;
    double wall_ms_p05;
    double wall_ms_p95;

    bool skipped = false;
    std::string skip_reason;
};

class ResultSet {
public:
    void add(RunRecord r);
    nlohmann::json to_json() const;
    void write_json(const std::string& path) const;
    void write_csv (const std::string& path) const;  // long-format, one row per repeat
};

} // namespace entropy
```

**Why long-format CSV:** pandas / seaborn / ggplot consume long-format trivially. Wide-format CSVs are pain.

### 4.7 Self-registering loaders and algorithms

This is the keystone that makes adding code zero-touch on the core.

```cpp
// include/entropy/core/algorithm_registry.hpp
#pragma once
#include "i_entropy_algorithm.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace entropy {

class AlgorithmRegistry {
public:
    using Factory = std::function<std::unique_ptr<IEntropyAlgorithm>()>;

    static AlgorithmRegistry& instance();

    // Returns false (without overwriting) if name is already registered.
    bool register_factory(std::string name, Factory f);

    std::unique_ptr<IEntropyAlgorithm> create(const std::string& name) const;
    std::vector<std::string> list_names() const;

private:
    std::unordered_map<std::string, Factory> factories_;
};

// Use this in each algorithm .cpp file at namespace scope:
//
//   namespace {
//     const bool registered_von_neumann = entropy::AlgorithmRegistry::instance()
//         .register_factory("von_neumann",
//             [] { return std::make_unique<VonNeumannEntropy>(); });
//   }
//
// A small helper macro is provided to remove boilerplate:
#define ENTROPY_REGISTER_ALGORITHM(KEY, CLASS)                              \
    namespace {                                                             \
        const bool _reg_##CLASS = ::entropy::AlgorithmRegistry::instance()  \
            .register_factory(KEY, []{ return std::make_unique<CLASS>(); });\
    }

} // namespace entropy
```

`GraphRegistry` is analogous, with `IGraphLoader` instead.

**Important CMake detail:** static initializers in object files that are not referenced may be dropped by the linker. To prevent that, build the algorithm/loader sources into an **OBJECT library** and link with `-Wl,--whole-archive` (GCC/Clang) or `/WHOLEARCHIVE` (MSVC). The provided `CMakeLists.txt` must do this — call it out explicitly in a comment in the build file. This is the #1 thing people get wrong with self-registration.

### 4.8 `ExperimentRunner`

```cpp
// include/entropy/core/experiment_runner.hpp
#pragma once
#include "graph_registry.hpp"
#include "algorithm_registry.hpp"
#include "result.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace entropy {

struct AlgorithmSpec {
    std::string name;
    Parameters  params;
};

struct GraphSpec {
    std::string loader_name;        // e.g. "edgelist", "erdos_renyi"
    Parameters  loader_params;      // e.g. path, n, p, seed
};

struct ExperimentConfig {
    std::string name;
    std::vector<GraphSpec>     graphs;
    std::vector<AlgorithmSpec> algorithms;

    int repeats   = 5;
    int warmups   = 1;
    std::uint64_t seed = 0;         // base seed; per-graph seed derived deterministically

    std::string output_dir;         // results/<name>/<utc_timestamp>/
};

class ExperimentRunner {
public:
    explicit ExperimentRunner(ExperimentConfig cfg);
    ResultSet run();                // produces ResultSet AND writes manifest.json
private:
    ExperimentConfig cfg_;
};

} // namespace entropy
```

---

## 5. Reproducibility Contract

This is the hardest part to get right and the most important. Every result file MUST contain a `manifest.json` with **all** of:

1. **Code identity**
   - `git_commit`: full SHA, baked in at CMake configure time via `git rev-parse HEAD`.
   - `git_dirty`: bool; whether the working tree had uncommitted changes.
   - `build_type`: Release / RelWithDebInfo / Debug.
   - `compiler`: e.g. `gcc-13.2.0`.
2. **Config identity**
   - The exact TOML config, embedded verbatim.
   - `config_sha256`.
3. **Per-graph identity**
   - `origin_label` (filename or generator string).
   - `content_hash` (xxh3 of the canonical edge representation).
   - `num_nodes`, `num_edges`, `directedness`, `weighting`.
4. **RNG provenance**
   - Base seed.
   - Per-(graph, algorithm, repeat) derived seed = `splitmix64(base ^ hash(graph_name) ^ hash(algo_name) ^ repeat_index)`.
   - All randomness in the system MUST go through `entropy::util::Rng` — no calls to global RNGs, no `std::rand`, no time-based seeding.
5. **Run records** (as defined in §4.6).

`git_dirty == true` should be allowed (it's a sandbox, not production) but the manifest must clearly mark it.

---

## 6. Numerical Conventions

- All entropies are returned **in nats** (natural log). If a user wants bits, they can divide by `ln(2)` in post. Document this in `docs/`.
- Use `entropy::util::safe_log(p)` which returns `0` when `p == 0` (with `p > 0` precondition documented), implementing the `0·log(0) = 0` convention by convention at the caller. Provide also `xlogx(p)` returning `p * log(p)` with `p ≤ 0 → 0`.
- All probability vectors must be normalized to sum to 1 within `1e-12` before any entropy formula is applied. Algorithms should normalize themselves and assert this; they should not trust callers.
- Spectral methods: use `Eigen::SelfAdjointEigenSolver` for symmetric Laplacians. Never call a general nonsymmetric solver when you can avoid it.

---

## 7. Configuration File Format (TOML)

```toml
# configs/example.toml
name    = "vn_vs_renyi_real_world"
repeats = 10
warmups = 2
seed    = 42
output_dir = "results"

[[graphs]]
loader = "edgelist"
[graphs.params]
path = "data/snap/ca-GrQc.txt"
directed = false

[[graphs]]
loader = "erdos_renyi"
[graphs.params]
n = 500
p = 0.02
seed = 1

[[algorithms]]
name = "von_neumann"

[[algorithms]]
name = "renyi_spectral"
[algorithms.params]
alpha = 2.0

[[algorithms]]
name = "shannon_degree"
```

The CLI parses this into `ExperimentConfig`. CLI flags can override individual fields:

```
entropy-cli --config configs/example.toml --repeats 20 --output-dir /tmp/run1
```

---

## 8. Benchmarking Methodology

A single timed run is not data. Implement this exactly:

1. **Warm-up:** run the algorithm `cfg.warmups` times on the graph, discard results and timings.
2. **Measure:** run `cfg.repeats` times, recording wall time per run via `std::chrono::steady_clock`.
3. **Determinism check:** all repeats must yield the same `value` modulo a tolerance of `1e-9` (relative). If they don't, mark `nondeterministic = true` in the run record and log a warning. (Some algorithms — randomized estimators — will legitimately differ; they should opt out of the determinism assertion in their `output.metadata`.)
4. **Stats:** record median, p05, p95 of wall time. Don't report mean (latency distributions are skewed).
5. **Outlier policy:** none at the C++ layer; raw repeats are preserved in long-format CSV so Python can do robust analysis.

---

## 9. Logging & Error Handling

- **Logging:** spdlog with two sinks — colored stderr at `info` and a per-run rotating file at `debug` in the run output dir.
- **Errors:** algorithms that cannot proceed for principled reasons (e.g. graph violates `Requirements`) result in a `skipped` `RunRecord`, not an exception. Unexpected errors propagate as `std::runtime_error`-derived exceptions, are caught by the runner, and produce a `skipped` record with `skip_reason = "exception: <what>"`. The runner continues with the next (graph, algorithm) pair. **The runner never aborts the whole experiment on a single failure.**
- **Assertions:** `ENTROPY_ASSERT(...)` macro that always evaluates in Debug, no-ops in Release, and a separate `ENTROPY_CHECK(...)` that always throws on failure for invariants that must hold at runtime.

---

## 10. Testing Strategy

Four layers:

1. **Unit tests** (`tests/unit/`): each algorithm against analytically known values:
   - Complete graph K_n: degree distribution is uniform, Shannon-degree entropy = `log(n-1)` (or `log(n)` depending on convention — document and pick one).
   - Star graph S_n: known degree entropy.
   - Path P_n and cycle C_n: known Laplacian eigenvalues, so Von Neumann entropy is computable in closed form.
   - Disconnected two-component graph: connectivity requirement triggers correctly.
2. **Property tests:** for any random graph G, entropies satisfy invariants (e.g. Rényi entropy is non-increasing in α; Von Neumann entropy is bounded by `log(n)`).
3. **Golden / snapshot tests:** run a fixed config against a small fixed graph; compare full `manifest.json` modulo timestamps and timings.
4. **Sanitizer CI:** the unit-test suite runs under ASan+UBSan in one CI job and under TSan in another. Both must pass.

---

## 11. Python Interop (Optional but Recommended)

Expose the same registry to Python via pybind11. The Python package is for **analysis and quick prototyping**, not the main API.

```python
import pyentropy_sandbox as es

g = es.load_edgelist("data/snap/ca-GrQc.txt", directed=False)
vn = es.algorithm("von_neumann").compute(g)
print(vn.value, vn.metadata)

# Or: run an experiment defined in TOML and get a pandas DataFrame
df = es.run_experiment("configs/example.toml").to_dataframe()
```

The C++ source of truth doesn't depend on Python. The Python module is an optional CMake target.

---

## 12. Adding a New Algorithm — Worked Example

Suppose we are adding **Rényi entropy of the degree distribution**, parameterized by `alpha`.

**Step 1.** Create `src/algorithms/renyi_degree_entropy.cpp`. No other file is touched.

```cpp
#include "entropy/core/i_entropy_algorithm.hpp"
#include "entropy/core/algorithm_registry.hpp"
#include "entropy/util/numerics.hpp"
#include <cmath>

namespace entropy {

class RenyiDegreeEntropy final : public IEntropyAlgorithm {
public:
    std::string_view name() const noexcept override { return "renyi_degree"; }
    std::string_view description() const noexcept override {
        return "Rényi entropy of order alpha of the normalized degree distribution.";
    }

    Requirements requirements() const noexcept override {
        Requirements r;
        r.undirected = true;
        r.simple     = false;     // self-loops are fine here
        return r;
    }

    Parameters default_parameters() const override {
        Parameters p;
        p.set("alpha", 2.0);
        return p;
    }

    AlgorithmOutput compute(const Graph& g,
                            GraphCache&  cache,
                            const Parameters& params) const override
    {
        const double alpha = params.get_or<double>("alpha", 2.0);
        if (alpha <= 0.0 || alpha == 1.0) {
            throw std::runtime_error("renyi_degree: alpha must be > 0 and != 1");
        }
        const auto& p = cache.degree_distribution(); // already normalized
        double s = 0.0;
        for (double pi : p) if (pi > 0.0) s += std::pow(pi, alpha);
        const double value = std::log(s) / (1.0 - alpha);

        AlgorithmOutput out;
        out.value = value;
        out.metadata["alpha"] = alpha;
        out.metadata["distribution_size"] = p.size();
        return out;
    }
};

ENTROPY_REGISTER_ALGORITHM("renyi_degree", RenyiDegreeEntropy)

} // namespace entropy
```

**Step 2.** Add a unit test in `tests/unit/test_renyi_degree.cpp` (one file).

**Step 3.** Rebuild. Run `entropy-cli --list-algorithms`. The new algorithm appears. Reference it by name in any TOML config.

That's the entire workflow. No central file edits, no recompiling anything in `src/core/`, no manual registration list.

---

## 13. Adding a New Graph Loader

Identical pattern: one file in `src/loaders/`, derives from `IGraphLoader`, registers itself via `ENTROPY_REGISTER_LOADER("snap", SnapLoader)`.

---

## 14. CMake Outline (Critical Details Only)

```cmake
cmake_minimum_required(VERSION 3.25)
project(entropy_sandbox LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)

# --- Git version baked in ---
execute_process(COMMAND git rev-parse HEAD
                OUTPUT_VARIABLE GIT_SHA OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND git diff --quiet
                RESULT_VARIABLE GIT_DIRTY)
configure_file(include/entropy/util/version.hpp.in
               ${CMAKE_BINARY_DIR}/generated/entropy/util/version.hpp @ONLY)

# --- Core library ---
add_library(entropy_core STATIC
    src/core/graph.cpp
    src/core/graph_cache.cpp
    src/core/algorithm_registry.cpp
    src/core/graph_registry.cpp
    src/core/experiment_runner.cpp
    src/io/result_exporter.cpp
)
target_include_directories(entropy_core PUBLIC include ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(entropy_core PUBLIC Eigen3::Eigen nlohmann_json::nlohmann_json spdlog::spdlog xxhash)

# --- Plugins: OBJECT library + whole-archive linkage so static reg works ---
add_library(entropy_plugins OBJECT
    src/algorithms/shannon_degree_entropy.cpp
    src/algorithms/von_neumann_entropy.cpp
    src/algorithms/renyi_spectral_entropy.cpp
    src/algorithms/structural_entropy.cpp
    src/loaders/edgelist_loader.cpp
    src/loaders/graphml_loader.cpp
    src/loaders/snap_loader.cpp
    src/loaders/erdos_renyi_generator.cpp
    src/loaders/barabasi_albert_generator.cpp
)
target_link_libraries(entropy_plugins PUBLIC entropy_core)

# --- CLI app: force-link all plugin TUs so static initializers run ---
add_executable(entropy-cli apps/cli/main.cpp)
target_link_libraries(entropy-cli PRIVATE entropy_core CLI11::CLI11 tomlplusplus::tomlplusplus)
if (MSVC)
    target_link_options(entropy-cli PRIVATE /WHOLEARCHIVE:$<TARGET_FILE:entropy_plugins>)
else()
    target_link_libraries(entropy-cli PRIVATE
        "-Wl,--whole-archive" $<TARGET_OBJECTS:entropy_plugins> "-Wl,--no-whole-archive")
endif()
```

The whole-archive trick is essential. Without it, the linker silently drops every `ENTROPY_REGISTER_ALGORITHM` static initializer in unused TUs and the registry comes up empty.

---

## 15. Result File Layout

After `entropy-cli --config configs/example.toml` runs:

```
results/vn_vs_renyi_real_world/2025-05-15T14-32-01Z/
├── manifest.json          # config, code version, all metadata
├── results.json           # full ResultSet
├── results.csv            # long-format: one row per (graph, algo, repeat)
└── run.log                # spdlog debug-level file sink
```

This directory is the unit of scientific reproducibility. It is sufficient to reproduce, audit, and plot.

---

## 16. Implementation Phases

Implement in this order. Do not skip ahead — each phase makes the next easier to test.

**Phase 1 — Skeleton (gets a smoke test green):**
- `Graph`, `GraphBuilder`, CSR storage.
- `IEntropyAlgorithm`, `AlgorithmRegistry` with self-registration + whole-archive linkage verified.
- One loader: `edgelist`.
- One algorithm: `shannon_degree`.
- `ExperimentRunner` (single repeat, no benchmarking statistics yet).
- TOML config parsing.
- `manifest.json` + `results.json` writing.
- Unit test: K_4 → known Shannon-degree entropy.
- **Exit criterion:** `entropy-cli --config configs/smoke.toml` produces a valid manifest.

**Phase 2 — Real algorithms:**
- `GraphCache` with degree distribution + Laplacian + spectrum.
- `IGraphLoader` interface + `erdos_renyi` and `barabasi_albert` generators.
- `von_neumann`, `renyi_spectral`, `renyi_degree`.
- `Requirements` enforcement and skip records.

**Phase 3 — Scientific rigor:**
- Warm-ups + repeats + percentile timings.
- Per-run derived seeds via splitmix64.
- Long-format CSV exporter.
- Property tests + golden snapshot test.

**Phase 4 — Ergonomics:**
- More loaders: `graphml`, `snap`.
- `structural_entropy` (Li & Pan).
- pybind11 module + a single notebook in `notebooks/` showing a plotted comparison.

**Phase 5 — Hardening:**
- ASan/UBSan/TSan CI.
- Doxygen on the public headers.
- `docs/adding_an_algorithm.md` and `docs/reproducibility.md`.

---

## 17. Things Explicitly Out of Scope (Do Not Build)

- GUI / OpenGL viewer.
- Database backend.
- Distributed runner.
- GPU eigensolvers.
- A DSL for describing graphs.
- Anything in `core/` that knows about a specific algorithm or loader by name.

---

## 18. Common Pitfalls to Pre-empt

1. **Static initializer drop.** Without whole-archive linkage, the registry is empty. The CLI should print `Loaded N algorithms, M loaders` on startup and fail loudly if either is zero.
2. **`std::mt19937` is not deterministic across platforms when seeded from `std::seed_seq`.** Use a fixed splitmix64 expansion of the seed and `std::mt19937_64` directly.
3. **Eigen's `SelfAdjointEigenSolver` allocates.** For repeated runs on the same matrix, reuse the solver object; or just accept the allocation cost — it's dwarfed by the eigendecomposition itself.
4. **Floating-point sums are order-dependent.** For determinism across thread counts, do not parallelize reductions inside an algorithm in v1.
5. **`size_t` vs `uint32_t` for `NodeId`.** Standardize on `uint32_t` everywhere; 4G nodes is well past what fits in RAM for spectral methods anyway.
6. **TOML strings are UTF-8.** File paths from configs must round-trip through `std::filesystem::path::u8string` on Windows. Don't assume narrow `char*`.

---

## 19. Definition of Done for v1

- `entropy-cli --list-algorithms` lists at least 4 algorithms and 4 loaders.
- A full smoke experiment runs end-to-end and produces `manifest.json`, `results.json`, `results.csv`, `run.log`.
- Adding a new algorithm requires editing exactly **zero** files in `src/core/`, `include/entropy/core/`, `apps/`, or `CMakeLists.txt` (the new file is auto-globbed or, preferably, added explicitly to the `entropy_plugins` OBJECT library — that one-line addition is the only existing-file change).
- A Python notebook in `notebooks/` loads `results.csv` into pandas and produces a comparison plot.
- All unit and property tests pass under ASan+UBSan.

End of specification.
