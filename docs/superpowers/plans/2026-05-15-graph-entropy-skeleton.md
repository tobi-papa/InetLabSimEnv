# Graph Entropy Sandbox — Skeleton Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a compilable C++ research sandbox skeleton with one algorithm (`structural_entropy`), one loader (`edgelist`), and a working CLI that produces `manifest.json`, `results.json`, `results.csv`, and `run.log` on a smoke run.

**Architecture:** Self-registering plugin pattern: algorithms and loaders live in `src/algorithms/` and `src/loaders/`, register themselves via a static initializer macro (`ENTROPY_REGISTER_ALGORITHM`), and are force-linked via an OBJECT library + `--whole-archive`. The core library (`entropy_core`) knows nothing about specific algorithm names. `ExperimentRunner` orchestrates graph loading, requirement checking, timed compute, and output writing.

**Tech Stack:** C++20, CMake 3.25+ with FetchContent, Eigen 3.4 (headers), nlohmann/json 3.11, toml++ 3.4, CLI11 2.4, spdlog 1.14, xxhash 0.8.2.

---

## File Map

```
graph-entropy-sandbox/
├── CMakeLists.txt
├── cmake/
│   ├── CompilerWarnings.cmake
│   └── Sanitizers.cmake
├── include/entropy/
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
│       ├── numerics.hpp
│       ├── rng.hpp
│       └── version.hpp.in
├── src/
│   ├── core/
│   │   ├── graph.cpp
│   │   ├── graph_cache.cpp
│   │   ├── algorithm_registry.cpp
│   │   ├── graph_registry.cpp
│   │   ├── parameters.cpp
│   │   ├── requirements.cpp
│   │   ├── result.cpp
│   │   └── experiment_runner.cpp
│   ├── io/
│   │   ├── config.cpp
│   │   └── result_exporter.cpp
│   ├── algorithms/
│   │   └── structural_entropy.cpp
│   └── loaders/
│       └── edgelist_loader.cpp
├── apps/cli/main.cpp
├── configs/smoke.toml
└── data/samples/k4.txt
```

---

## Task 1: Directory Scaffold + CMake Helper Modules

**Files:**
- Create: `graph-entropy-sandbox/` (root directory)
- Create: `cmake/CompilerWarnings.cmake`
- Create: `cmake/Sanitizers.cmake`
- Create: `.gitignore`

- [ ] Create the root directory and all subdirectories:

```bash
cd /home/tobi/SimEnv
mkdir -p graph-entropy-sandbox/{cmake,apps/cli,apps/bench}
mkdir -p graph-entropy-sandbox/include/entropy/{core,io,util}
mkdir -p graph-entropy-sandbox/src/{core,io,algorithms,loaders}
mkdir -p graph-entropy-sandbox/{configs,data/samples,results,docs,notebooks}
mkdir -p graph-entropy-sandbox/tests/{unit,property,golden}
mkdir -p graph-entropy-sandbox/bindings/python
```

- [ ] Create `graph-entropy-sandbox/.gitignore`:

```gitignore
build/
results/
.cache/
*.o
*.a
*.so
CMakeCache.txt
CMakeFiles/
cmake_install.cmake
Makefile
```

- [ ] Create `graph-entropy-sandbox/data/samples/.gitkeep`:

```
```

(empty file — keeps the directory in git)

- [ ] Create `graph-entropy-sandbox/cmake/CompilerWarnings.cmake`:

```cmake
function(apply_compiler_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Werror
            -Wno-unused-parameter
        )
    endif()
endfunction()
```

- [ ] Create `graph-entropy-sandbox/cmake/Sanitizers.cmake`:

```cmake
option(ENTROPY_ENABLE_ASAN  "Enable AddressSanitizer + UBSan" OFF)
option(ENTROPY_ENABLE_TSAN  "Enable ThreadSanitizer"          OFF)

function(apply_sanitizers target)
    if(ENTROPY_ENABLE_ASAN)
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE
            -fsanitize=address,undefined)
    endif()
    if(ENTROPY_ENABLE_TSAN)
        target_compile_options(${target} PRIVATE -fsanitize=thread)
        target_link_options(${target} PRIVATE   -fsanitize=thread)
    endif()
endfunction()
```

- [ ] Commit:

```bash
cd /home/tobi/SimEnv/graph-entropy-sandbox
git init
git add cmake/ .gitignore data/
git commit -m "chore: project scaffold and cmake helper modules"
```

---

## Task 2: Utility Headers

**Files:**
- Create: `include/entropy/util/numerics.hpp`
- Create: `include/entropy/util/rng.hpp`
- Create: `include/entropy/util/version.hpp.in`

- [ ] Create `include/entropy/util/numerics.hpp`:

```cpp
#pragma once
#include <cmath>

namespace entropy::util {

// Returns p * ln(p), with the convention 0 * ln(0) := 0.
// Precondition: p >= 0.
inline double xlogx(double p) noexcept {
    if (p <= 0.0) return 0.0;
    return p * std::log(p);
}

// Returns ln(p). Returns 0.0 when p <= 0 (caller is responsible for
// guarding p == 0 via the 0*ln(0) = 0 convention at the call site).
inline double safe_log(double p) noexcept {
    if (p <= 0.0) return 0.0;
    return std::log(p);
}

} // namespace entropy::util
```

- [ ] Create `include/entropy/util/rng.hpp`:

```cpp
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
```

- [ ] Create `include/entropy/util/version.hpp.in`
(CMake substitutes `@VAR@` at configure time into the generated header):

```cpp
#pragma once
#include <string_view>

namespace entropy::version {

constexpr std::string_view git_commit = "@GIT_SHA@";
constexpr bool             git_dirty  = @GIT_DIRTY_BOOL@;
constexpr std::string_view build_type = "@CMAKE_BUILD_TYPE@";

} // namespace entropy::version
```

- [ ] Commit:

```bash
git add include/entropy/util/
git commit -m "feat: utility headers — numerics, rng, version template"
```

---

## Task 3: Graph Header

**Files:**
- Create: `include/entropy/core/graph.hpp`

- [ ] Create `include/entropy/core/graph.hpp`:

```cpp
#pragma once
#include <cstdint>
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

    NodeId num_nodes()    const noexcept;
    EdgeId num_edges()    const noexcept; // undirected edges counted once
    GraphDirectedness directedness() const noexcept;
    GraphWeighting    weighting()    const noexcept;

    // Zero-allocation neighbour views via std::span.
    std::span<const NodeId> neighbors   (NodeId v) const noexcept;
    std::span<const double> edge_weights(NodeId v) const noexcept;

    // xxh3 content hash of (sorted edge list + weights + flags).
    std::uint64_t      content_hash()  const noexcept;
    const std::string& origin_label()  const noexcept;

private:
    std::vector<std::uint64_t> row_ptr_;
    std::vector<NodeId>         col_idx_;
    std::vector<double>         weights_;
    GraphDirectedness           directedness_;
    GraphWeighting              weighting_;
    std::string                 origin_label_;
    EdgeId                      num_edges_;
    mutable std::uint64_t       cached_hash_    = 0;
    mutable bool                hash_computed_  = false;
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
```

- [ ] Commit:

```bash
git add include/entropy/core/graph.hpp
git commit -m "feat: graph.hpp — CSR graph interface"
```

---

## Task 4: Core Type Headers (Parameters, Requirements, Result)

**Files:**
- Create: `include/entropy/core/parameters.hpp`
- Create: `include/entropy/core/requirements.hpp`
- Create: `include/entropy/core/result.hpp`

- [ ] Create `include/entropy/core/parameters.hpp`:

```cpp
#pragma once
#include <map>
#include <string>
#include <variant>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace entropy {

using ParamValue = std::variant<bool, std::int64_t, double, std::string>;

class Parameters {
public:
    void set(std::string key, ParamValue v);
    bool contains(const std::string& key) const noexcept;

    template <typename T>
    T get(const std::string& key) const {
        auto it = kv_.find(key);
        if (it == kv_.end())
            throw std::out_of_range("Parameter not found: " + key);
        return std::get<T>(it->second); // throws std::bad_variant_access if wrong type
    }

    template <typename T>
    T get_or(const std::string& key, T fallback) const noexcept {
        auto it = kv_.find(key);
        if (it == kv_.end()) return fallback;
        if (const T* val = std::get_if<T>(&it->second)) return *val;
        return fallback;
    }

    nlohmann::json to_json() const;

private:
    std::map<std::string, ParamValue> kv_;
};

} // namespace entropy
```

- [ ] Create `include/entropy/core/requirements.hpp`:

```cpp
#pragma once
#include "graph.hpp"
#include <optional>
#include <string>

namespace entropy {

struct Requirements {
    bool undirected    = false; // require GraphDirectedness::Undirected
    bool simple        = false; // no self-loops, no parallel edges (check is TODO Phase 3)
    bool connected     = false; // single connected component (check is TODO Phase 3)
    bool unweighted_ok = true;  // accept unweighted graphs
    bool weighted_ok   = true;  // accept weighted graphs
    std::optional<NodeId> max_nodes; // skip if graph is larger
};

struct RequirementCheck {
    bool        ok;
    std::string reason; // empty iff ok == true
};

RequirementCheck check(const Graph& g, const Requirements& r);

} // namespace entropy
```

- [ ] Create `include/entropy/core/result.hpp`:

```cpp
#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace entropy {

struct AlgorithmOutput {
    double         value;
    std::optional<std::vector<double>> aux;
    nlohmann::json metadata = nlohmann::json::object();
};

struct RunRecord {
    std::string   algorithm_name;
    std::string   graph_label;
    std::uint64_t graph_content_hash = 0;

    nlohmann::json parameters;
    nlohmann::json output_metadata;

    double              value = 0.0;
    std::vector<double> value_repeats;
    std::vector<double> wall_ms_repeats;

    double wall_ms_median = 0.0;
    double wall_ms_p05    = 0.0;
    double wall_ms_p95    = 0.0;

    bool        skipped     = false;
    std::string skip_reason;
};

class ResultSet {
public:
    void           add(RunRecord r);
    nlohmann::json to_json() const;
    void           write_json(const std::string& path) const;
    void           write_csv (const std::string& path) const; // long-format

    const std::vector<RunRecord>& records() const noexcept { return records_; }

private:
    std::vector<RunRecord> records_;
};

} // namespace entropy
```

- [ ] Commit:

```bash
git add include/entropy/core/parameters.hpp \
        include/entropy/core/requirements.hpp \
        include/entropy/core/result.hpp
git commit -m "feat: core type headers — parameters, requirements, result"
```

---

## Task 5: Core Interface Headers (GraphCache, IEntropyAlgorithm, IGraphLoader)

**Files:**
- Create: `include/entropy/core/graph_cache.hpp`
- Create: `include/entropy/core/i_entropy_algorithm.hpp`
- Create: `include/entropy/core/i_graph_loader.hpp`

- [ ] Create `include/entropy/core/graph_cache.hpp`:

```cpp
#pragma once
#include "graph.hpp"
#include <Eigen/Sparse>
#include <Eigen/Dense>
#include <mutex>
#include <vector>

namespace entropy {

// Lazy cache of expensive derived graph properties.
// Constructed around an immutable Graph; lives for the duration of one experiment run.
// Thread-safety: each property is initialized with std::call_once (safe for concurrent reads
// after first initialization, but this codebase is single-threaded in v1).
class GraphCache {
public:
    explicit GraphCache(const Graph& g) noexcept;

    // Implemented in Phase 1:
    const std::vector<std::uint32_t>& degrees()            const;
    const std::vector<double>&        degree_distribution() const; // normalized, sums to 1

    // Stubbed — throws std::runtime_error("not yet implemented").
    // Fill these in when adding spectral algorithms (Phase 2+).
    const Eigen::SparseMatrix<double>& adjacency_matrix()          const;
    const Eigen::SparseMatrix<double>& laplacian()                  const;
    const Eigen::SparseMatrix<double>& normalized_laplacian()       const;
    const Eigen::VectorXd&             normalized_laplacian_spectrum() const;
    const Eigen::VectorXd&             random_walk_stationary()     const;

private:
    const Graph& g_;

    mutable std::once_flag              degrees_flag_;
    mutable std::vector<std::uint32_t>  degrees_;

    mutable std::once_flag              dist_flag_;
    mutable std::vector<double>         degree_distribution_;

    // Placeholders for spectral data (not initialized in Phase 1)
    mutable Eigen::SparseMatrix<double> adjacency_matrix_;
    mutable Eigen::SparseMatrix<double> laplacian_;
    mutable Eigen::SparseMatrix<double> normalized_laplacian_;
    mutable Eigen::VectorXd             normalized_laplacian_spectrum_;
    mutable Eigen::VectorXd             random_walk_stationary_;
};

} // namespace entropy
```

- [ ] Create `include/entropy/core/i_entropy_algorithm.hpp`:

```cpp
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

    // Stable snake_case identifier used in configs, results, and Python. Must be unique.
    virtual std::string_view name()        const noexcept = 0;

    // Human-readable description for the manifest.
    virtual std::string_view description() const noexcept = 0;

    // Declares what this algorithm needs from a graph. Checked before compute().
    virtual Requirements requirements()    const noexcept = 0;

    // Returns a Parameters object with all supported keys set to their defaults.
    // Used for manifest serialization and config validation.
    virtual Parameters default_parameters() const = 0;

    // Core computation. Must be pure: no global state, no mutation of g or cache.
    // (GraphCache& is non-const only because lazy init populates mutable fields.)
    virtual AlgorithmOutput compute(const Graph&      g,
                                    GraphCache&       cache,
                                    const Parameters& params) const = 0;
};

} // namespace entropy
```

- [ ] Create `include/entropy/core/i_graph_loader.hpp`:

```cpp
#pragma once
#include "graph.hpp"
#include "parameters.hpp"
#include <string_view>

namespace entropy {

class IGraphLoader {
public:
    virtual ~IGraphLoader() = default;

    // Stable snake_case identifier used in TOML configs.
    virtual std::string_view name() const noexcept = 0;

    // Load and return a Graph. Throw std::runtime_error on failure.
    virtual Graph load(const Parameters& params) const = 0;
};

} // namespace entropy
```

- [ ] Commit:

```bash
git add include/entropy/core/graph_cache.hpp \
        include/entropy/core/i_entropy_algorithm.hpp \
        include/entropy/core/i_graph_loader.hpp
git commit -m "feat: interface headers — graph_cache, i_entropy_algorithm, i_graph_loader"
```

---

## Task 6: Registry and Runner Headers

**Files:**
- Create: `include/entropy/core/algorithm_registry.hpp`
- Create: `include/entropy/core/graph_registry.hpp`
- Create: `include/entropy/core/experiment_runner.hpp`

- [ ] Create `include/entropy/core/algorithm_registry.hpp`:

```cpp
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

    // Returns false (without overwriting) if name already registered.
    bool register_factory(std::string name, Factory f);

    std::unique_ptr<IEntropyAlgorithm> create(const std::string& name) const;
    std::vector<std::string>           list_names()                     const;

private:
    std::unordered_map<std::string, Factory> factories_;
};

// Place this at namespace scope in each algorithm .cpp file (outside any function):
//
//   ENTROPY_REGISTER_ALGORITHM("my_algo", MyAlgoClass)
//
// The static initializer fires at program startup and inserts a factory into the
// registry. The OBJECT-library + --whole-archive linkage in CMakeLists.txt ensures
// this TU is not silently dropped by the linker.
#define ENTROPY_REGISTER_ALGORITHM(KEY, CLASS)                                      \
    namespace {                                                                     \
        const bool _reg_##CLASS =                                                   \
            ::entropy::AlgorithmRegistry::instance()                                \
                .register_factory(KEY, [] { return std::make_unique<CLASS>(); });   \
    }

} // namespace entropy
```

- [ ] Create `include/entropy/core/graph_registry.hpp`:

```cpp
#pragma once
#include "i_graph_loader.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace entropy {

class GraphRegistry {
public:
    using Factory = std::function<std::unique_ptr<IGraphLoader>()>;

    static GraphRegistry& instance();

    bool register_factory(std::string name, Factory f);

    std::unique_ptr<IGraphLoader> create(const std::string& name) const;
    std::vector<std::string>      list_names()                    const;

private:
    std::unordered_map<std::string, Factory> factories_;
};

#define ENTROPY_REGISTER_LOADER(KEY, CLASS)                                         \
    namespace {                                                                     \
        const bool _reg_##CLASS =                                                   \
            ::entropy::GraphRegistry::instance()                                    \
                .register_factory(KEY, [] { return std::make_unique<CLASS>(); });   \
    }

} // namespace entropy
```

- [ ] Create `include/entropy/core/experiment_runner.hpp`:

```cpp
#pragma once
#include "parameters.hpp"
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
    std::string loader_name;
    Parameters  loader_params;
};

struct ExperimentConfig {
    std::string name;
    std::vector<GraphSpec>     graphs;
    std::vector<AlgorithmSpec> algorithms;

    int           repeats   = 5;
    int           warmups   = 1;
    std::uint64_t seed      = 0;
    std::string   output_dir;

    // Populated by parse_config for manifest traceability.
    std::string config_verbatim;
    std::string config_path;
};

class ExperimentRunner {
public:
    explicit ExperimentRunner(ExperimentConfig cfg);

    // Runs the experiment and writes manifest.json, results.json, results.csv,
    // and run.log to output_dir/<name>/<utc_timestamp>/.
    ResultSet run();

private:
    ExperimentConfig cfg_;
};

} // namespace entropy
```

- [ ] Commit:

```bash
git add include/entropy/core/algorithm_registry.hpp \
        include/entropy/core/graph_registry.hpp \
        include/entropy/core/experiment_runner.hpp
git commit -m "feat: registry and runner headers"
```

---

## Task 7: IO Headers

**Files:**
- Create: `include/entropy/io/config.hpp`
- Create: `include/entropy/io/result_exporter.hpp`

- [ ] Create `include/entropy/io/config.hpp`:

```cpp
#pragma once
#include "entropy/core/experiment_runner.hpp"
#include <string>

namespace entropy::io {

// Parse a TOML file at `path` into an ExperimentConfig.
// Populates config.config_verbatim with the raw file content.
// Throws std::runtime_error on parse failure.
ExperimentConfig parse_config(const std::string& path);

} // namespace entropy::io
```

- [ ] Create `include/entropy/io/result_exporter.hpp`:

```cpp
#pragma once
#include "entropy/core/result.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace entropy::io {

// Write manifest.json, results.json, and results.csv into run_dir.
// run_dir must already exist.
void write_run_output(const std::string&    run_dir,
                      const nlohmann::json& manifest,
                      const ResultSet&      results);

} // namespace entropy::io
```

- [ ] Commit:

```bash
git add include/entropy/io/
git commit -m "feat: IO headers — config parser and result exporter"
```

---

## Task 8: Graph Implementation

**Files:**
- Create: `src/core/graph.cpp`

- [ ] Create `src/core/graph.cpp`:

```cpp
#include "entropy/core/graph.hpp"
#include <algorithm>
#include <xxhash.h>

namespace entropy {

// ---------------------------------------------------------------------------
// Graph
// ---------------------------------------------------------------------------

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
    // For undirected graphs each undirected edge is stored as two directed
    // entries; divide col_idx size by 2 to get the undirected edge count.
    const EdgeId directed_count = static_cast<EdgeId>(col_idx_.size());
    num_edges_ = (directedness_ == GraphDirectedness::Undirected)
                     ? directed_count / 2
                     : directed_count;
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

std::uint64_t Graph::content_hash() const noexcept {
    if (hash_computed_) return cached_hash_;
    XXH3_state_t* state = XXH3_createState();
    XXH3_64bits_reset(state);
    XXH3_64bits_update(state, col_idx_.data(), col_idx_.size() * sizeof(NodeId));
    XXH3_64bits_update(state, weights_.data(), weights_.size() * sizeof(double));
    const std::uint8_t flags[2] = {
        static_cast<std::uint8_t>(directedness_),
        static_cast<std::uint8_t>(weighting_)
    };
    XXH3_64bits_update(state, flags, sizeof(flags));
    cached_hash_   = XXH3_64bits_digest(state);
    hash_computed_ = true;
    XXH3_freeState(state);
    return cached_hash_;
}

const std::string& Graph::origin_label() const noexcept { return origin_label_; }

// ---------------------------------------------------------------------------
// GraphBuilder
// ---------------------------------------------------------------------------

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

    // For undirected graphs, add the reverse of each non-self-loop edge.
    std::vector<Edge> all_edges = edges_;
    if (directedness_ == GraphDirectedness::Undirected) {
        for (const auto& e : edges_)
            if (e.u != e.v) all_edges.push_back({e.v, e.u, e.weight});
    }

    // Sort by source, then destination for deterministic CSR.
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
```

- [ ] Commit:

```bash
git add src/core/graph.cpp
git commit -m "feat: graph.cpp — CSR construction and content hashing"
```

---

## Task 9: GraphCache Implementation

**Files:**
- Create: `src/core/graph_cache.cpp`

- [ ] Create `src/core/graph_cache.cpp`:

```cpp
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
```

- [ ] Commit:

```bash
git add src/core/graph_cache.cpp
git commit -m "feat: graph_cache.cpp — degrees and degree_distribution implemented, spectral stubs"
```

---

## Task 10: Parameters, Requirements, and Result Implementations

**Files:**
- Create: `src/core/parameters.cpp`
- Create: `src/core/requirements.cpp`
- Create: `src/core/result.cpp`

- [ ] Create `src/core/parameters.cpp`:

```cpp
#include "entropy/core/parameters.hpp"

namespace entropy {

void Parameters::set(std::string key, ParamValue v) {
    kv_[std::move(key)] = std::move(v);
}

bool Parameters::contains(const std::string& key) const noexcept {
    return kv_.find(key) != kv_.end();
}

nlohmann::json Parameters::to_json() const {
    nlohmann::json j = nlohmann::json::object();
    for (const auto& [k, v] : kv_) {
        std::visit([&](const auto& val) { j[k] = val; }, v);
    }
    return j;
}

} // namespace entropy
```

- [ ] Create `src/core/requirements.cpp`:

```cpp
#include "entropy/core/requirements.hpp"

namespace entropy {

RequirementCheck check(const Graph& g, const Requirements& r) {
    if (r.undirected && g.directedness() != GraphDirectedness::Undirected)
        return {false, "algorithm requires an undirected graph"};

    if (!r.unweighted_ok && g.weighting() == GraphWeighting::Unweighted)
        return {false, "algorithm requires a weighted graph"};

    if (!r.weighted_ok && g.weighting() == GraphWeighting::Weighted)
        return {false, "algorithm requires an unweighted graph"};

    if (r.max_nodes && g.num_nodes() > *r.max_nodes)
        return {false, "graph exceeds algorithm's max_nodes limit ("
                        + std::to_string(*r.max_nodes) + ")"};

    // r.simple and r.connected checks require graph traversal — TODO Phase 3.
    return {true, ""};
}

} // namespace entropy
```

- [ ] Create `src/core/result.cpp`:

```cpp
#include "entropy/core/result.hpp"
#include <fstream>
#include <stdexcept>

namespace entropy {

void ResultSet::add(RunRecord r) {
    records_.push_back(std::move(r));
}

nlohmann::json ResultSet::to_json() const {
    auto arr = nlohmann::json::array();
    for (const auto& rec : records_) {
        nlohmann::json j;
        j["algorithm"]          = rec.algorithm_name;
        j["graph_label"]        = rec.graph_label;
        j["graph_content_hash"] = rec.graph_content_hash;
        j["parameters"]         = rec.parameters;
        j["output_metadata"]    = rec.output_metadata;
        j["value"]              = rec.value;
        j["value_repeats"]      = rec.value_repeats;
        j["wall_ms_repeats"]    = rec.wall_ms_repeats;
        j["wall_ms_median"]     = rec.wall_ms_median;
        j["wall_ms_p05"]        = rec.wall_ms_p05;
        j["wall_ms_p95"]        = rec.wall_ms_p95;
        j["skipped"]            = rec.skipped;
        j["skip_reason"]        = rec.skip_reason;
        arr.push_back(j);
    }
    return arr;
}

void ResultSet::write_json(const std::string& path) const {
    std::ofstream f(path);
    if (!f) throw std::runtime_error("Cannot write: " + path);
    f << to_json().dump(2) << "\n";
}

void ResultSet::write_csv(const std::string& path) const {
    std::ofstream f(path);
    if (!f) throw std::runtime_error("Cannot write: " + path);
    f << "algorithm,graph_label,repeat_idx,value,wall_ms\n";
    for (const auto& rec : records_) {
        if (rec.skipped) continue;
        for (std::size_t i = 0; i < rec.value_repeats.size(); ++i) {
            f << rec.algorithm_name << ","
              << '"' << rec.graph_label << '"' << ","
              << i << ","
              << rec.value_repeats[i] << ","
              << rec.wall_ms_repeats[i] << "\n";
        }
    }
}

} // namespace entropy
```

- [ ] Commit:

```bash
git add src/core/parameters.cpp src/core/requirements.cpp src/core/result.cpp
git commit -m "feat: parameters, requirements, result implementations"
```

---

## Task 11: Registry Implementations

**Files:**
- Create: `src/core/algorithm_registry.cpp`
- Create: `src/core/graph_registry.cpp`

- [ ] Create `src/core/algorithm_registry.cpp`:

```cpp
#include "entropy/core/algorithm_registry.hpp"
#include <stdexcept>

namespace entropy {

AlgorithmRegistry& AlgorithmRegistry::instance() {
    static AlgorithmRegistry inst;
    return inst;
}

bool AlgorithmRegistry::register_factory(std::string name, Factory f) {
    return factories_.emplace(std::move(name), std::move(f)).second;
}

std::unique_ptr<IEntropyAlgorithm>
AlgorithmRegistry::create(const std::string& name) const {
    auto it = factories_.find(name);
    if (it == factories_.end())
        throw std::runtime_error("Unknown algorithm: '" + name
                                 + "'. Did you forget --whole-archive linkage?");
    return it->second();
}

std::vector<std::string> AlgorithmRegistry::list_names() const {
    std::vector<std::string> names;
    names.reserve(factories_.size());
    for (const auto& [k, _] : factories_) names.push_back(k);
    return names;
}

} // namespace entropy
```

- [ ] Create `src/core/graph_registry.cpp`:

```cpp
#include "entropy/core/graph_registry.hpp"
#include <stdexcept>

namespace entropy {

GraphRegistry& GraphRegistry::instance() {
    static GraphRegistry inst;
    return inst;
}

bool GraphRegistry::register_factory(std::string name, Factory f) {
    return factories_.emplace(std::move(name), std::move(f)).second;
}

std::unique_ptr<IGraphLoader>
GraphRegistry::create(const std::string& name) const {
    auto it = factories_.find(name);
    if (it == factories_.end())
        throw std::runtime_error("Unknown loader: '" + name
                                 + "'. Did you forget --whole-archive linkage?");
    return it->second();
}

std::vector<std::string> GraphRegistry::list_names() const {
    std::vector<std::string> names;
    names.reserve(factories_.size());
    for (const auto& [k, _] : factories_) names.push_back(k);
    return names;
}

} // namespace entropy
```

- [ ] Commit:

```bash
git add src/core/algorithm_registry.cpp src/core/graph_registry.cpp
git commit -m "feat: algorithm and graph registry singleton implementations"
```

---

## Task 12: IO Implementations

**Files:**
- Create: `src/io/config.cpp`
- Create: `src/io/result_exporter.cpp`

- [ ] Create `src/io/config.cpp`:

```cpp
#include "entropy/io/config.hpp"
#include <toml++/toml.hpp>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace entropy::io {

namespace {

// Read raw file content for manifest traceability.
std::string slurp(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("Cannot open config: " + path);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Populate a Parameters object from a TOML table.
void fill_params(Parameters& p, const toml::table& tbl) {
    for (auto& [k, v] : tbl) {
        std::string key{k.str()};
        if (v.is_string())         p.set(key, std::string{v.as_string()->get()});
        else if (v.is_boolean())   p.set(key, v.as_boolean()->get());
        else if (v.is_integer())   p.set(key, static_cast<std::int64_t>(v.as_integer()->get()));
        else if (v.is_floating_point()) p.set(key, v.as_floating_point()->get());
    }
}

} // namespace

ExperimentConfig parse_config(const std::string& path) {
    const std::string verbatim = slurp(path);
    auto tbl = toml::parse(verbatim);

    ExperimentConfig cfg;
    cfg.config_verbatim = verbatim;
    cfg.config_path     = path;

    cfg.name       = tbl["name"].value_or(std::string{"experiment"});
    cfg.repeats    = tbl["repeats"].value_or(5);
    cfg.warmups    = tbl["warmups"].value_or(1);
    cfg.seed       = tbl["seed"].value_or(std::uint64_t{0});
    cfg.output_dir = tbl["output_dir"].value_or(std::string{"results"});

    if (auto* graphs = tbl["graphs"].as_array()) {
        for (auto& elem : *graphs) {
            auto* gt = elem.as_table();
            if (!gt) continue;
            GraphSpec spec;
            spec.loader_name = (*gt)["loader"].value_or(std::string{""});
            if (auto* pt = (*gt)["params"].as_table())
                fill_params(spec.loader_params, *pt);
            cfg.graphs.push_back(std::move(spec));
        }
    }

    if (auto* algos = tbl["algorithms"].as_array()) {
        for (auto& elem : *algos) {
            auto* at = elem.as_table();
            if (!at) continue;
            AlgorithmSpec spec;
            spec.name = (*at)["name"].value_or(std::string{""});
            if (auto* pt = (*at)["params"].as_table())
                fill_params(spec.params, *pt);
            cfg.algorithms.push_back(std::move(spec));
        }
    }

    return cfg;
}

} // namespace entropy::io
```

- [ ] Create `src/io/result_exporter.cpp`:

```cpp
#include "entropy/io/result_exporter.hpp"
#include <fstream>
#include <stdexcept>

namespace entropy::io {

void write_run_output(const std::string&    run_dir,
                      const nlohmann::json& manifest,
                      const ResultSet&      results) {
    {
        std::ofstream f(run_dir + "/manifest.json");
        if (!f) throw std::runtime_error("Cannot write manifest.json in " + run_dir);
        f << manifest.dump(2) << "\n";
    }
    results.write_json(run_dir + "/results.json");
    results.write_csv (run_dir + "/results.csv");
}

} // namespace entropy::io
```

- [ ] Commit:

```bash
git add src/io/config.cpp src/io/result_exporter.cpp
git commit -m "feat: IO implementations — TOML config parser and result exporter"
```

---

## Task 13: ExperimentRunner Implementation

**Files:**
- Create: `src/core/experiment_runner.cpp`

- [ ] Create `src/core/experiment_runner.cpp`:

```cpp
#include "entropy/core/experiment_runner.hpp"
#include "entropy/core/algorithm_registry.hpp"
#include "entropy/core/graph_registry.hpp"
#include "entropy/core/graph_cache.hpp"
#include "entropy/core/requirements.hpp"
#include "entropy/io/result_exporter.hpp"
#include "entropy/util/version.hpp"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <nlohmann/json.hpp>
#include <xxhash.h>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <stdexcept>

namespace entropy {

namespace {

std::string utc_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H-%M-%SZ", std::gmtime(&t));
    return buf;
}

} // namespace

ExperimentRunner::ExperimentRunner(ExperimentConfig cfg) : cfg_(std::move(cfg)) {}

ResultSet ExperimentRunner::run() {
    // Create timestamped output directory.
    const std::string run_dir =
        cfg_.output_dir + "/" + cfg_.name + "/" + utc_timestamp();
    std::filesystem::create_directories(run_dir);

    // Two-sink logger: coloured stderr at INFO, file at DEBUG.
    auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file    = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
                       run_dir + "/run.log", true);
    console->set_level(spdlog::level::info);
    file->set_level(spdlog::level::debug);
    auto logger = std::make_shared<spdlog::logger>(
        "entropy", spdlog::sinks_init_list{console, file});
    spdlog::set_default_logger(logger);

    auto& algo_reg   = AlgorithmRegistry::instance();
    auto& loader_reg = GraphRegistry::instance();

    ResultSet    result_set;
    auto         graphs_meta = nlohmann::json::array();

    for (const auto& graph_spec : cfg_.graphs) {
        std::unique_ptr<IGraphLoader> loader;
        try {
            loader = loader_reg.create(graph_spec.loader_name);
        } catch (const std::exception& e) {
            spdlog::error("Loader error: {}", e.what());
            continue;
        }

        Graph graph = loader->load(graph_spec.loader_params);
        GraphCache cache(graph);

        nlohmann::json g_meta;
        g_meta["origin_label"] = graph.origin_label();
        g_meta["content_hash"] = graph.content_hash();
        g_meta["num_nodes"]    = graph.num_nodes();
        g_meta["num_edges"]    = graph.num_edges();
        g_meta["directedness"] = (graph.directedness() == GraphDirectedness::Undirected)
                                     ? "undirected" : "directed";
        g_meta["weighting"]    = (graph.weighting() == GraphWeighting::Unweighted)
                                     ? "unweighted" : "weighted";
        graphs_meta.push_back(g_meta);

        spdlog::info("Graph '{}': {} nodes, {} edges",
                     graph.origin_label(), graph.num_nodes(), graph.num_edges());

        for (const auto& algo_spec : cfg_.algorithms) {
            std::unique_ptr<IEntropyAlgorithm> algo;
            try {
                algo = algo_reg.create(algo_spec.name);
            } catch (const std::exception& e) {
                spdlog::error("Algorithm error: {}", e.what());
                continue;
            }

            // Use algorithm defaults; config params can override (Phase 3: merge).
            Parameters params = algo->default_parameters();

            const auto req = check(graph, algo->requirements());
            if (!req.ok) {
                spdlog::warn("Skip ({}, '{}'): {}",
                             algo_spec.name, graph.origin_label(), req.reason);
                RunRecord rec;
                rec.algorithm_name = algo_spec.name;
                rec.graph_label    = graph.origin_label();
                rec.skipped        = true;
                rec.skip_reason    = req.reason;
                result_set.add(rec);
                continue;
            }

            // TODO Phase 3: implement warmup + repeat loop here.
            // Phase 1: single timed run.
            const auto t0 = std::chrono::steady_clock::now();
            AlgorithmOutput output;
            try {
                output = algo->compute(graph, cache, params);
            } catch (const std::exception& e) {
                spdlog::error("Compute exception ({}, '{}'): {}",
                              algo_spec.name, graph.origin_label(), e.what());
                RunRecord rec;
                rec.algorithm_name = algo_spec.name;
                rec.graph_label    = graph.origin_label();
                rec.skipped        = true;
                rec.skip_reason    = std::string("exception: ") + e.what();
                result_set.add(rec);
                continue;
            }
            const auto   t1      = std::chrono::steady_clock::now();
            const double wall_ms =
                std::chrono::duration<double, std::milli>(t1 - t0).count();

            RunRecord rec;
            rec.algorithm_name     = std::string(algo->name());
            rec.graph_label        = graph.origin_label();
            rec.graph_content_hash = graph.content_hash();
            rec.parameters         = params.to_json();
            rec.output_metadata    = output.metadata;
            rec.value              = output.value;
            rec.value_repeats      = {output.value};
            rec.wall_ms_repeats    = {wall_ms};
            rec.wall_ms_median     = wall_ms;
            rec.wall_ms_p05        = wall_ms;
            rec.wall_ms_p95        = wall_ms;
            result_set.add(rec);

            spdlog::info("  {} -> value={:.6f}  ({:.3f} ms)",
                         algo->name(), output.value, wall_ms);
        }
    }

    // Assemble manifest.
    const std::uint64_t cfg_hash =
        XXH3_64bits(cfg_.config_verbatim.data(), cfg_.config_verbatim.size());

    nlohmann::json manifest;
    manifest["experiment_name"]  = cfg_.name;
    manifest["git_commit"]       = std::string(version::git_commit);
    manifest["git_dirty"]        = version::git_dirty;
    manifest["build_type"]       = std::string(version::build_type);
    manifest["seed"]             = cfg_.seed;
    manifest["config_path"]      = cfg_.config_path;
    manifest["config_hash_xxh3"] = cfg_hash;
    manifest["config_verbatim"]  = cfg_.config_verbatim;
    manifest["graphs"]           = graphs_meta;
    manifest["runs"]             = result_set.to_json();

    entropy::io::write_run_output(run_dir, manifest, result_set);
    spdlog::info("Output written to {}", run_dir);

    return result_set;
}

} // namespace entropy
```

- [ ] Commit:

```bash
git add src/core/experiment_runner.cpp
git commit -m "feat: experiment_runner — orchestration, timing, manifest writing"
```

---

## Task 14: Structural Entropy Algorithm Plugin

**Files:**
- Create: `src/algorithms/structural_entropy.cpp`

- [ ] Create `src/algorithms/structural_entropy.cpp`:

```cpp
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
               "H(G) = -sum_i p_i * ln(p_i), p_i = d_i / vol(G). "
               "Result in nats.";
    }

    Requirements requirements() const noexcept override {
        Requirements r;
        r.undirected = true;
        return r;
    }

    Parameters default_parameters() const override {
        return Parameters{};  // no hyperparameters
    }

    AlgorithmOutput compute(const Graph& /*g*/,
                            GraphCache&       cache,
                            const Parameters& /*params*/) const override {
        // degree_distribution() returns the normalized degree vector (sums to 1).
        // For K_n: all p_i = 1/n, H = ln(n).  For K_4: H = ln(4) ≈ 1.3863 nats.
        const auto& dist = cache.degree_distribution();

        double H = 0.0;
        for (const double p : dist)
            H -= util::xlogx(p); // xlogx returns p*ln(p), 0 when p <= 0

        AlgorithmOutput out;
        out.value = H;
        out.metadata["formula"] = "-sum(p_i * ln(p_i))";
        out.metadata["unit"]    = "nats";
        return out;
    }
};

ENTROPY_REGISTER_ALGORITHM("structural_entropy", StructuralEntropy)

} // namespace entropy
```

- [ ] Commit:

```bash
git add src/algorithms/structural_entropy.cpp
git commit -m "feat: structural_entropy plugin — degree-based Shannon entropy in nats"
```

---

## Task 15: Edgelist Loader Plugin

**Files:**
- Create: `src/loaders/edgelist_loader.cpp`

- [ ] Create `src/loaders/edgelist_loader.cpp`:

```cpp
#include "entropy/core/i_graph_loader.hpp"
#include "entropy/core/graph_registry.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace entropy {

// Reads plain-text edge lists: one "u v [weight]" per line.
// Lines beginning with '#' are treated as comments and skipped.
// Parameters:
//   path     (string) — file path
//   directed (bool, default false)
//   weighted (bool, default false) — if false, all weights set to 1.0
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
```

- [ ] Commit:

```bash
git add src/loaders/edgelist_loader.cpp
git commit -m "feat: edgelist_loader plugin — plain-text u v [weight] format"
```

---

## Task 16: CLI Application

**Files:**
- Create: `apps/cli/main.cpp`

- [ ] Create `apps/cli/main.cpp`:

```cpp
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <iostream>
#include "entropy/core/algorithm_registry.hpp"
#include "entropy/core/graph_registry.hpp"
#include "entropy/core/experiment_runner.hpp"
#include "entropy/io/config.hpp"

int main(int argc, char* argv[]) {
    CLI::App app{"entropy-cli — graph entropy research sandbox"};
    app.set_version_flag("--version", "0.1.0-skeleton");

    std::string config_path;
    bool        list_algorithms = false;
    bool        list_loaders    = false;
    int         repeats_override = -1;
    std::string output_dir_override;

    app.add_option("-c,--config",     config_path,         "Path to TOML experiment config");
    app.add_flag  ("--list-algorithms", list_algorithms,   "List registered algorithms and exit");
    app.add_flag  ("--list-loaders",    list_loaders,      "List registered loaders and exit");
    app.add_option("--repeats",       repeats_override,    "Override repeats from config");
    app.add_option("--output-dir",    output_dir_override, "Override output_dir from config");

    CLI11_PARSE(app, argc, argv);

    // Print registry counts at startup. If either is 0, the whole-archive
    // linkage is broken and no experiment will work — fail loudly.
    auto& algo_reg   = entropy::AlgorithmRegistry::instance();
    auto& loader_reg = entropy::GraphRegistry::instance();

    const auto algo_names   = algo_reg.list_names();
    const auto loader_names = loader_reg.list_names();

    spdlog::info("Loaded {} algorithm(s), {} loader(s)",
                 algo_names.size(), loader_names.size());

    if (algo_names.empty() || loader_names.empty()) {
        spdlog::error(
            "Registry is empty — likely a linker issue. "
            "Check that entropy_plugins is linked with --whole-archive in CMakeLists.txt.");
        return 1;
    }

    if (list_algorithms) {
        for (const auto& n : algo_names) std::cout << n << "\n";
        return 0;
    }

    if (list_loaders) {
        for (const auto& n : loader_names) std::cout << n << "\n";
        return 0;
    }

    if (config_path.empty()) {
        std::cerr << "Error: --config <path> is required.\n";
        std::cerr << app.help() << "\n";
        return 1;
    }

    entropy::ExperimentConfig cfg = entropy::io::parse_config(config_path);
    if (repeats_override >= 0)         cfg.repeats    = repeats_override;
    if (!output_dir_override.empty())  cfg.output_dir = output_dir_override;

    entropy::ExperimentRunner runner(std::move(cfg));
    runner.run();

    return 0;
}
```

- [ ] Commit:

```bash
git add apps/cli/main.cpp
git commit -m "feat: CLI app — config parsing, registry listing, experiment dispatch"
```

---

## Task 17: CMakeLists.txt + Configure Verification

**Files:**
- Create: `CMakeLists.txt`

- [ ] Create `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.25)
project(entropy_sandbox LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(cmake/CompilerWarnings.cmake)
include(cmake/Sanitizers.cmake)
include(FetchContent)

# ---------------------------------------------------------------------------
# Git version baked in at configure time
# ---------------------------------------------------------------------------
execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_SHA
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_SHA)
    set(GIT_SHA "unknown")
endif()

execute_process(
    COMMAND git diff --quiet
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE GIT_DIRTY_CODE
    ERROR_QUIET
)
if(GIT_DIRTY_CODE EQUAL 0)
    set(GIT_DIRTY_BOOL "false")
else()
    set(GIT_DIRTY_BOOL "true")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
endif()

configure_file(
    include/entropy/util/version.hpp.in
    ${CMAKE_BINARY_DIR}/generated/entropy/util/version.hpp
    @ONLY
)

# ---------------------------------------------------------------------------
# Dependencies via FetchContent
# ---------------------------------------------------------------------------
set(FETCHCONTENT_QUIET OFF)

# Eigen — header only, don't run its CMakeLists (avoids building its tests)
FetchContent_Declare(Eigen3
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG        3.4.0
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(Eigen3)
if(NOT eigen3_POPULATED)
    FetchContent_Populate(Eigen3)
endif()
add_library(Eigen3_iface INTERFACE)
target_include_directories(Eigen3_iface INTERFACE ${eigen3_SOURCE_DIR})
add_library(Eigen3::Eigen ALIAS Eigen3_iface)

# nlohmann/json
set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install    OFF CACHE INTERNAL "")
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# toml++
FetchContent_Declare(tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(tomlplusplus)

# CLI11
FetchContent_Declare(CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.4.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(CLI11)

# spdlog
set(SPDLOG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(spdlog)

# xxhash — compiled as a tiny static library from a single .c file
FetchContent_Declare(xxhash
    GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
    GIT_TAG        v0.8.2
    GIT_SHALLOW    TRUE
)
FetchContent_GetProperties(xxhash)
if(NOT xxhash_POPULATED)
    FetchContent_Populate(xxhash)
endif()
add_library(xxhash_lib STATIC ${xxhash_SOURCE_DIR}/xxhash.c)
target_include_directories(xxhash_lib PUBLIC ${xxhash_SOURCE_DIR})

# ---------------------------------------------------------------------------
# Core library (everything that is NOT an algorithm or loader plugin)
# ---------------------------------------------------------------------------
add_library(entropy_core STATIC
    src/core/graph.cpp
    src/core/graph_cache.cpp
    src/core/algorithm_registry.cpp
    src/core/graph_registry.cpp
    src/core/experiment_runner.cpp
    src/core/parameters.cpp
    src/core/requirements.cpp
    src/core/result.cpp
    src/io/config.cpp
    src/io/result_exporter.cpp
)
target_include_directories(entropy_core PUBLIC
    include
    ${CMAKE_BINARY_DIR}/generated
)
target_link_libraries(entropy_core PUBLIC
    Eigen3::Eigen
    nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus
    spdlog::spdlog
    xxhash_lib
)
apply_compiler_warnings(entropy_core)
apply_sanitizers(entropy_core)

# ---------------------------------------------------------------------------
# Plugins — OBJECT library
#
# IMPORTANT: These are compiled into an OBJECT library (not a static archive).
# Each .cpp registers itself into AlgorithmRegistry / GraphRegistry via a
# static initialiser macro (ENTROPY_REGISTER_ALGORITHM / ENTROPY_REGISTER_LOADER).
#
# Without the $<TARGET_OBJECTS:entropy_plugins> trick below, the linker would
# silently discard every plugin TU that has no direct symbol reference from
# main.cpp, leaving the registry empty at runtime.
#
# To add a new algorithm: add its .cpp here. That is the only existing-file
# change required.
# ---------------------------------------------------------------------------
add_library(entropy_plugins OBJECT
    src/algorithms/structural_entropy.cpp
    src/loaders/edgelist_loader.cpp
)
target_link_libraries(entropy_plugins PUBLIC entropy_core)
apply_compiler_warnings(entropy_plugins)
apply_sanitizers(entropy_plugins)

# ---------------------------------------------------------------------------
# CLI executable
# ---------------------------------------------------------------------------
add_executable(entropy-cli apps/cli/main.cpp)
target_link_libraries(entropy-cli PRIVATE
    entropy_core
    CLI11::CLI11
)

# Force all plugin object files into the final binary so static initialisers run.
if(MSVC)
    target_link_options(entropy-cli PRIVATE
        /WHOLEARCHIVE:$<TARGET_FILE:entropy_plugins>)
else()
    target_link_libraries(entropy-cli PRIVATE
        -Wl,--whole-archive
        $<TARGET_OBJECTS:entropy_plugins>
        -Wl,--no-whole-archive
    )
endif()

apply_compiler_warnings(entropy-cli)
apply_sanitizers(entropy-cli)
```

- [ ] Configure the build and verify it succeeds:

```bash
cd /home/tobi/SimEnv/graph-entropy-sandbox
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

Expected: configuration completes with no errors. FetchContent will download dependencies on first run (takes a minute). No "Cannot find source file" errors.

- [ ] Commit:

```bash
git add CMakeLists.txt
git commit -m "build: CMakeLists.txt with FetchContent deps and whole-archive plugin linkage"
```

---

## Task 18: Sample Data, Smoke Config, Full Build, and Smoke Run

**Files:**
- Create: `data/samples/k4.txt`
- Create: `configs/smoke.toml`

- [ ] Create `data/samples/k4.txt`
(K₄ complete graph: 4 nodes, 6 undirected edges — each node has degree 3,
expected structural entropy = ln(4) ≈ 1.386294 nats):

```
# K4 complete graph — 4 nodes, 6 undirected edges
# Node IDs: 0, 1, 2, 3
0 1
0 2
0 3
1 2
1 3
2 3
```

- [ ] Create `configs/smoke.toml`:

```toml
name       = "smoke"
repeats    = 1
warmups    = 0
seed       = 42
output_dir = "results"

[[graphs]]
loader = "edgelist"
[graphs.params]
path     = "data/samples/k4.txt"
directed = false

[[algorithms]]
name = "structural_entropy"
```

- [ ] Build the full project:

```bash
cmake --build build --parallel
```

Expected: compiles `entropy-cli` with no errors or warnings.

- [ ] Verify algorithm registration (tests the --whole-archive linkage):

```bash
./build/entropy-cli --list-algorithms
```

Expected output:
```
structural_entropy
```

If the output is empty or the binary exits with code 1, the whole-archive linkage is broken. Check that `$<TARGET_OBJECTS:entropy_plugins>` is present in the `target_link_libraries` call for `entropy-cli` in CMakeLists.txt.

- [ ] Verify loader registration:

```bash
./build/entropy-cli --list-loaders
```

Expected output:
```
edgelist
```

- [ ] Run the smoke experiment:

```bash
./build/entropy-cli --config configs/smoke.toml
```

Expected console output (timestamps and hash will differ):
```
[info] Loaded 1 algorithm(s), 1 loader(s)
[info] Graph 'data/samples/k4.txt': 4 nodes, 6 edges
[info]   structural_entropy -> value=1.386294  (x.xxx ms)
[info] Output written to results/smoke/2026-...Z
```

- [ ] Verify output files exist:

```bash
ls results/smoke/*/
```

Expected: `manifest.json  results.csv  results.json  run.log`

- [ ] Spot-check the entropy value in results.json:

```bash
python3 -c "
import json, math
data = json.load(open(sorted(__import__('glob').glob('results/smoke/*/results.json'))[0]))
val = data[0]['value']
expected = math.log(4)
print(f'value={val:.6f}  expected={expected:.6f}  diff={abs(val-expected):.2e}')
assert abs(val - expected) < 1e-9, 'Value mismatch!'
print('OK')
"
```

Expected output:
```
value=1.386294  expected=1.386294  diff=0.00e+00
OK
```

- [ ] Commit:

```bash
git add data/samples/k4.txt configs/smoke.toml
git commit -m "feat: smoke config and K4 sample graph — skeleton complete"
```

---

## Self-Review Checklist

- [x] **Spec §4.1 Graph/GraphBuilder** — implemented in Tasks 3 + 8 (CSR, immutable, content_hash via xxh3)
- [x] **Spec §4.2 GraphCache** — implemented in Tasks 5 + 9 (degrees + distribution live; spectral stubbed)
- [x] **Spec §4.3 Parameters** — Tasks 4 + 10 (typed variant map, template get/get_or, to_json)
- [x] **Spec §4.4 Requirements** — Tasks 4 + 10 (undirected + weight + max_nodes checked; simple/connected TODO Phase 3)
- [x] **Spec §4.5 IEntropyAlgorithm** — Task 5 (5 pure virtual methods per spec)
- [x] **Spec §4.6 AlgorithmOutput + ResultSet** — Tasks 4 + 10 (write_json, write_csv long-format)
- [x] **Spec §4.7 Self-registration** — Tasks 6 + 14 + 15 + 17 (macro + OBJECT + whole-archive)
- [x] **Spec §4.8 ExperimentRunner** — Task 13 (single repeat Phase 1, TODO Phase 3 comment for loop)
- [x] **Spec §5 Reproducibility** — Task 13 (git SHA from version.hpp, config_verbatim, config_hash_xxh3, graph content_hash)
- [x] **Spec §6 Numerics** — Task 2 (xlogx, safe_log, nats); structural entropy uses natural log
- [x] **Spec §7 TOML config** — Task 12 (name, repeats, warmups, seed, output_dir, graphs, algorithms)
- [x] **Spec §14 CMake** — Task 17 (FetchContent, OBJECT library, whole-archive comment)
- [x] **Spec §15 Result layout** — Task 13 (manifest.json + results.json + results.csv + run.log)
- [x] **Spec §18 Pitfall 1** — CLI prints count at startup and exits 1 if empty (Task 16)
- [x] **structural_entropy.md** — Task 14 (natural log, nats, degree_distribution from cache)
