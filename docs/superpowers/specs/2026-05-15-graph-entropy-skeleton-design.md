# Graph Entropy Sandbox — Skeleton Design

**Date:** 2026-05-15
**Status:** Approved
**Scope:** Phase 1 skeleton with structural entropy as the sole algorithm

---

## Purpose

Implement the project skeleton described in `graph_entropy_sandbox_spec.md`, with exactly one algorithm (`structural_entropy`) and one loader (`edgelist`). The goal is to establish the full directory structure, all core abstractions, and the self-registration plugin pattern — so future algorithms and loaders can be added by dropping a single `.cpp` file with zero changes to existing code.

---

## Architecture

```
apps/cli/main.cpp
  └─ parses TOML → ExperimentConfig → ExperimentRunner

ExperimentRunner
  └─ for each (graph, algorithm) pair:
       1. GraphRegistry  → IGraphLoader  → Graph (CSR)
       2. AlgorithmRegistry → IEntropyAlgorithm
       3. Requirements check → skip with reason if fails
       4. GraphCache around Graph
       5. algorithm.compute(graph, cache, params) → AlgorithmOutput
       6. RunRecord → ResultSet → manifest.json + results.json
```

Three static libraries:

| Target | Type | Contents |
|---|---|---|
| `entropy_core` | STATIC | Graph, GraphCache, registries, runner, IO |
| `entropy_plugins` | OBJECT | `structural_entropy.cpp`, `edgelist_loader.cpp` |
| `entropy-cli` | EXE | `apps/cli/main.cpp`, links both via `--whole-archive` |

The OBJECT + `--whole-archive` pattern is essential: it forces the linker to include all plugin translation units so their static initializers (which register factories) actually execute at program startup.

---

## File Structure

Files marked `*` receive full implementations. Everything else is a declared header or empty placeholder directory.

```
graph-entropy-sandbox/
├── CMakeLists.txt *
├── cmake/
│   ├── CompilerWarnings.cmake *
│   └── Sanitizers.cmake *
├── include/entropy/
│   ├── core/
│   │   ├── graph.hpp *
│   │   ├── graph_cache.hpp *
│   │   ├── i_entropy_algorithm.hpp *
│   │   ├── i_graph_loader.hpp *
│   │   ├── algorithm_registry.hpp *
│   │   ├── graph_registry.hpp *
│   │   ├── parameters.hpp *
│   │   ├── requirements.hpp *
│   │   ├── result.hpp *
│   │   └── experiment_runner.hpp *
│   ├── io/
│   │   ├── config.hpp *
│   │   └── result_exporter.hpp *
│   └── util/
│       ├── numerics.hpp *
│       ├── rng.hpp *
│       └── version.hpp.in
├── src/
│   ├── core/
│   │   ├── graph.cpp *
│   │   ├── graph_cache.cpp *
│   │   ├── algorithm_registry.cpp *
│   │   ├── graph_registry.cpp *
│   │   └── experiment_runner.cpp *
│   ├── io/
│   │   ├── config.cpp *
│   │   └── result_exporter.cpp *
│   ├── algorithms/
│   │   └── structural_entropy.cpp *
│   └── loaders/
│       └── edgelist_loader.cpp *
├── apps/cli/main.cpp *
├── configs/smoke.toml *
├── data/samples/k4.txt *         ← K₄ complete graph, 6 edges
├── data/samples/.gitkeep
├── results/.gitignore
├── docs/
├── notebooks/
├── tests/
│   ├── unit/
│   ├── property/
│   └── golden/
├── bindings/python/
└── .gitignore
```

---

## Core Abstractions

All interfaces are implemented exactly as specified in `graph_entropy_sandbox_spec.md` §4.

### Graph / GraphBuilder
- CSR (Compressed Sparse Row) storage. Immutable after `GraphBuilder::build()`.
- Undirected graphs store each edge as two directed entries.
- `content_hash()` uses xxhash (xxh3) over the canonical sorted edge list.
- `neighbors(v)` and `edge_weights(v)` return `std::span` — zero allocation.

### GraphCache
- Constructed around a `const Graph&`. Lazy init with `std::call_once` per field.
- **Implemented:** `degrees()`, `degree_distribution()` (normalized, sums to 1).
- **Stubbed** (throws `std::runtime_error("not yet implemented")`): `adjacency_matrix()`, `laplacian()`, `normalized_laplacian()`, `normalized_laplacian_spectrum()`, `random_walk_stationary()`. These are filled in when spectral algorithms are added.

### AlgorithmRegistry / GraphRegistry
- Singleton, populated at startup by static initializers in plugin `.cpp` files.
- `ENTROPY_REGISTER_ALGORITHM(key, Class)` macro in each algorithm file.
- `ENTROPY_REGISTER_LOADER(key, Class)` macro in each loader file.
- CLI prints `Loaded N algorithms, M loaders` at startup; exits non-zero if either is 0.

### Parameters
- Typed key-value store (`variant<bool, int64_t, double, string>`).
- `get<T>(key)` throws if missing or wrong type.
- `get_or<T>(key, fallback)` is noexcept.
- `to_json()` used for manifest serialization.

### Requirements
- Struct declaring what a graph must satisfy (undirected, simple, connected, etc.).
- `check(graph, requirements)` returns `RequirementCheck { ok, reason }`.
- Runner records a `skipped` RunRecord with the reason on failure — never silently proceeds.

### IEntropyAlgorithm
Five pure virtual methods: `name()`, `description()`, `requirements()`, `default_parameters()`, `compute()`.

### ExperimentRunner — Phase 1 scope
- Single repeat, no warm-up.
- One timed run via `std::chrono::steady_clock`; populates `wall_ms_repeats` with one entry.
- `wall_ms_median`, `wall_ms_p05`, `wall_ms_p95` all equal that single value in Phase 1.
- Phase 1 runs exactly one repeat regardless of the `repeats` config value. A `// TODO Phase 3: implement full warmup+repeat loop` comment marks the expansion point.

---

## Algorithm: Structural Entropy

**Formula:**
```
H(G) = -Σᵢ pᵢ · ln(pᵢ)    where pᵢ = dᵢ / vol(G),  vol(G) = Σ dᵢ
```

- Returns **nats** (natural log), consistent with the project-wide convention. `structural_entropy.md` describes log₂; we use `std::log` instead.
- Uses `cache.degree_distribution()` — a normalized vector already computed by GraphCache.
- Delegates to `util::xlogx(p)` which handles the `0·ln(0) = 0` convention.
- Requirements: `undirected = true`.
- No hyperparameters (empty `default_parameters()`).

```cpp
// sketch of compute():
const auto& p = cache.degree_distribution();
double H = 0.0;
for (double pi : p) H -= util::xlogx(pi);   // xlogx returns p*ln(p), 0 if p<=0
return AlgorithmOutput{ .value = H };
```

---

## Loader: Edgelist

- Reads a plain-text file: one `u v [weight]` edge per line. Lines starting with `#` are comments.
- Parameters: `path` (string), `directed` (bool, default false).
- Uses `GraphBuilder` to construct the Graph.

---

## Numerical Conventions (`util/numerics.hpp`)

```cpp
// Returns p * ln(p), with 0*ln(0) := 0
double xlogx(double p) noexcept;

// Returns ln(p), asserting p > 0. Caller responsible for p==0 guard.
double safe_log(double p) noexcept;
```

All entropy values in nats. Conversion to bits = value / ln(2); documented in a header comment, not enforced.

---

## RNG (`util/rng.hpp`)

- Wraps `std::mt19937_64` seeded via splitmix64 expansion of a `uint64_t` seed.
- Per-(graph, algorithm, repeat) derived seed: `splitmix64(base ^ hash(graph_name) ^ hash(algo_name) ^ repeat_index)`.
- No calls to global RNGs or `std::rand` anywhere in the codebase.

---

## Output

```
results/smoke/2026-05-15T14-32-01Z/
├── manifest.json    ← git SHA, config verbatim, graph hashes, all run records
├── results.json     ← full ResultSet
├── results.csv      ← long-format: one row per (graph, algo, repeat)
└── run.log          ← spdlog debug sink
```

**manifest.json** fields: `experiment_name`, `git_commit`, `git_dirty`, `build_type`, `compiler`, `config_verbatim`, `config_sha256`, per-graph identity (origin_label, content_hash, num_nodes, num_edges), and all RunRecords.

---

## Config Format (`configs/smoke.toml`)

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

---

## Dependencies (via FetchContent)

| Library | Version | Use |
|---|---|---|
| Eigen | 3.4 | Linear algebra (GraphCache spectral, future) |
| nlohmann/json | 3.11+ | JSON serialization |
| toml++ | 3.x | TOML config parsing |
| CLI11 | 2.x | CLI argument parsing |
| spdlog | 1.x | Structured logging |
| xxhash | latest | Graph content hashing |
| Catch2 | v3 | Unit testing — not fetched in this phase; add when tests are introduced |

---

## Exit Criteria for This Skeleton

- `cmake -B build && cmake --build build` succeeds with no warnings.
- `./build/entropy-cli --config configs/smoke.toml` runs and produces `results/smoke/<timestamp>/manifest.json`, `results.json`, `results.csv`, `run.log`.
- `./build/entropy-cli --list-algorithms` prints `structural_entropy`.
- `./build/entropy-cli --list-loaders` prints `edgelist`.
- Adding a second algorithm = creating one `.cpp` file in `src/algorithms/` and one line in `CMakeLists.txt`'s OBJECT library source list (the only allowable existing-file edit).
