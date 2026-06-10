# Pre-Merge SI — Plan 01: Foundation (C++ kernel + pybind11 + Python harness)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the verified numeric foundation for the pre-merge structural-information experiments: a C++ `entropy::premerge` kernel (bits), a pybind11 binding, and a Python harness layer (generators, estimators, results), cross-validated to 1e-9 against the verified Appendix-B reference code.

**Architecture:** A new C++ module `src/premerge/` (namespace `entropy::premerge`) operates on **global-label edge lists** — `E` = `vector<pair<int,int>>`, `V` = `vector<int>`, partition = `map<int,int>` — mirroring the Python reference in `pre_merge_SI_theory.md` Appendix B exactly, so the two implementations compare directly. It is **not** an `IEntropyAlgorithm` plugin (those are single-graph scalar). A pybind11 module `premerge_kernel` exposes free functions; no C++ `Graph` crosses the boundary. A Python package `experiments/premerge_py/` builds graphs (NetworkX), composes estimators, and writes reproducible results.

**Tech Stack:** C++20, CMake 3.25 + FetchContent, Catch2 v3 (tests), pybind11 2.12, Python 3.10+, NetworkX, NumPy, SciPy, pytest. Entropy in **bits (log₂)** everywhere.

**Reference contract:** All formulas and verified fixtures come from `pre_merge_SI_theory.md` (Appendix A/B) and `preliminary_exp_spec.md`. Where a fixture value is quoted (e.g. `H^P=1.2924812`), it is from those documents and is in bits.

---

## File Structure

```
graph-entropy-sandbox/
  CMakeLists.txt                         # MODIFY: Catch2, pybind11, premerge lib, tests, module
  cmake/Catch2.cmake                     # NEW (optional helper; or inline in CMakeLists)
  include/entropy/util/numerics.hpp      # MODIFY: add xlog2x, safe_log2
  include/entropy/premerge/
    types.hpp                            # NEW: Edge, EdgeList, NodeSet, Partition, Message aliases
    h1.hpp                               # NEW
    partition_entropy.hpp                # NEW
    merge.hpp                            # NEW
    h2_min.hpp                           # NEW
    message.hpp                          # NEW
    bias.hpp                             # NEW
  src/premerge/
    h1.cpp partition_entropy.cpp merge.cpp h2_min.cpp message.cpp bias.cpp   # NEW
  src/algorithms/structural_entropy.cpp  # MODIFY: nats -> bits
  bindings/python/premerge_module.cpp    # NEW: pybind11
  tests/premerge/                        # NEW: Catch2 tests, one file per unit
    test_h1.cpp test_partition_entropy.cpp test_merge.cpp
    test_h2_min.cpp test_message.cpp test_bias.cpp test_witnesses.cpp
  docs/superpowers/specs/graph_entropy_sandbox_spec.md   # MODIFY §6: nats -> bits

experiments/                             # NEW Python package at repo ROOT
  pyproject.toml  requirements.txt
  premerge_py/
    __init__.py kernel.py reference.py generators.py estimators.py
    results.py summary.py
  tests/
    test_kernel_fixtures.py test_cross_validation.py test_generators.py
    test_estimators.py test_results.py
  configs/.gitkeep
```

---

## Phase A — C++ premerge kernel (bits)

### Task 1: Wire Catch2 test framework + premerge library targets

**Files:**
- Modify: `graph-entropy-sandbox/CMakeLists.txt`
- Create: `graph-entropy-sandbox/tests/premerge/test_smoke.cpp`

- [ ] **Step 1: Add Catch2 + premerge lib + test target to CMake.** Append to `CMakeLists.txt` after the CLI section (around line 192):

```cmake
# ---------------------------------------------------------------------------
# Catch2 v3 (tests)
# ---------------------------------------------------------------------------
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.6.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

# ---------------------------------------------------------------------------
# premerge kernel (label-keyed; NOT an IEntropyAlgorithm plugin)
# ---------------------------------------------------------------------------
add_library(entropy_premerge STATIC
    src/premerge/h1.cpp
    src/premerge/partition_entropy.cpp
    src/premerge/merge.cpp
    src/premerge/h2_min.cpp
    src/premerge/message.cpp
    src/premerge/bias.cpp
)
target_include_directories(entropy_premerge PUBLIC include ${CMAKE_BINARY_DIR}/generated)
apply_compiler_warnings(entropy_premerge)
apply_sanitizers(entropy_premerge)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
include(Catch)
add_executable(premerge_tests
    tests/premerge/test_smoke.cpp
)
target_link_libraries(premerge_tests PRIVATE entropy_premerge Catch2::Catch2WithMain)
apply_compiler_warnings(premerge_tests)
catch_discover_tests(premerge_tests)
```

Create the empty source files referenced by `entropy_premerge` as one-line stubs so CMake configures (each will be filled by later tasks):

```bash
mkdir -p graph-entropy-sandbox/src/premerge graph-entropy-sandbox/include/entropy/premerge graph-entropy-sandbox/tests/premerge
for f in h1 partition_entropy merge h2_min message bias; do
  printf 'namespace entropy::premerge {}\n' > graph-entropy-sandbox/src/premerge/$f.cpp
done
```

- [ ] **Step 2: Write a trivial smoke test.** `tests/premerge/test_smoke.cpp`:

```cpp
#include <catch2/catch_test_macros.hpp>
TEST_CASE("catch2 is wired", "[smoke]") { REQUIRE(1 + 1 == 2); }
```

- [ ] **Step 3: Configure + build + run.**

Run:
```bash
cmake -S graph-entropy-sandbox -B graph-entropy-sandbox/build -DCMAKE_BUILD_TYPE=Debug
cmake --build graph-entropy-sandbox/build -j --target premerge_tests
ctest --test-dir graph-entropy-sandbox/build --output-on-failure
```
Expected: `100% tests passed`.

- [ ] **Step 4: Commit.**

```bash
git add graph-entropy-sandbox/CMakeLists.txt graph-entropy-sandbox/src/premerge graph-entropy-sandbox/tests/premerge
git commit -m "Wire Catch2 and premerge kernel targets"
```

---

### Task 2: Bits numerics + convert structural_entropy to bits

**Files:**
- Modify: `graph-entropy-sandbox/include/entropy/util/numerics.hpp`
- Modify: `graph-entropy-sandbox/src/algorithms/structural_entropy.cpp`
- Modify: `graph-entropy-sandbox/tests/premerge/test_smoke.cpp` (temporary numerics check)
- Modify: `docs/superpowers/specs/graph_entropy_sandbox_spec.md` (§6)

- [ ] **Step 1: Write failing test for bit helpers.** Append to `tests/premerge/test_smoke.cpp`:

```cpp
#include "entropy/util/numerics.hpp"
#include <catch2/catch_approx.hpp>
TEST_CASE("bit-based entropy helpers", "[numerics]") {
    // xlog2x(0.5) = 0.5*log2(0.5) = -0.5
    REQUIRE(entropy::util::xlog2x(0.5) == Catch::Approx(-0.5));
    REQUIRE(entropy::util::xlog2x(0.0) == Catch::Approx(0.0));
    REQUIRE(entropy::util::safe_log2(8.0) == Catch::Approx(3.0));
}
```

- [ ] **Step 2: Run, verify it fails to compile** (`xlog2x` undefined).
Run: `cmake --build graph-entropy-sandbox/build -j --target premerge_tests`
Expected: compile error `no member named 'xlog2x'`.

- [ ] **Step 3: Add bit helpers.** In `numerics.hpp`, inside `namespace entropy::util`, add:

```cpp
// Returns p * log2(p), with the convention 0 * log2(0) := 0. Precondition: p >= 0.
inline double xlog2x(double p) noexcept {
    if (p <= 0.0) return 0.0;
    return p * std::log2(p);
}
// Returns log2(p). Returns 0.0 when p <= 0 (0*log2(0)=0 convention at call site).
inline double safe_log2(double p) noexcept {
    if (p <= 0.0) return 0.0;
    return std::log2(p);
}
```

- [ ] **Step 4: Convert structural_entropy.cpp to bits.** Replace `util::xlogx(p)` with `util::xlog2x(p)` (line ~37); change the description "Result in nats." → "Result in bits."; `out.metadata["unit"] = "nats";` → `= "bits";`; `out.metadata["formula"] = "-sum(p_i * log2(p_i))";`.

- [ ] **Step 5: Update sandbox spec §6.** In `docs/superpowers/specs/graph_entropy_sandbox_spec.md` change "All entropies are returned **in nats** (natural log). If a user wants bits, they can divide by `ln(2)`" to "All entropies are returned **in bits** (log₂). This is the project-wide convention." Keep the `xlogx`/`safe_log` mention but add that `xlog2x`/`safe_log2` are the bit-based forms used by entropy code.

- [ ] **Step 6: Build + run tests.**
Run: `cmake --build graph-entropy-sandbox/build -j --target premerge_tests && ctest --test-dir graph-entropy-sandbox/build --output-on-failure`
Expected: PASS.

- [ ] **Step 7: Commit.**

```bash
git add graph-entropy-sandbox/include/entropy/util/numerics.hpp graph-entropy-sandbox/src/algorithms/structural_entropy.cpp docs/superpowers/specs/graph_entropy_sandbox_spec.md graph-entropy-sandbox/tests/premerge/test_smoke.cpp
git commit -m "Add bit-based entropy helpers; convert structural_entropy and spec to bits"
```

---

### Task 3: Kernel types + H¹

**Files:**
- Create: `include/entropy/premerge/types.hpp`, `include/entropy/premerge/h1.hpp`, `src/premerge/h1.cpp`
- Create: `tests/premerge/test_h1.cpp`
- Modify: `CMakeLists.txt` (add `test_h1.cpp` to `premerge_tests`)

- [ ] **Step 1: Define kernel types.** `include/entropy/premerge/types.hpp`:

```cpp
#pragma once
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace entropy::premerge {
using Node     = std::int64_t;                 // GLOBAL integer label
using EdgePair = std::pair<Node, Node>;         // unordered; stored canonical (min,max)
using EdgeList = std::vector<EdgePair>;
using NodeSet  = std::vector<Node>;
using Partition = std::map<Node, Node>;         // node -> module id

inline EdgePair canon(Node u, Node v) { return (u <= v) ? EdgePair{u, v} : EdgePair{v, u}; }
} // namespace entropy::premerge
```

- [ ] **Step 2: Write failing test.** `tests/premerge/test_h1.cpp`:

```cpp
#include "entropy/premerge/h1.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
using namespace entropy::premerge;

TEST_CASE("H1 of 3-node path = 1.5 bits", "[h1]") {
    EdgeList E = {{0,1},{1,2}};
    NodeSet  V = {0,1,2};
    auto [h1, W] = h1_value(E, V);
    REQUIRE(W == 4);                 // degrees 1,2,1 -> vol 4
    REQUIRE(h1 == Catch::Approx(1.5).epsilon(1e-12));
}
```

- [ ] **Step 3: Header + impl.** `include/entropy/premerge/h1.hpp`:

```cpp
#pragma once
#include "entropy/premerge/types.hpp"
#include <map>
#include <utility>
namespace entropy::premerge {
// Degrees keyed by global label. Self-loops counted twice (as in vol).
std::map<Node,long> degrees(const EdgeList& E, const NodeSet& V);
// Returns (H1 in bits, volume W = sum of degrees).
std::pair<double,long> h1_value(const EdgeList& E, const NodeSet& V);
} // namespace entropy::premerge
```

`src/premerge/h1.cpp`:

```cpp
#include "entropy/premerge/h1.hpp"
#include "entropy/util/numerics.hpp"
namespace entropy::premerge {
std::map<Node,long> degrees(const EdgeList& E, const NodeSet& V) {
    std::map<Node,long> d;
    for (Node v : V) d[v] = 0;
    for (auto [u,v] : E) { d[u] += 1; d[v] += 1; }
    return d;
}
std::pair<double,long> h1_value(const EdgeList& E, const NodeSet& V) {
    auto d = degrees(E, V);
    long W = 0; for (auto& [n,dn] : d) W += dn;
    double H = 0.0;
    if (W > 0) for (auto& [n,dn] : d) H -= util::xlog2x(static_cast<double>(dn)/W);
    return {H, W};
}
} // namespace entropy::premerge
```

- [ ] **Step 4: Add test file to CMake** (`premerge_tests` sources) and build + run.
Run: `cmake --build graph-entropy-sandbox/build -j --target premerge_tests && ctest --test-dir graph-entropy-sandbox/build --output-on-failure`
Expected: PASS.

- [ ] **Step 5: Commit.** `git add -A graph-entropy-sandbox && git commit -m "premerge: H1 (bits), label-keyed degrees"`

---

### Task 4: Partition entropy H^P + decomposition identity

**Files:**
- Create: `include/entropy/premerge/partition_entropy.hpp`, `src/premerge/partition_entropy.cpp`, `tests/premerge/test_partition_entropy.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Failing test (3-node path fixture from spec §2.3 / notes B.2).** `tests/premerge/test_partition_entropy.cpp`:

```cpp
#include "entropy/premerge/partition_entropy.hpp"
#include "entropy/premerge/h1.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
using namespace entropy::premerge;

TEST_CASE("H^P 3-node path {0,1}|{2} = 1.2924812 bits", "[hp]") {
    EdgeList E = {{0,1},{1,2}}; NodeSet V = {0,1,2};
    Partition p = {{0,0},{1,0},{2,1}};
    REQUIRE(h_partition(E, V, p) == Catch::Approx(1.2924812).epsilon(1e-6));
}
TEST_CASE("decomposition identity H^P = H1 - H(q) + S", "[hp][identity]") {
    EdgeList E = {{0,1},{1,2}}; NodeSet V = {0,1,2};
    Partition p = {{0,0},{1,0},{2,1}};
    auto d = decompose(E, V, p);
    auto [h1, W] = h1_value(E, V);
    REQUIRE(d.Hq  == Catch::Approx(0.81127812));
    REQUIRE(d.S   == Catch::Approx(0.60375937));   // = -(1/4)log2(3/4) - (1/4)log2(1/4); notes §B.2 "0.60381" is a rounding typo
    REQUIRE(h1 - d.Hq + d.S == Catch::Approx(d.HP).epsilon(1e-12));
    REQUIRE(d.HP  == Catch::Approx(1.2924812).epsilon(1e-6));
}
```

- [ ] **Step 2: Header.** `include/entropy/premerge/partition_entropy.hpp`:

```cpp
#pragma once
#include "entropy/premerge/types.hpp"
namespace entropy::premerge {
struct Decomposition { double HP, H1, Hq, S, benefit; long W; };
double h_partition(const EdgeList& E, const NodeSet& V, const Partition& part);
Decomposition decompose(const EdgeList& E, const NodeSet& V, const Partition& part);
} // namespace entropy::premerge
```

- [ ] **Step 3: Impl** (Term I / Term II per spec §1.2; identity per notes A.3). `src/premerge/partition_entropy.cpp`:

```cpp
#include "entropy/premerge/partition_entropy.hpp"
#include "entropy/premerge/h1.hpp"
#include "entropy/util/numerics.hpp"
#include <map>
namespace entropy::premerge {

static void module_stats(const EdgeList& E, const NodeSet& V, const Partition& part,
                         std::map<Node,long>& deg, std::map<Node,long>& Vj,
                         std::map<Node,long>& gj, std::map<Node,long>& eint, long& W) {
    deg = degrees(E, V);
    W = 0; for (auto& [n,dn] : deg) W += dn;
    for (auto& [n,dn] : deg) Vj[part.at(n)] += dn;
    for (auto [u,v] : E) {
        Node pu = part.at(u), pv = part.at(v);
        if (pu == pv) eint[pu] += 1;
        else { gj[pu] += 1; gj[pv] += 1; }
    }
}

double h_partition(const EdgeList& E, const NodeSet& V, const Partition& part) {
    std::map<Node,long> deg, Vj, gj, eint; long W;
    module_stats(E, V, part, deg, Vj, gj, eint, W);
    double term1 = 0.0; // -sum_v (d_v/W) log2(d_v/Vj)
    for (auto& [n,dn] : deg) {
        Node j = part.at(n);
        if (dn > 0 && Vj[j] > 0)
            term1 -= (static_cast<double>(dn)/W) * util::safe_log2(static_cast<double>(dn)/Vj[j]);
    }
    double term2 = 0.0; // -sum_j (g_j/W) log2(Vj/W)
    for (auto& [j,vj] : Vj)
        if (vj > 0) term2 -= (static_cast<double>(gj[j])/W) * util::safe_log2(static_cast<double>(vj)/W);
    return term1 + term2;
}

Decomposition decompose(const EdgeList& E, const NodeSet& V, const Partition& part) {
    std::map<Node,long> deg, Vj, gj, eint; long W;
    module_stats(E, V, part, deg, Vj, gj, eint, W);
    auto [H1, W2] = h1_value(E, V);
    double Hq = 0.0, S = 0.0;
    for (auto& [j,vj] : Vj) if (vj > 0) {
        double q = static_cast<double>(vj)/W;
        Hq -= util::xlog2x(q);
        S  -= (static_cast<double>(gj[j])/W) * util::safe_log2(q);
    }
    Decomposition d; d.W = W; d.H1 = H1; d.Hq = Hq; d.S = S;
    d.HP = H1 - Hq + S; d.benefit = Hq - S;
    return d;
}
} // namespace entropy::premerge
```

- [ ] **Step 4: Add to CMake, build, run.** Expected: PASS.
- [ ] **Step 5: Commit.** `git commit -am "premerge: H^P and decomposition identity (verified on 3-node path)"`

---

### Task 5: Merge (simple union, dedup) + overlaps + merged degrees

**Files:**
- Create: `include/entropy/premerge/merge.hpp`, `src/premerge/merge.cpp`, `tests/premerge/test_merge.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Failing test** (volume-sum invariant + overlap). `tests/premerge/test_merge.cpp`:

```cpp
#include "entropy/premerge/merge.hpp"
#include "entropy/premerge/h1.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace entropy::premerge;

TEST_CASE("simple union dedups shared edges; W = 2|E_A ∪ E_B|", "[merge]") {
    EdgeList EA = {{0,1},{1,2}}; NodeSet VA = {0,1,2};
    EdgeList EB = {{1,2},{2,3}}; NodeSet VB = {1,2,3};   // edge {1,2} shared
    auto M = merge(EA, VA, EB, VB);
    REQUIRE(M.EM.size() == 3);                            // {0,1},{1,2},{2,3}
    long W = 0; for (auto& [n,dn] : degrees(M.EM, M.VM)) W += dn;
    REQUIRE(W == 2 * static_cast<long>(M.EM.size()));     // volume-sum invariant
}
TEST_CASE("overlaps o_i counts edges present in BOTH among anchors", "[merge]") {
    EdgeList EA = {{0,1},{1,2}}; EdgeList EB = {{1,2},{2,3}};
    NodeSet S = {1,2};
    auto o = overlaps(EA, EB, S);
    REQUIRE(o[1] == 1); REQUIRE(o[2] == 1);               // shared edge {1,2}
}
```

- [ ] **Step 2: Header.** `include/entropy/premerge/merge.hpp`:

```cpp
#pragma once
#include "entropy/premerge/types.hpp"
#include <map>
namespace entropy::premerge {
struct Merged { EdgeList EM; NodeSet VM; };
Merged merge(const EdgeList& EA, const NodeSet& VA, const EdgeList& EB, const NodeSet& VB);
// o_i for i in S: number of edges {i,j}, j in S, present in BOTH E_A and E_B.
std::map<Node,long> overlaps(const EdgeList& EA, const EdgeList& EB, const NodeSet& S);
} // namespace entropy::premerge
```

- [ ] **Step 3: Impl.** `src/premerge/merge.cpp`:

```cpp
#include "entropy/premerge/merge.hpp"
#include <set>
#include <algorithm>
namespace entropy::premerge {
Merged merge(const EdgeList& EA, const NodeSet& VA, const EdgeList& EB, const NodeSet& VB) {
    std::set<EdgePair> es;
    for (auto [u,v] : EA) es.insert(canon(u,v));
    for (auto [u,v] : EB) es.insert(canon(u,v));
    std::set<Node> vs(VA.begin(), VA.end()); vs.insert(VB.begin(), VB.end());
    Merged M; M.EM.assign(es.begin(), es.end()); M.VM.assign(vs.begin(), vs.end());
    return M;
}
std::map<Node,long> overlaps(const EdgeList& EA, const EdgeList& EB, const NodeSet& S) {
    std::set<Node> Sset(S.begin(), S.end());
    std::set<EdgePair> a, b;
    for (auto [u,v] : EA) if (Sset.count(u) && Sset.count(v)) a.insert(canon(u,v));
    for (auto [u,v] : EB) if (Sset.count(u) && Sset.count(v)) b.insert(canon(u,v));
    std::map<Node,long> o; for (Node s : S) o[s] = 0;
    for (auto& e : a) if (b.count(e)) { o[e.first] += 1; o[e.second] += 1; }
    return o;
}
} // namespace entropy::premerge
```

- [ ] **Step 4: Add to CMake, build, run.** Expected: PASS.
- [ ] **Step 5: Commit.** `git commit -am "premerge: simple-union merge, overlaps, volume-sum invariant"`

---

### Task 6: H² minimizer — `agglo` (random-restart greedy)

**Files:**
- Create: `include/entropy/premerge/h2_min.hpp`, `src/premerge/h2_min.cpp`, `tests/premerge/test_h2_min.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Failing test.** Two disjoint triangles: optimum keeps them split, `H^P < H¹`. `tests/premerge/test_h2_min.cpp`:

```cpp
#include "entropy/premerge/h2_min.hpp"
#include "entropy/premerge/partition_entropy.hpp"
#include "entropy/premerge/h1.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
using namespace entropy::premerge;

TEST_CASE("agglo finds two-triangle split below all-in-one", "[h2]") {
    EdgeList E = {{0,1},{1,2},{0,2},{3,4},{4,5},{3,5},{2,3}}; // two triangles + 1 bridge
    NodeSet V = {0,1,2,3,4,5};
    auto [h2, part] = h2_min_agglo(E, V, /*restarts=*/200, /*seed=*/1);
    Partition all_one; for (Node n : V) all_one[n] = 0;
    REQUIRE(h2 <= h_partition(E, V, all_one) + 1e-9);     // never worse than trivial
    auto [h1, W] = h1_value(E, V);
    REQUIRE(h2 <= h1 + 1e-9);                              // H^P = H1 - benefit <= H1
}
```

- [ ] **Step 2: Header.** `include/entropy/premerge/h2_min.hpp`:

```cpp
#pragma once
#include "entropy/premerge/types.hpp"
#include <cstdint>
#include <utility>
namespace entropy::premerge {
// Random-restart greedy agglomerative (notes B.3.1). Returns (best H^P, partition).
// Upper bound on true H². Deterministic given seed.
std::pair<double,Partition> h2_min_agglo(const EdgeList& E, const NodeSet& V,
                                         int restarts, std::uint64_t seed);
} // namespace entropy::premerge
```

- [ ] **Step 3: Impl** (start all-singletons; repeatedly merge the edge-adjacent module pair that most decreases `h_partition`; keep best over random orderings). `src/premerge/h2_min.cpp`:

```cpp
#include "entropy/premerge/h2_min.hpp"
#include "entropy/premerge/partition_entropy.hpp"
#include <random>
#include <set>
#include <map>
#include <vector>
#include <algorithm>
namespace entropy::premerge {

// adjacency at module granularity: which module pairs are connected by >=1 edge
static std::set<std::pair<Node,Node>> module_adjacency(const EdgeList& E, const Partition& part) {
    std::set<std::pair<Node,Node>> adj;
    for (auto [u,v] : E) {
        Node a = part.at(u), b = part.at(v);
        if (a != b) adj.insert({std::min(a,b), std::max(a,b)});
    }
    return adj;
}

std::pair<double,Partition> h2_min_agglo(const EdgeList& E, const NodeSet& V,
                                         int restarts, std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    double best = 1e300; Partition best_part;
    for (int r = 0; r < restarts; ++r) {
        Partition part; for (Node n : V) part[n] = n;     // singletons
        double cur = h_partition(E, V, part);
        bool improved = true;
        while (improved) {
            improved = false;
            auto adj = module_adjacency(E, part);
            std::vector<std::pair<Node,Node>> pairs(adj.begin(), adj.end());
            std::shuffle(pairs.begin(), pairs.end(), rng);  // random ordering per restart
            double bestDelta = 0.0; std::pair<Node,Node> bestPair{0,0}; bool found = false;
            for (auto [a,b] : pairs) {
                Partition cand = part;
                for (auto& [n,m] : cand) if (m == b) m = a;  // merge b into a
                double h = h_partition(E, V, cand);
                if (h < cur - bestDelta - 1e-12) { bestDelta = cur - h; bestPair = {a,b}; found = true; }
            }
            if (found) {
                for (auto& [n,m] : part) if (m == bestPair.second) m = bestPair.first;
                cur -= bestDelta; improved = true;
            }
        }
        if (cur < best) { best = cur; best_part = part; }
    }
    return {best, best_part};
}
} // namespace entropy::premerge
```

- [ ] **Step 4: Add to CMake, build, run.** Expected: PASS.
- [ ] **Step 5: Commit.** `git commit -am "premerge: agglomerative H2 minimizer (upper bound, seeded)"`

---

### Task 7: H² minimizer — `exact` (brute force, small instances)

**Files:**
- Modify: `include/entropy/premerge/h2_min.hpp`, `src/premerge/h2_min.cpp`, `tests/premerge/test_h2_min.cpp`

- [ ] **Step 1: Failing test** (exact ≤ agglo; both finite; cap enforced). Append to `test_h2_min.cpp`:

```cpp
TEST_CASE("exact H2 <= agglo H2 on a small graph", "[h2][exact]") {
    EdgeList E = {{0,1},{1,2},{0,2},{3,4},{4,5},{3,5},{2,3}};
    NodeSet V = {0,1,2,3,4,5};
    auto [hex, pex] = h2_min_exact(E, V, /*max_nodes=*/12);
    auto [hag, pag] = h2_min_agglo(E, V, 200, 1);
    REQUIRE(hex <= hag + 1e-9);
}
TEST_CASE("exact refuses oversized instances", "[h2][exact]") {
    NodeSet V; EdgeList E; for (Node i=0;i<13;++i){V.push_back(i); if(i)E.push_back({i-1,i});}
    REQUIRE_THROWS_AS(h2_min_exact(E, V, 12), std::runtime_error);
}
```

- [ ] **Step 2: Header add.** In `h2_min.hpp`:

```cpp
// Certified minimum over ALL partitions (set partitions, Bell-number cost).
// Throws std::runtime_error if |V| > max_nodes. Use only for small instances.
std::pair<double,Partition> h2_min_exact(const EdgeList& E, const NodeSet& V, int max_nodes);
```

- [ ] **Step 3: Impl** (enumerate set partitions via restricted-growth strings). Add to `h2_min.cpp`:

```cpp
#include <stdexcept>
std::pair<double,Partition> h2_min_exact(const EdgeList& E, const NodeSet& V, int max_nodes) {
    if (static_cast<int>(V.size()) > max_nodes)
        throw std::runtime_error("h2_min_exact: instance exceeds max_nodes");
    int n = static_cast<int>(V.size());
    std::vector<int> a(n, 0), b(n, 0);     // restricted growth string + running max
    double best = 1e300; Partition best_part;
    auto eval = [&]() {
        Partition part; for (int i = 0; i < n; ++i) part[V[i]] = a[i];
        double h = h_partition(E, V, part);
        if (h < best) { best = h; best_part = part; }
    };
    // iterate all restricted growth strings (each = one set partition)
    while (true) {
        eval();
        int i = n - 1;
        while (i > 0 && a[i] == b[i-1] + 1) { a[i] = 0; --i; }
        if (i == 0) break;
        a[i] += 1;
        int bi = std::max(b[i-1], a[i]);
        for (int j = i; j < n; ++j) b[j] = (j==i)? bi : b[i];
        for (int j = i+1; j < n; ++j) b[j] = std::max(b[j-1], a[j]);
    }
    return {best, best_part};
}
```

- [ ] **Step 4: Build, run.** Expected: PASS. (If the RGS iteration is fiddly, the invariant tests above are the guard — `exact ≤ agglo` must hold.)
- [ ] **Step 5: Commit.** `git commit -am "premerge: exact H2 minimizer via set-partition enumeration"`

---

### Task 8: Message protocol — build M1–M4 + reconstruct

**Files:**
- Create: `include/entropy/premerge/message.hpp`, `src/premerge/message.cpp`, `tests/premerge/test_message.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Failing test** — reconstruction equals direct `h_partition` to 1e-9 (notes §B.4: `|S|=3 → 2.675732`). Build the `B.3` clique-merge `|S|=3` instance and the standalone partition, then assert. `tests/premerge/test_message.cpp`:

```cpp
#include "entropy/premerge/message.hpp"
#include "entropy/premerge/merge.hpp"
#include "entropy/premerge/partition_entropy.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
using namespace entropy::premerge;

// Two 5-cliques + bridge on each side; B remapped to share 3 anchors with A.
static void build_S3(EdgeList& EA, NodeSet& VA, EdgeList& EB, NodeSet& VB,
                     NodeSet& S, Partition& partA, Partition& partB) {
    auto clique = [](std::vector<Node> ns, EdgeList& out){
        for (size_t i=0;i<ns.size();++i) for (size_t j=i+1;j<ns.size();++j) out.push_back(canon(ns[i],ns[j]));
    };
    std::vector<Node> A1={0,1,2,3,4}, A2={5,6,7,8,9};
    clique(A1,EA); clique(A2,EA); EA.push_back(canon(4,5));
    VA={0,1,2,3,4,5,6,7,8,9};
    // B cliques on 1000.. and 2000.., remap (1000->4),(1001->3),(2000->5) => S={3,4,5}
    std::vector<Node> B1={1000,1001,1002,1003,1004}, B2={2000,2001,2002,2003,2004};
    EdgeList rawB; clique(B1,rawB); clique(B2,rawB); rawB.push_back(canon(1004,2000));
    std::map<Node,Node> remap={{1000,4},{1001,3},{2000,5}};
    std::set<Node> vb;
    for (auto [u,v]:rawB){ Node ru=remap.count(u)?remap[u]:u, rv=remap.count(v)?remap[v]:v; if(ru!=rv){EB.push_back(canon(ru,rv)); vb.insert(ru); vb.insert(rv);} }
    VB.assign(vb.begin(), vb.end());
    S={3,4,5};
    for (Node n : A1) partA[n]=0; for (Node n : A2) partA[n]=1;
    // B private modules: B1-> "B1", B2-> "B2" (anchors excluded from partB private)
    for (Node n : VB) if (n>=1000) partB[n] = (n<2000)?100:101;
}

TEST_CASE("M1-M4 reconstruction == direct H^P (|S|=3 -> 2.675732)", "[message]") {
    EdgeList EA,EB; NodeSet VA,VB,S; Partition partA,partB; build_S3(EA,VA,EB,VB,S,partA,partB);
    auto msg = build_message(EB, VB, S, partB);
    double recon = reconstruct(EA, VA, S, partA, msg);
    // ground truth: direct H^P on the merged standalone partition
    auto M = merge(EA,VA,EB,VB);
    Partition pM = partA; for (auto& [n,m]: partB) pM[n] = m;   // A-side + B-private
    for (Node s : S) pM[s] = partA.at(s);                       // anchors folded to A-side
    double direct = h_partition(M.EM, M.VM, pM);
    REQUIRE(recon == Catch::Approx(direct).epsilon(1e-9));
}
```

- [ ] **Step 2: Header.** `include/entropy/premerge/message.hpp`:

```cpp
#pragma once
#include "entropy/premerge/types.hpp"
#include <map>
#include <vector>
namespace entropy::premerge {
struct Message {
    std::map<Node,long>           M1;             // anchor B-degrees
    EdgeList                      M2;             // E_B[S]
    std::map<long,long>           M3;             // private B-degree histogram value->count
    std::map<Node,std::pair<long,long>> M4;       // module -> (private volume, private internal edges)
};
// Phase-2 adjustment (carve anchors out of their B-module) is applied inside build_message.
Message build_message(const EdgeList& EB, const NodeSet& VB, const NodeSet& S, const Partition& partB);
// A reconstructs H^{P_M}(G_M) from G_A + message (notes B.4 7-step).
double reconstruct(const EdgeList& EA, const NodeSet& VA, const NodeSet& S,
                   const Partition& partA, const Message& msg);
} // namespace entropy::premerge
```

- [ ] **Step 3: Impl** following notes §B.4. `src/premerge/message.cpp`:

```cpp
#include "entropy/premerge/message.hpp"
#include "entropy/premerge/h1.hpp"
#include "entropy/util/numerics.hpp"
#include <set>
namespace entropy::premerge {

Message build_message(const EdgeList& EB, const NodeSet& VB, const NodeSet& S, const Partition& partB) {
    std::set<Node> Sset(S.begin(), S.end());
    auto dB = degrees(EB, VB);
    Message m;
    for (Node i : S) m.M1[i] = dB.count(i) ? dB[i] : 0;                 // M1
    for (auto [u,v] : EB) if (Sset.count(u) && Sset.count(v)) m.M2.push_back(canon(u,v)); // M2 = E_B[S]
    for (auto& [n,dn] : dB) if (!Sset.count(n)) m.M3[dn] += 1;          // M3 over P_B
    // M4 per B-module over PRIVATE nodes only (anchors carved out, Phase-2)
    for (auto& [n,dn] : dB) if (!Sset.count(n)) m.M4[partB.at(n)].first += dn;   // private volume
    for (auto [u,v] : EB) {
        if (Sset.count(u) || Sset.count(v)) continue;                  // skip anchor-incident
        if (partB.at(u) == partB.at(v)) m.M4[partB.at(u)].second += 1; // private internal edge
    }
    return m;
}

double reconstruct(const EdgeList& EA, const NodeSet& VA, const NodeSet& S,
                   const Partition& partA, const Message& msg) {
    std::set<Node> Sset(S.begin(), S.end());
    auto dA = degrees(EA, VA);
    // overlaps: edges among S present in both E_A and msg.M2
    std::set<EdgePair> EA_S, M2set;
    for (auto [u,v] : EA) if (Sset.count(u) && Sset.count(v)) EA_S.insert(canon(u,v));
    for (auto& e : msg.M2) M2set.insert(canon(e.first,e.second));
    std::map<Node,long> o; for (Node s : S) o[s]=0;
    for (auto& e : EA_S) if (M2set.count(e)) { o[e.first]+=1; o[e.second]+=1; }
    // merged degrees A knows
    std::map<Node,long> dM = dA;
    for (Node i : S) dM[i] = dA[i] + msg.M1.at(i) - o[i];
    // W = A-side merged volume + B-private volume (from M3)
    long W = 0; for (auto& [n,dn] : dM) W += dn;
    for (auto& [k,nk] : msg.M3) W += k * nk;
    // module volumes & internal edges. A-modules from partA; B-modules from M4.
    std::map<Node,long> Vj, eint;
    for (auto& [n,dn] : dM) Vj[partA.at(n)] += dn;          // A-side (anchors on A-side)
    for (auto [u,v] : EA) if (partA.at(u)==partA.at(v)) eint[partA.at(u)] += 1; // A internal
    // add B-internal edges among anchors that are NOT already A-internal (from M2)
    for (auto& e : M2set) if (partA.at(e.first)==partA.at(e.second) && !EA_S.count(e)) eint[partA.at(e.first)] += 1;
    long modbase = 1; for (auto& [j,vj] : Vj) modbase = std::max<long>(modbase, j+1);
    for (auto& [bmod, vp] : msg.M4) { Vj[modbase+bmod] = vp.first; eint[modbase+bmod] = vp.second; }
    // H1(GM): A-side d log d + B-private (from M3); H1 = -(1/W) sum d log2 d + log2 W
    double sum = 0.0;
    for (auto& [n,dn] : dM) if (dn>0) sum += dn * util::safe_log2(static_cast<double>(dn));
    for (auto& [k,nk] : msg.M3) if (k>0) sum += static_cast<double>(k) * util::safe_log2((double)k) * nk;
    double H1M = -(sum / W) + util::safe_log2((double)W);
    // H(q), cuts g_j = V_j - 2 e_j^int, S-term
    double Hq = 0.0, Sterm = 0.0;
    for (auto& [j,vj] : Vj) if (vj>0) {
        double q = static_cast<double>(vj)/W;
        long g = vj - 2*eint[j];
        Hq -= util::xlog2x(q);
        Sterm -= (static_cast<double>(g)/W) * util::safe_log2(q);
    }
    return H1M - Hq + Sterm;
}
} // namespace entropy::premerge
```

- [ ] **Step 4: Add to CMake, build, run.** Expected: PASS (`recon ≈ direct`, both ≈ `2.675732`). FALSIFIER if nonzero error: carve-out or `o_i` double-count — see notes §B.4.
- [ ] **Step 5: Commit.** `git commit -am "premerge: M1-M4 message build + exact reconstruction (verified 1e-9)"`

---

### Task 9: Bias bounds

**Files:**
- Create: `include/entropy/premerge/bias.hpp`, `src/premerge/bias.cpp`, `tests/premerge/test_bias.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Failing test.** `tests/premerge/test_bias.cpp`:

```cpp
#include "entropy/premerge/bias.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
using namespace entropy::premerge;
TEST_CASE("bias bounds: sigma and seam bound", "[bias]") {
    // vol(S)=9, W=84 -> sigma=0.107..., seam=2*sigma*log2(84)
    std::map<Node,long> dM = {{1,9}}; NodeSet S={1}; long W=84;
    auto b = bias_bounds(S, dM, W);
    REQUIRE(b.sigma == Catch::Approx(9.0/84.0));
    REQUIRE(b.seam_bound == Catch::Approx(2.0*(9.0/84.0)*std::log2(84.0)));
    REQUIRE(b.renorm_cap == Catch::Approx(1.06).margin(0.01));
}
```

- [ ] **Step 2: Header + impl.** `include/entropy/premerge/bias.hpp`:

```cpp
#pragma once
#include "entropy/premerge/types.hpp"
#include <map>
namespace entropy::premerge {
struct BiasBounds { double sigma, seam_bound, renorm_cap; };
// sigma = vol_{G_M}(S)/W ; seam_bound = 2*sigma*log2(W) ; renorm_cap = 2*log2(e)/e ≈ 1.06.
BiasBounds bias_bounds(const NodeSet& S, const std::map<Node,long>& dM, long W);
} // namespace entropy::premerge
```

`src/premerge/bias.cpp`:

```cpp
#include "entropy/premerge/bias.hpp"
#include <cmath>
namespace entropy::premerge {
BiasBounds bias_bounds(const NodeSet& S, const std::map<Node,long>& dM, long W) {
    long volS = 0; for (Node s : S) { auto it = dM.find(s); if (it != dM.end()) volS += it->second; }
    double sigma = (W > 0) ? static_cast<double>(volS)/W : 0.0;
    BiasBounds b;
    b.sigma = sigma;
    b.seam_bound = 2.0 * sigma * std::log2((double)std::max<long>(W,2));
    b.renorm_cap = 2.0 * std::log2(std::exp(1.0)) / std::exp(1.0); // ≈1.0615
    return b;
}
} // namespace entropy::premerge
```

- [ ] **Step 3: Build, run.** Expected: PASS.
- [ ] **Step 4: Commit.** `git commit -am "premerge: bias bounds (sigma, seam, renorm cap)"`

---

### Task 10: E4 irredundancy witnesses as C++ regression fixtures

**Files:**
- Create: `tests/premerge/test_witnesses.cpp`
- Modify: `CMakeLists.txt`

This locks the five §7.2 / §B.5 witnesses as regression fixtures. Each builds the fixed `G_A` and two `EB` versions, asserts the three held-fixed message coordinates equal and the target differs, with the exact `H^{P_M}` values.

- [ ] **Step 1: Write the witness tests.** Transcribe the exact edge lists from `pre_merge_SI_theory.md` §B.5 (M1, M2, M3, M4-int, M4-vol) and assert the fixture pairs: `M3: 2.383605 ≠ 2.408767`, `M4-int: 2.187014 ≠ 2.574600`, `M4-vol: 2.529236 ≠ 2.542125`, `M1: 2.188297 ≠ 2.173108`, `M2: 2.125815 ≠ 2.049452`, each via `h_partition` on the merged standalone partition, to 1e-5. (Full edge lists are in §B.5; copy verbatim — `EB_w`, `EA6`, etc.)

```cpp
#include "entropy/premerge/merge.hpp"
#include "entropy/premerge/partition_entropy.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
using namespace entropy::premerge;
// helper: given EA/partA fixed, EB + partB (B-private module ids), build merged standalone
// partition and return H^P. (A-side keeps partA; B-private nodes keep their module; anchors
// fold to A-side.) Then each TEST_CASE encodes one §B.5 witness pair and asserts the values.
// ... (transcribe §B.5 EA, partA, EB_v1/EB_v2, partB and expected H^P pairs) ...
```

- [ ] **Step 2: Add to CMake, build, run.** Expected: all five pairs differ at the fixture values to 1e-5. FALSIFIER for M4-vol: if equal, a cohesionless/symmetric module was used — must be cohesive asymmetric (`EB_w`, §7.4).
- [ ] **Step 3: Commit.** `git commit -am "premerge: E4 irredundancy witnesses as regression fixtures"`

**Phase A exit:** `ctest --test-dir graph-entropy-sandbox/build --output-on-failure` → all premerge tests pass; no nats remain (`grep -rn "nats\|xlogx" graph-entropy-sandbox/src graph-entropy-sandbox/include` returns only the legacy `xlogx`/`safe_log` helpers, unused by entropy code).

---

## Phase B — pybind11 module + cross-validation

### Task 11: pybind11 module exposing the kernel

**Files:**
- Create: `graph-entropy-sandbox/bindings/python/premerge_module.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Add pybind11 + module target to CMake.** Append:

```cmake
FetchContent_Declare(pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG        v2.12.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(pybind11)
pybind11_add_module(premerge_kernel bindings/python/premerge_module.cpp)
target_link_libraries(premerge_kernel PRIVATE entropy_premerge)
apply_compiler_warnings(premerge_kernel)
```

- [ ] **Step 2: Write the binding.** `bindings/python/premerge_module.cpp` exposes `h1_value`, `h_partition`, `decompose`, `merge`, `overlaps`, `h2_min_agglo`, `h2_min_exact`, `build_message`, `reconstruct`, `bias_bounds`. Marshal `EdgeList` as `list[tuple[int,int]]`, `NodeSet` as `list[int]`, `Partition` as `dict[int,int]`, `Message` as a Python object/namedtuple. Example pattern:

```cpp
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "entropy/premerge/h1.hpp"
#include "entropy/premerge/partition_entropy.hpp"
#include "entropy/premerge/merge.hpp"
#include "entropy/premerge/h2_min.hpp"
#include "entropy/premerge/message.hpp"
#include "entropy/premerge/bias.hpp"
namespace py = pybind11; using namespace entropy::premerge;
PYBIND11_MODULE(premerge_kernel, m) {
    m.def("h1", &h1_value);                         // (E,V) -> (H1, W)
    m.def("h_partition", &h_partition);             // (E,V,part) -> float
    m.def("decompose", [](const EdgeList& E, const NodeSet& V, const Partition& p){
        auto d = decompose(E,V,p);
        return py::dict("HP"_a=d.HP,"H1"_a=d.H1,"Hq"_a=d.Hq,"S"_a=d.S,"benefit"_a=d.benefit,"W"_a=d.W);
    });
    m.def("merge", [](const EdgeList& EA, const NodeSet& VA, const EdgeList& EB, const NodeSet& VB){
        auto M = merge(EA,VA,EB,VB); return py::make_tuple(M.EM, M.VM); });
    m.def("overlaps", &overlaps);
    m.def("h2_min", [](const EdgeList& E, const NodeSet& V, const std::string& method, int restarts, std::uint64_t seed, int max_nodes){
        if (method=="exact") { auto [h,p]=h2_min_exact(E,V,max_nodes); return py::make_tuple(h,p); }
        auto [h,p]=h2_min_agglo(E,V,restarts,seed); return py::make_tuple(h,p);
    }, py::arg("E"), py::arg("V"), py::arg("method")="agglo", py::arg("restarts")=200, py::arg("seed")=0, py::arg("max_nodes")=12);
    m.def("build_message", [](const EdgeList& EB, const NodeSet& VB, const NodeSet& S, const Partition& pB){
        auto msg = build_message(EB,VB,S,pB);
        return py::dict("M1"_a=msg.M1,"M2"_a=msg.M2,"M3"_a=msg.M3,"M4"_a=msg.M4);
    });
    m.def("reconstruct", [](const EdgeList& EA, const NodeSet& VA, const NodeSet& S, const Partition& pA, const py::dict& d){
        Message msg;
        msg.M1 = d["M1"].cast<std::map<Node,long>>();
        msg.M2 = d["M2"].cast<EdgeList>();
        msg.M3 = d["M3"].cast<std::map<long,long>>();
        msg.M4 = d["M4"].cast<std::map<Node,std::pair<long,long>>>();
        return reconstruct(EA,VA,S,pA,msg);
    });
    m.def("bias_bounds", [](const NodeSet& S, const std::map<Node,long>& dM, long W){
        auto b = bias_bounds(S,dM,W); return py::make_tuple(b.sigma,b.seam_bound,b.renorm_cap); });
}
```

- [ ] **Step 3: Build the module.**
Run: `cmake --build graph-entropy-sandbox/build -j --target premerge_kernel`
Expected: produces `graph-entropy-sandbox/build/premerge_kernel*.so`.

- [ ] **Step 4: Commit.** `git commit -am "premerge: pybind11 module exposing the kernel"`

---

### Task 12: Python package skeleton + import test

**Files:**
- Create: `experiments/pyproject.toml`, `experiments/requirements.txt`, `experiments/premerge_py/__init__.py`, `experiments/premerge_py/kernel.py`, `experiments/tests/test_kernel_fixtures.py`, `experiments/conftest.py`

- [ ] **Step 1: requirements + pyproject.** `experiments/requirements.txt`:

```
networkx>=3.2
numpy>=1.26
scipy>=1.11
pandas>=2.1
matplotlib>=3.8
pytest>=8.0
```

`experiments/pyproject.toml`:

```toml
[project]
name = "premerge_py"
version = "0.1.0"
requires-python = ">=3.10"
[tool.pytest.ini_options]
testpaths = ["tests"]
```

- [ ] **Step 2: kernel.py shim** (single import point; adds the build dir to path). `experiments/premerge_py/kernel.py`:

```python
"""Import shim for the compiled C++ premerge_kernel module."""
import os, sys
_BUILD = os.environ.get("PREMERGE_BUILD_DIR",
    os.path.join(os.path.dirname(__file__), "..", "..", "graph-entropy-sandbox", "build"))
sys.path.insert(0, os.path.abspath(_BUILD))
import premerge_kernel as k  # noqa: E402
h1 = k.h1; h_partition = k.h_partition; decompose = k.decompose
merge = k.merge; overlaps = k.overlaps; h2_min = k.h2_min
build_message = k.build_message; reconstruct = k.reconstruct; bias_bounds = k.bias_bounds
```

`experiments/premerge_py/__init__.py`: empty.
`experiments/conftest.py`: empty (ensures `premerge_py` importable from `experiments/`).

- [ ] **Step 3: Fixture test through the binding.** `experiments/tests/test_kernel_fixtures.py`:

```python
from premerge_py import kernel as k

def test_h1_path():
    h1, W = k.h1([(0,1),(1,2)], [0,1,2])
    assert W == 4 and abs(h1 - 1.5) < 1e-12

def test_hpartition_path():
    hp = k.h_partition([(0,1),(1,2)], [0,1,2], {0:0,1:0,2:1})
    assert abs(hp - 1.2924812) < 1e-6

def test_decompose_identity():
    d = k.decompose([(0,1),(1,2)], [0,1,2], {0:0,1:0,2:1})
    assert abs(d["H1"] - d["Hq"] + d["S"] - d["HP"]) < 1e-12
```

- [ ] **Step 4: Build module, install deps, run pytest.**
Run:
```bash
cmake --build graph-entropy-sandbox/build -j --target premerge_kernel
pip install -r experiments/requirements.txt
cd experiments && python -m pytest tests/test_kernel_fixtures.py -v
```
Expected: 3 passed.

- [ ] **Step 5: Commit.** `git add experiments && git commit -m "premerge_py: package skeleton + kernel shim + fixture tests"`

---

### Task 13: Appendix-B reference oracle + cross-validation to 1e-9

**Files:**
- Create: `experiments/premerge_py/reference.py`, `experiments/tests/test_cross_validation.py`

- [ ] **Step 1: Transcribe Appendix-B primitives verbatim.** `experiments/premerge_py/reference.py` = the pure-Python `degrees`, `H1`, `H_partition` from `pre_merge_SI_theory.md` §B.1, but in **bits** (the §B.1 code already uses `log2`). This is the TEST ORACLE only.

```python
from math import log2
def degrees(edges, nodes):
    d = {v: 0 for v in nodes}
    for (u, v) in edges: d[u] += 1; d[v] += 1
    return d
def H1(edges, nodes):
    d = degrees(edges, nodes); W = sum(d.values())
    return (-sum((di/W)*log2(di/W) for di in d.values() if di > 0), W)
def H_partition(edges, nodes, part):
    d = degrees(edges, nodes); W = sum(d.values())
    mods = {}
    for v in nodes: mods.setdefault(part[v], []).append(v)
    Vj = {j: sum(d[v] for v in mem) for j, mem in mods.items()}
    g = {j: 0 for j in mods}; eint = {j: 0 for j in mods}
    for (u, v) in edges:
        if part[u] == part[v]: eint[part[u]] += 1
        else: g[part[u]] += 1; g[part[v]] += 1
    term1 = -sum((d[v]/W)*log2(d[v]/Vj[j]) for j, mem in mods.items() for v in mem if d[v] > 0 and Vj[j] > 0)
    term2 = -sum((g[j]/W)*log2(Vj[j]/W) for j in mods if Vj[j] > 0)
    return term1 + term2
```

- [ ] **Step 2: Cross-validation test on random SBM instances.** `experiments/tests/test_cross_validation.py`:

```python
import random, networkx as nx
from premerge_py import kernel as k, reference as ref

def _edges_nodes(G):
    return [(int(u), int(v)) for u, v in G.edges()], [int(n) for n in G.nodes()]

def test_h1_matches_reference():
    rng = random.Random(0)
    for _ in range(50):
        G = nx.stochastic_block_model([8, 8], [[0.6, 0.05], [0.05, 0.6]], seed=rng.randint(0, 10**6))
        E, V = _edges_nodes(G)
        if not E: continue
        h_c, W_c = k.h1(E, V); h_r, W_r = ref.H1(E, V)
        assert W_c == W_r and abs(h_c - h_r) < 1e-9

def test_hpartition_matches_reference():
    rng = random.Random(1)
    for _ in range(50):
        G = nx.stochastic_block_model([7, 7], [[0.6, 0.05], [0.05, 0.6]], seed=rng.randint(0, 10**6))
        E, V = _edges_nodes(G)
        if not E: continue
        part = {n: (0 if n < 7 else 1) for n in V}
        assert abs(k.h_partition(E, V, part) - ref.H_partition(E, V, part)) < 1e-9
```

- [ ] **Step 3: Run.** `cd experiments && python -m pytest tests/test_cross_validation.py -v` → 2 passed.
- [ ] **Step 4: Commit.** `git commit -am "premerge_py: Appendix-B reference oracle + C++↔Python cross-validation (1e-9)"`

**Phase B exit:** kernel callable from Python; kernel == reference to 1e-9 on random instances.

---

## Phase C — Python harness primitives

### Task 14: Generators (SBM/LFR + degree-preserving rewire)

**Files:**
- Create: `experiments/premerge_py/generators.py`, `experiments/tests/test_generators.py`

- [ ] **Step 1: Failing test** (rewire preserves the degree sequence). `experiments/tests/test_generators.py`:

```python
import networkx as nx
from premerge_py import generators as gen

def test_rewire_preserves_degrees():
    G = nx.stochastic_block_model([10, 10], [[0.6, 0.05], [0.05, 0.6]], seed=3)
    before = sorted(d for _, d in G.degree())
    H = gen.degree_preserving_rewire(G, n_swaps=200, seed=7)
    after = sorted(d for _, d in H.degree())
    assert before == after

def test_sbm_returns_planted_partition():
    E, V, planted = gen.gen_sbm([10, 10], p_in=0.6, p_out=0.05, seed=1)
    assert set(planted.values()) == {0, 1} and len(V) == 20
```

- [ ] **Step 2: Implement.** `experiments/premerge_py/generators.py`:

```python
import networkx as nx

def gen_sbm(sizes, p_in, p_out, seed):
    L = len(sizes)
    P = [[p_in if i == j else p_out for j in range(L)] for i in range(L)]
    G = nx.stochastic_block_model(sizes, P, seed=seed)
    planted = {}; base = 0
    for blk, sz in enumerate(sizes):
        for n in range(base, base + sz): planted[n] = blk
        base += sz
    return [(int(u), int(v)) for u, v in G.edges()], [int(n) for n in G.nodes()], planted

def gen_lfr(n, mu, seed, tau1=3, tau2=1.5, average_degree=8, min_community=10):
    G = nx.LFR_benchmark_graph(n, tau1, tau2, mu, average_degree=average_degree,
                               min_community=min_community, seed=seed)
    G.remove_edges_from(nx.selfloop_edges(G))
    planted = {}
    for n_ in G.nodes():
        comm = frozenset(G.nodes[n_]["community"])
        planted[int(n_)] = hash(comm) & 0xffffffff
    return [(int(u), int(v)) for u, v in G.edges()], [int(x) for x in G.nodes()], planted

def degree_preserving_rewire(G, n_swaps, seed):
    H = G.copy()
    nx.double_edge_swap(H, nswap=n_swaps, max_tries=n_swaps * 20, seed=seed)
    return H
```

(LFR can fail to generate at extreme μ — callers catch `nx.ExceededMaxIterations` and resample, per spec E2 pitfalls.)

- [ ] **Step 3: Run.** `cd experiments && python -m pytest tests/test_generators.py -v` → 2 passed.
- [ ] **Step 4: Commit.** `git commit -am "premerge_py: SBM/LFR generators + degree-preserving rewire"`

---

### Task 15: Estimators (standalone / bridge / oracle)

**Files:**
- Create: `experiments/premerge_py/estimators.py`, `experiments/tests/test_estimators.py`

- [ ] **Step 1: Failing test** (ordering `H_standalone ≥ H_bridge ≥ H_oracle` on a small merge). `experiments/tests/test_estimators.py`:

```python
from premerge_py import estimators as est, kernel as k

def _two_triangle_merge():
    # G_A: triangle {0,1,2}; G_B: triangle {2,3,4} sharing anchor 2
    EA, VA = [(0,1),(1,2),(0,2)], [0,1,2]
    EB, VB = [(2,3),(3,4),(2,4)], [2,3,4]
    S = [2]
    return EA, VA, EB, VB, S

def test_partition_ordering():
    EA, VA, EB, VB, S = _two_triangle_merge()
    EM, VM = k.merge(EA, VA, EB, VB)
    p_stand = est.partition_standalone(EA, VA, EB, VB, S)
    p_bridge = est.partition_bridge(EA, VA, EB, VB, S)
    h_stand = k.h_partition(EM, VM, p_stand)
    h_bridge = k.h_partition(EM, VM, p_bridge)
    h_oracle, _ = k.h2_min(EM, VM, method="exact", max_nodes=12)
    assert h_stand >= h_bridge - 1e-9 >= 0
    assert h_bridge >= h_oracle - 1e-9
```

- [ ] **Step 2: Implement P9 (estimators are Python over the kernel; bridge = restricted local re-optimization).** `experiments/premerge_py/estimators.py`:

```python
from premerge_py import kernel as k

def _h2_part(E, V, restarts=200, seed=0):
    _, part = k.h2_min(E, V, method="agglo", restarts=restarts, seed=seed)
    return part

def partition_standalone(EA, VA, EB, VB, S, restarts=200, seed=0):
    Sset = set(S)
    pA = _h2_part(EA, VA, restarts, seed)
    pB = _h2_part(EB, VB, restarts, seed)
    part = {}
    for v in VA: part[v] = ("A", pA[v])                 # anchors land on A-side
    for v in VB:
        if v not in Sset: part[v] = ("B", pB[v])        # only B-private
    # relabel to ints for the kernel
    labels = {lab: i for i, lab in enumerate(sorted(set(part.values()), key=str))}
    return {n: labels[lab] for n, lab in part.items()}

def _neighbors_in(EM, S):
    Sset = set(S); nbr = set(S)
    for u, v in EM:
        if u in Sset: nbr.add(v)
        if v in Sset: nbr.add(u)
    return nbr

def partition_bridge(EA, VA, EB, VB, S, restarts=200, seed=0):
    EM, VM = k.merge(EA, VA, EB, VB)
    part = partition_standalone(EA, VA, EB, VB, S, restarts, seed)
    movable = _neighbors_in(EM, S)
    modules = sorted(set(part.values()))
    cur = k.h_partition(EM, VM, part)
    improved = True
    while improved:
        improved = False
        for node in movable:
            best_m, best_h = part[node], cur
            for m in modules:
                if m == part[node]: continue
                trial = dict(part); trial[node] = m
                h = k.h_partition(EM, VM, trial)
                if h < best_h - 1e-12: best_h, best_m = h, m
            if best_m != part[node]:
                part[node] = best_m; cur = best_h; improved = True
    return part

def partition_oracle(EM, VM, method="agglo", restarts=400, seed=0, max_nodes=12):
    h, part = k.h2_min(EM, VM, method=method, restarts=restarts, seed=seed, max_nodes=max_nodes)
    return h, part
```

- [ ] **Step 3: Run.** `cd experiments && python -m pytest tests/test_estimators.py -v` → passed.
- [ ] **Step 4: Commit.** `git commit -am "premerge_py: standalone/bridge/oracle estimators (P9)"`

---

### Task 16: Results schema writer + manifest + summary

**Files:**
- Create: `experiments/premerge_py/results.py`, `experiments/premerge_py/summary.py`, `experiments/tests/test_results.py`

- [ ] **Step 1: Failing test** (schema columns present; manifest written; deterministic seed). `experiments/tests/test_results.py`:

```python
import json, os
from premerge_py import results as R

SCHEMA = R.SCHEMA_COLUMNS

def test_schema_and_manifest(tmp_path):
    run = R.RunWriter("E_test", base_seed=42, out_root=str(tmp_path))
    run.add_row({c: 0 for c in SCHEMA})
    run.close(config={"k": "v"})
    # results file + manifest exist
    files = os.listdir(run.run_dir)
    assert any(f.endswith(".csv") for f in files) and "manifest.json" in files
    man = json.load(open(os.path.join(run.run_dir, "manifest.json")))
    assert man["base_seed"] == 42 and "git_commit" in man

def test_derived_seed_deterministic():
    assert R.derive_seed(42, "inst-1") == R.derive_seed(42, "inst-1")
    assert R.derive_seed(42, "inst-1") != R.derive_seed(42, "inst-2")
```

- [ ] **Step 2: Implement results.py** (fixed §8.1 schema; splitmix64 seeds; manifest with git SHA). `experiments/premerge_py/results.py`:

```python
import csv, json, os, subprocess, hashlib, datetime

SCHEMA_COLUMNS = [
    "experiment_id","instance_id","dataset","params",
    "n_VA","n_VB","n_S","W","sigma",
    "H1_GA","H1_GB","H1_GM","H2_GA","H2_GB",
    "H_standalone","H_bridge","H2_oracle","oracle_method","oracle_restarts",
    "DeltaH1","DeltaHq","DeltaS",
    "gain_standalone","gain_bridge","gain_truth",
    "realized_bias","seam_bound","renorm_cap","within_bound",
    "message_bits","n_EB_S","n_hist_bins","n_B_modules",
    "seed","timestamp","code_version",
]

def _splitmix64(x):
    x = (x + 0x9E3779B97F4A7C15) & 0xFFFFFFFFFFFFFFFF
    x = ((x ^ (x >> 30)) * 0xBF58476D1CE4E5B9) & 0xFFFFFFFFFFFFFFFF
    x = ((x ^ (x >> 27)) * 0x94D049BB133111EB) & 0xFFFFFFFFFFFFFFFF
    return x ^ (x >> 31)

def derive_seed(base_seed, instance_id):
    h = int.from_bytes(hashlib.sha256(str(instance_id).encode()).digest()[:8], "little")
    return _splitmix64(base_seed ^ h)

def _git(args, default="unknown"):
    try: return subprocess.check_output(["git"] + args, text=True).strip()
    except Exception: return default

class RunWriter:
    def __init__(self, experiment_id, base_seed, out_root="results"):
        self.experiment_id = experiment_id; self.base_seed = base_seed
        ts = datetime.datetime.utcnow().strftime("%Y-%m-%dT%H-%M-%SZ")
        self.run_dir = os.path.join(out_root, experiment_id, ts)
        os.makedirs(self.run_dir, exist_ok=True)
        self.rows = []; self.timestamp = ts
        self.code_version = _git(["rev-parse", "HEAD"])
    def add_row(self, row):
        full = {c: row.get(c, "") for c in SCHEMA_COLUMNS}
        full["experiment_id"] = self.experiment_id
        full["timestamp"] = self.timestamp; full["code_version"] = self.code_version
        self.rows.append(full)
    def close(self, config):
        csv_path = os.path.join(self.run_dir, "results.csv")
        with open(csv_path, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=SCHEMA_COLUMNS); w.writeheader(); w.writerows(self.rows)
        cfg = json.dumps(config, sort_keys=True)
        manifest = {
            "experiment_id": self.experiment_id, "base_seed": self.base_seed,
            "git_commit": self.code_version,
            "git_dirty": _git(["status", "--porcelain"]) != "",
            "config": config, "config_sha256": hashlib.sha256(cfg.encode()).hexdigest(),
            "n_rows": len(self.rows), "timestamp": self.timestamp,
        }
        json.dump(manifest, open(os.path.join(self.run_dir, "manifest.json"), "w"), indent=2)
        return self.run_dir
```

- [ ] **Step 3: Implement summary.py** (§8.3 PASS/FAIL/NULL aggregator). `experiments/premerge_py/summary.py`:

```python
import csv, os
def write_summary(rows, out_path):
    # rows: list of dicts with keys experiment, claim, metric, threshold, observed, status
    cols = ["experiment","claim","metric","threshold","observed","status"]
    with open(out_path, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=cols); w.writeheader()
        for r in rows: w.writerow({c: r.get(c, "") for c in cols})
    return out_path
```

- [ ] **Step 4: Run.** `cd experiments && python -m pytest tests/test_results.py -v` → passed.
- [ ] **Step 5: Commit.** `git commit -am "premerge_py: §8.1 results schema + manifest + §8.3 summary"`

---

### Task 17: End-to-end smoke harness

**Files:**
- Create: `experiments/tests/test_smoke_e2e.py`

- [ ] **Step 1: Write the smoke test** (one synthetic merge → standalone vs oracle → schema row). `experiments/tests/test_smoke_e2e.py`:

```python
from premerge_py import kernel as k, estimators as est, generators as gen, results as R

def test_end_to_end(tmp_path):
    EA, VA, _ = gen.gen_sbm([6, 6], 0.7, 0.05, seed=1)
    EB, VB, _ = gen.gen_sbm([6, 6], 0.7, 0.05, seed=2)
    VB = [n + 100 for n in VB]; EB = [(u + 100, v + 100) for u, v in EB]
    # share 2 anchors by id remap
    EB = [(u if u not in (100,) else 0, v if v not in (100,) else 0) for u, v in EB]
    S = sorted(set(VA) & set(VB))
    EM, VM = k.merge(EA, VA, EB, VB)
    p_stand = est.partition_standalone(EA, VA, EB, VB, S)
    h_stand = k.h_partition(EM, VM, p_stand)
    h_oracle, _ = est.partition_oracle(EM, VM, method="agglo", restarts=100, seed=1)
    assert h_stand >= h_oracle - 1e-9
    run = R.RunWriter("E_smoke", base_seed=1, out_root=str(tmp_path))
    run.add_row({"instance_id": "0", "H_standalone": h_stand, "H2_oracle": h_oracle})
    run.close(config={"smoke": True})
```

- [ ] **Step 2: Run full suite.**
Run: `cd experiments && python -m pytest -v`
Expected: all tests pass.

- [ ] **Step 3: Commit.** `git commit -am "premerge_py: end-to-end smoke harness"`

**Phase C exit (Foundation done):** `ctest` (C++) green; `pytest` (Python) green; a synthetic merge flows end-to-end to a schema-valid results file with a manifest.

---

## Self-review notes (coverage)

- Spec primitives P1–P11 → Tasks 3–9, 14–15 (P12 proxies deferred to Plan 04/Group 4).
- bits everywhere → Task 2 (+ §6 spec edit); fixtures in bits throughout.
- C++↔Python verification → Task 13.
- §8.1 results schema / reproducibility → Task 16.
- E4 witnesses → Task 10 (C++); re-packaged as the E4 runner in Plan 03.
- Deferred to later plans: E1/E2 (Plan 02), E3/E4 (Plan 03), E5/E6/E-cert (Plan 04), E7/E8 + baselines (Plan 05, real data — LAST).
