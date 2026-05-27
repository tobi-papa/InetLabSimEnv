# Experiment A — Implementation Plan

## Context

**Goal.** Implement *Experiment A* ("Does Community Structure Matter for Gain?") exactly as
specified in `/home/tobi/SimEnv/Experiment_A_Rewritten_Implementation_Plan_Final.md` (the
authoritative spec; the `docs/superpowers/specs/Exp_A_implementation_prompt` file only points to
it and adds scientific-integrity rules).

The experiment tests whether a **1D degree-only** structural-entropy estimate is sufficient to
estimate Alice's 2D structural merge gain, or whether a **2D community-aware** estimate is
necessary, as Bob's realized community strength varies. It is a **bias-controlled, non-forced**
experiment: positive (Case A), compact-only-failure (Case B), negative (Case C), and ambiguous
(Case D) are all valid outcomes and must be reported honestly.

**Why now / what prompted it.** The reusable C++ sandbox (`graph-entropy-sandbox/`) already
implements the skeleton (Graph CSR, registries, config, CSV/JSON export, 1D structural entropy in
*nats*). Experiment 1 (on branch `experiment-1-mi-overlap`) established the precedent for adding a
new experiment to this skeleton: **C++20 for all core computation + Catch2 tests, Python only for
golden-reference generation and analysis/plots**, with a dedicated experiment runner + CLI
subcommand that writes its own trial-level CSV (see `src/experiments/mi_overlap_runner.cpp`,
`src/generators/sbm.cpp`, `tests/` on that branch).

**Decisions locked with the user:**
1. **Fresh on the skeleton.** Build Experiment A on the current `Experiment_A` branch (bare
   skeleton). Do **not** merge `experiment-1-mi-overlap`. Reimplement what is needed
   (SBM, runner, test harness) scoped to Experiment A. Use the exp-1 files only as a *reference
   pattern* (read via `git show experiment-1-mi-overlap:<path>`), not as a dependency.
2. **Build + smoke; defer the full run.** Implement core library, tests, runner, and analysis;
   validate with a tiny smoke grid. Document (but do not execute) the full 660-trial command.
3. **log2 per spec.** Implement Experiment A entropy in **log2 (bits)** as a *new, separate* code
   path. Leave the existing nats-based `structural_entropy.cpp` plugin untouched.

**Intended outcome.** A reproducible Experiment-A pipeline: `entropy-cli exp-a --config <toml>`
produces a raw trial-level CSV (schema = spec §15) + a `_null` CSV + a logged config/manifest with
git SHA and seeds; a Python analysis script consumes the CSV to compute cell summaries, bootstrap
CIs, correlations, regressions, the required plots (spec §14), and emits a report classifying the
result as Case A/B/C/D using the spec §13 logic. All 15 required tests (spec §B) pass, none of
which asserts the hypothesis is confirmed.

---

## Scientific-integrity guardrails (apply throughout)

These come from `Exp_A_implementation_prompt` and spec §11/§18. They are **binding**:
- Use **full graph degree `d_i`** in the `H_P` node term — never internal community degree.
- Select the H2 oracle partition by **minimum `H_P`**, never by maximum modularity.
- Always inject the **single-block partition** into the candidate set ⇒ `H2_est(G) <= H1(G)+TOL`.
- **Independent** `G_A`/`G_B` generation; **do not** prevent duplicate shared-shared edges in the
  primary experiment — record overlap instead.
- Reject trials **only** for predeclared reasons (disconnected / invalid probability / failed hard
  sanity check / failed construction invariant). Never drop a trial for weakening the hypothesis.
- Fix grid/thresholds/metrics **before** running; no post-hoc tuning.
- **No test may assert the hypothesis is confirmed** (spec §B.15). No mock CSVs/plots/results.
- Every CSV row must come from a real completed trial. All outputs reproducible from config+seed.
- Report `mu_B = 0.75` (≈ER), never call `0.50` the near-ER endpoint.
- Conclusions say "**2D structural entropy is supported under these tested conditions**", never
  "K-dimensional entropy is necessary".

---

## Reuse map (what already exists — do not rewrite)

| Need | Reuse from | Path |
|---|---|---|
| Immutable CSR graph + builder | `Graph`, `GraphBuilder` | `include/entropy/core/graph.hpp` |
| Neighbour iteration | `Graph::neighbors(v)` | same |
| Degrees | `GraphCache::degrees()` / `.degree_distribution()` | `include/entropy/core/graph_cache.hpp` |
| Deterministic RNG | `util::splitmix64`, `util::Rng` | `include/entropy/util/rng.hpp` |
| `x·ln x` helper (pattern) | `util::xlogx` (nats) | `include/entropy/util/numerics.hpp` |
| Git SHA / dirty in manifest | already configured | `CMakeLists.txt` lines 16-47, `version.hpp.in` |
| Plugin/whole-archive linking pattern | `entropy_plugins` OBJECT lib | `CMakeLists.txt` lines 161-188 |
| CLI scaffold (CLI11) | `apps/cli/main.cpp` |
| SBM core loop (reference only) | `git show experiment-1-mi-overlap:graph-entropy-sandbox/src/generators/sbm.cpp` |
| Trial-CSV runner + tests layout (reference only) | exp-1 `src/experiments/`, `tests/CMakeLists.txt` |

**New log2 helper** (existing `xlogx` is natural-log; Exp A needs bits): add
`util::xlog2x(double p)` returning `p>0 ? p*std::log2(p) : 0.0` and `util::log2_safe`.

---

## Directory layout (new files, fresh on `Experiment_A`)

```
graph-entropy-sandbox/
  include/entropy/exp_a/
    entropy2d.hpp        // compute_H1, compute_HP, H2 search + diagnostics structs
    sbm_gen.hpp          // mu->p conversion, planted SBM, connectivity, rewiring, union
    estimators.hpp       // compact 1D summary estimator, oracle 1D degree, modularity
    trial.hpp            // TrialConfig, TrialRecord (all spec §15 fields), grid def
  src/exp_a/
    entropy2d.cpp
    sbm_gen.cpp
    estimators.cpp
    experiment_a_runner.cpp   // rejection sampling, null control, CSV writer, manifest/log
  apps/cli/main.cpp      // EDIT: add `exp-a` subcommand
  configs/
    exp_a_smoke.toml     // tiny grid for validation
    exp_a_full.toml      // full 660-trial grid (documented, not run)
  tests/
    CMakeLists.txt       // NEW (not present on this branch) — Catch2 test target
    unit/test_entropy2d.cpp
    unit/test_sbm_gen.cpp
    unit/test_estimators.cpp
    unit/test_union_invariants.cpp
    unit/test_reproducibility.cpp
    unit/test_csv_schema.cpp
    golden/test_golden_entropy2d.cpp
    golden/reference_values.json        // generated by Python (real, committed)
  analysis/exp_a/
    generate_reference.py   // golden reference for H1/HP on tiny known graphs
    analyze_exp_a.py        // cells, bootstrap CIs, correlations, regressions, plots, report
    requirements.txt        // numpy, scipy, pandas, matplotlib, statsmodels
  CMakeLists.txt         // EDIT: add src/exp_a/*.cpp to entropy_plugins (or new lib); enable tests
```

Outputs land in `results/exp_a/<UTC-timestamp>/`: `trials.csv`, `trials_null.csv`,
`config.toml` (verbatim), `manifest.json` (git SHA, seeds, grid, counts), `run.log`, and after
analysis `summary_cells.csv`, `plots/*.png`, `report.md`.

---

## Core library (implement first, with these exact formulas)

All entropy in **log2**. `m = num_edges`, `total_volume = 2m`, `d_i` = full-graph degree.

### `entropy2d.hpp/.cpp`

- `double compute_H1(const Graph& g)` — spec §3.2.
  `m==0 ⇒ 0.0`; else `-Σ_i (d_i/2m) log2(d_i/2m)`, skipping `d_i==0`.
  Also provide `double H1_from_degree_map(const std::unordered_map<NodeId,double>& deg)` for the
  compact estimator (volume = Σ deg; treats virtual node as a degree entry).

- `double compute_HP(const Graph& g, const std::vector<int>& partition)` — spec §3.3, **exact
  pseudocode in lines 167-193**. Per module j: `V_j = Σ_{i∈X_j} d_i` (full degrees);
  `g_j` = #edges with exactly one endpoint in X_j; add `-(g_j/2m) log2(V_j/2m)` when `g_j>0`,
  then `-Σ_{i∈X_j}(d_i/2m) log2(d_i/V_j)`. **Forbidden:** internal degree in the node term.

- `struct H2Result { double h2_est; std::vector<int> best_partition; int best_partition_id;
  std::string method; uint64_t seed; int n_candidates; int n_unique_scores; double score_best,
  score_second_best, score_median, score_std, best_minus_second; int n_within_1e6, n_within_1e4,
  n_communities_best; bool fragility_warning; };`

- `H2Result compute_H2(const Graph& g, uint64_t base_seed)` — spec §3.4 + §4:
  - Candidate set (each scored by `compute_HP`, min wins):
    1. **single-block** (mandatory; guarantees `H2_est <= H1`),
    2. multi-seed structural-entropy-minimization candidates (greedy agglomerative / local
       refinement driven by ΔH_P; `seeds_G = max(10, ceil(num_nodes/50))` per spec §4),
    3. optional all-singletons (diagnostic),
    4. optional Louvain-style candidates **scored only by H_P**.
  - Record all §4 diagnostics. Set `fragility_warning` when `n_candidates` low AND `score_std`
    high AND best appears once (spec lines 288-293).
  - **Implementation note (ambiguity to confirm during impl, not silently resolved):** the spec
    mandates "structural-entropy-minimization candidates from multiple seeds" but does not fix the
    exact local-search operator. Plan: implement a documented greedy ΔH_P agglomeration seeded
    from random initial labelings + planted labels; record `method`/`seed` per candidate. If a
    stronger optimizer is needed for credible minima on G_merge, add Louvain candidates (scored by
    H_P only). Do not reduce seed counts after seeing results.

- `double modularity(const Graph& g, const std::vector<int>& partition)` — spec §8:
  `Q = Σ_j [ e_jj/m - (V_j/2m)^2 ]`, `e_jj` = edges fully inside j, `V_j` = full degrees.

### `sbm_gen.hpp/.cpp`

- `struct SbmParams { int n, k; double avg_degree, mu; uint64_t seed; }`.
- `mu_to_p(params) -> {p_in, p_out}` — spec §6.3:
  `p_in = avg_degree*(1-mu)/(n/k - 1)`, `p_out = avg_degree*mu/(n - n/k)`. Assert both in [0,1]
  (else hard-reject the cell as invalid probability).
- `struct PlantedGraph { Graph g; std::vector<int> community; }`.
- `PlantedGraph generate_sbm(SbmParams, const std::vector<NodeId>& node_ids)` — equal blocks;
  follows the exp-1 SBM loop pattern but using `mu_to_p`. Honors a **global ID space**: caller
  supplies the node IDs for this graph so shared nodes = first `n_shared` global IDs (spec §6.6).
  Shared nodes may carry different planted labels in A vs B (intentional).
- `bool is_connected(const Graph&)` — BFS/union-find over `neighbors`.
- `struct MergeResult { Graph g_merge; uint64_t edge_overlap_AB; uint64_t shared_shared_overlap; }`
  `union_graphs(G_A, G_B, V_shared)` — simple union, no self-loops/multi-edges; dedup an edge
  present in both ⇒ counts once; record `edge_overlap_AB = |E_A ∩ E_B|` and
  `shared_shared_edge_overlap`. **Identity:** `edges_merge = edges_A + edges_B - edge_overlap_AB`
  (spec §6.8). Do **not** prevent shared-shared duplicates.
- `Graph rewire_degree_preserving(const Graph& g_B, uint64_t seed, int& successful_swaps)` —
  spec §11.3: double-edge swaps preserving every node's degree (incl. shared); simple+connected;
  `>= 10*|E_B|` attempted swaps; return successful-swap count. Used to build `G_B_null` then
  `G_merge_null = union(G_A, G_B_null)`.

### `estimators.hpp/.cpp`

- `realized_mu_B(G_B, community_B)` — spec §7: crossing half-edges / total_volume_B.
- **Compact 1D** — spec §9.2, exact pseudocode lines 584-602:
  summary = `{vol_B = 2|E_B|, degrees_shared}`. Start from `G_A` degrees; add Bob shared degrees
  on shared nodes; `vol_unshared_B = vol_B - Σ degrees_shared` (flag invalid if `<0`); add a single
  `virtual_B` node with that volume; `H1_merge_compact = H1_from_degree_map(...)`;
  `gain_1D_compact = H1_merge_compact - H1_A`. `summary_compact_size = n_shared + 1`.
- **Oracle 1D degree** — spec §9.3: `gain_1D_oracle_degree = H1(G_merge) - H1(G_A)`. Does not touch
  graph generation.
- **Errors** — spec §10 (`TOL_GAIN = 1e-9`): for est ∈ {compact, oracle_degree}:
  `abs = |gain_oracle_est - gain_est|`; `rel = abs/|gain_oracle_est|` if denom>TOL else NaN/undef;
  `norm_B = abs/H2_est(G_B)` if H2_B>TOL else undef. `SMI_est` per spec; recorded as diagnostic.

### `trial.hpp`

- `TrialRecord` holds **every** field in spec §15 (≈90 fields incl. all H2 oracle diagnostics for
  A/B/merge, realized stats, modularity_planted/H2, rejection counts, sanity flags). A
  `to_csv_header()` / `to_csv_row()` pair guarantees schema completeness (tested — see test 14).
- Grid constants (spec §6.5, fixed before running): `n_A=n_B=300, k_A=k_B=4, avg_degree=10,
  mu_A=0.20`, `mu_B ∈ {0.05,0.075,0.10,0.15,0.20,0.30,0.40,0.50,0.60,0.70,0.75}`,
  `n_shared ∈ {15,30,90}`, `reps=20` ⇒ 660 accepted trials. Smoke config overrides to a tiny grid.

---

## Tests (write and pass BEFORE the full run — spec §B)

Catch2 (matching exp-1). Create `tests/CMakeLists.txt` (new on this branch) mirroring the exp-1
target wiring (whole-archive link of plugin objects, `catch_discover_tests`). Hook it in from the
top-level `CMakeLists.txt` behind a `BUILD_TESTING`/`option(EXP_A_TESTS ...)` guard.

Implement **all 15 required tests** (spec lines 38-55), one TEST_CASE each:
1. `compute_H1` of edgeless graph == 0.
2. `compute_H1(K_n) == log2(n)` (build K_n via GraphBuilder).
3. Single-block partition ⇒ `compute_HP == compute_H1` (within TOL).
4. `compute_H2` candidate set **always includes** single-block (assert present).
5. `compute_H2_est(G) <= compute_H1(G) + TOL`.
6. `compute_HP` uses full degree, not internal: construct a graph where the two differ and assert
   the full-degree value (guards the forbidden implementation).
7. Union identity `edges_merge == edges_A + edges_B - edge_overlap_AB` (incl. a case with a
   deliberate shared-shared duplicate).
8. Same seed ⇒ identical graphs (content_hash) and identical trial output row.
9. Different trial seeds ⇒ distinct logged seeds.
10. Compact estimator touches Bob only via `vol_B` and shared-node degrees (enforced by the
    estimator's signature taking only the summary struct; test asserts result is invariant to
    Bob-only topology changes that preserve vol_B and shared degrees).
11. `gain_1D_oracle_degree == H1(G_merge) - H1(G_A)` exactly.
12. H2 oracle picks **min H_P**, not max modularity: craft candidates where the argmin-H_P and
    argmax-Q partitions differ; assert the min-H_P one is chosen.
13. Disconnected graph is rejected (or routed to an explicit disconnected formula — we **reject**
    per spec §5); assert rejection path triggers.
14. All spec §15 CSV fields present in header (compare `to_csv_header()` against the canonical
    field list).
15. **No test asserts the hypothesis is confirmed** — enforced by code review + a meta-comment;
    add a grep-style CI note. (This is a constraint on tests 1-14, not a runnable assertion.)

**Plus** correctness-only tests kept separate from outcome analysis (spec §B.8): SBM `mu_to_p`
bounds, realized-mu sanity, modularity formula on a known partition, rewiring degree-preservation +
connectivity, reproducibility of full trial given seed.

**Golden test:** `analysis/exp_a/generate_reference.py` computes `H1`/`H_P`/`H2` (single-block) on a
handful of tiny hand-checkable graphs (path, cycle, K4, two-triangles-bridge) in pure Python
(log2), writes `reference_values.json` (real values, committed). `test_golden_entropy2d.cpp` asserts
the C++ matches within 1e-9. This cross-checks the C++ entropy against an independent impl.

---

## Experiment runner (`experiment_a_runner.cpp`, after tests pass — spec §C)

Dedicated runner (not the benchmarking `ExperimentRunner`), following the exp-1
`mi_overlap_runner.cpp` pattern. For each cell `(mu_B, n_shared)` × `rep`:
1. **Seed derivation:** deterministic per trial via `util::splitmix64` over `(base_seed, mu_B_idx,
   n_shared_idx, rep)`; log the rule + each trial seed (test 8/9).
2. **Rejection sampling** (spec §5): regenerate `G_A` until connected; `G_B` until connected;
   `G_merge` until connected. Count `rejected_A/B/merge_disconnected` and
   `rejected_draws_before_acceptance`. Validate probabilities (spec §6.3) before generating.
3. Compute `H1`/`H2_est` for A, B, merge; `gain_oracle_est = H2_est(merge) - H2_est(A)`; compact +
   oracle-degree estimators; all errors; `modularity_B_planted`, `modularity_B_H2_partition`;
   realized stats; all H2 oracle diagnostics.
4. **Hard sanity checks** (spec §16.1) — fail/reject on violation: size identities, connectedness,
   union identity, `H2_est <= H1 + TOL`, `summary_compact_size == n_shared+1`, prob validity.
   **Warning checks** (spec §16.2) recorded, never auto-reject (negative SMI/gain, fragility,
   narrow modularity, high rejection rate).
5. **Null control** (spec §11.3): build `G_B_null` via degree-preserving rewiring,
   `G_merge_null = union(G_A, G_B_null)`, recompute the same quantities ⇒ `trials_null.csv` keyed
   by `trial_id`.
6. **Write raw trial CSV before any aggregation** (spec §12.1). Write `config.toml` verbatim +
   `manifest.json` (git SHA from `version.hpp`, all seeds, full param values, grid, accepted/
   rejected counts) + `run.log`.
7. CLI: add `exp-a` subcommand to `apps/cli/main.cpp` (CLI11 subcommand), `--config`,
   `--output-dir`, optional `--smoke`. Print before any conclusions: accepted/rejected counts, all
   params, git SHA, seeds/seed-rule, test status reference, and "did code change after seeing
   outputs?" field (spec lines 84-91).

**Compute scope:** runner is correct and runnable on the smoke grid during this work. The full
660+660 run is **not executed** here; document the exact command in `report`/README.

---

## Analysis & report (`analyze_exp_a.py`, Python — spec §12-14, §19)

Consumes `trials.csv` + `trials_null.csv`. Computes (no thresholds tuned post-hoc):
- **Cell summaries** (spec §12.2): per `(mu_B, n_shared)` means/std of modularity, gain, errors; `n`.
- **Correlations** (spec §12.3): trial-level + **cell-level** Spearman of modularity vs
  abs/rel/norm_B error (cell-level is the main trend statistic).
- **Cluster/cell bootstrap CIs** (spec §12.3): resample **cells**, not rows.
- **Regressions** (spec §12.4): `error_abs ~ modularity + n_shared + gain_oracle_est`, etc.
- **Null comparison** (spec §12.5): `delta_error = error_real - error_null` by `mu_B`, `n_shared`.
- **Plots** (spec §14): main error vs modularity (abs/rel/norm_B; raw light + cell means + trend +
  n_shared color) for both estimators; gain-magnitude diagnostic; estimator-vs-target (y=x);
  real-vs-null delta panels; error-by-nominal-mu box/violin; realized-modularity coverage.
- **Report** (spec §13 + §19 template): classify as **Case A/B/C/D**, stating explicitly which
  occurred, using the actual statistics — never assert positive unless the raw data supports it.
  Phrase any positive as "2D structural entropy supported under tested conditions".

---

## Build wiring (`CMakeLists.txt` edits)

- Add `src/exp_a/entropy2d.cpp sbm_gen.cpp estimators.cpp` to the build. Simplest: compile them
  into `entropy_core` (they are free functions, not registry plugins) by appending to the
  `add_library(entropy_core ...)` list (lines 121-132). The CLI subcommand calls them directly, so
  no whole-archive trick is needed for them.
- Add `experiment_a_runner.cpp` to `entropy_core` (or link into the CLI target).
- Add the `util::xlog2x` helper to `numerics.hpp` (header-only; no build change).
- Add tests: `enable_testing()` + `add_subdirectory(tests)` guarded by an option; fetch Catch2 via
  FetchContent (already referenced in exp-1's tree). Mirror exp-1 `tests/CMakeLists.txt`.

---

## Verification (how to prove it works end-to-end)

1. **Build:** `cd graph-entropy-sandbox && cmake -B build -DCMAKE_BUILD_TYPE=Release -DEXP_A_TESTS=ON && cmake --build build -j`.
2. **Unit/golden tests:** `ctest --test-dir build --output-on-failure` — all 15 required + extra
   correctness + golden tests pass. Confirm test 5 (`H2<=H1`), test 6 (full-degree), test 12
   (min-H_P not max-Q) explicitly in output.
3. **Smoke run:** `./build/entropy-cli exp-a --config configs/exp_a_smoke.toml` (tiny grid, e.g.
   3 mu_B × 1 n_shared × 2 reps). Confirm `results/exp_a/<ts>/trials.csv` + `trials_null.csv` exist,
   every row from a real trial, header == spec §15, manifest has git SHA + seeds + accepted/rejected
   counts.
4. **Reproducibility:** re-run smoke with same seed ⇒ byte-identical `trials.csv` (test 8 at runner
   scale).
5. **Analysis dry-run:** `python analysis/exp_a/analyze_exp_a.py results/exp_a/<ts>/` on smoke data
   ⇒ produces `summary_cells.csv`, `plots/*.png`, `report.md` with a Case A/B/C/D classification
   (smoke result is not scientifically meaningful — labeled as such).
6. **Full run (documented, not executed here):**
   `./build/entropy-cli exp-a --config configs/exp_a_full.toml` then
   `python analysis/exp_a/analyze_exp_a.py results/exp_a/<latest>/`.

---

## Out of scope / deferred
- Executing the full 660+660-trial run and producing the final scientific verdict (deferred per
  decision 2; pipeline is ready and documented).
- Estimator C (degree-histogram summary, spec §9.4) — optional/secondary; stub the interface only.
- Modifying the existing nats-based `structural_entropy` plugin.
