# Pre-Merge Structural Information — Preliminary Experiments: Integration Design

> **Status:** authoritative integration design for the proposal-stage experiments (Groups 1–4)
> of `preliminary_exp_spec.md`. If this conflicts with prior conversation, this document wins.
> Where it is silent, `preliminary_exp_spec.md` (the experiment contract) and
> `pre_merge_SI_theory.md` (the theory + Appendix-B reference code) govern.
>
> **What this document is.** The experiments themselves — E1–E8, primitives P1–P12, pass/fail
> criteria, datasets, plots, results schema — are already fully specified in
> `preliminary_exp_spec.md` and are **not** re-derived here. This design covers only the
> **integration architecture**: how that spec is realized inside this repository under the
> chosen hybrid (C++ kernel + pybind11 + Python orchestration) approach, with extensibility as
> a first-class constraint.
>
> **Scientific bar (non-negotiable).** These are academic, proposal-stage, potentially
> publishable experiments. Every reported number must be re-derivable from a manifest. Pass/fail
> criteria and their falsifiers (from the spec) are implemented as real assertions that can fail.
> No result is tuned green; a failing or null experiment is reported as FAIL/NULL.

---

## 0. Decision record

- **Architecture (user-selected):** Hybrid — a C++ numeric **premerge kernel** exposed to Python
  via pybind11; Python orchestrates generators, estimators, baselines, ranking, and plots.
  Rationale: honors the sandbox's "C++ computes, Python plots" split (sandbox spec §1, §11),
  reuses the existing skeleton, keeps the entropy math in one tested place, and reuses the
  baseline authors' own (Python) code rather than reimplementing it — the verifiable choice for
  "we cite their implementation."
- **Run order (user-instructed):** Groups 1 → 2 → 3 first; **real data (Group 4) last.**
- **Groups 1–3 have zero external-repo dependencies.** They use only the C++ `agglo`/`exact`
  H² minimizers on small synthetic graphs. CoDeSEG/GESim/NHE/NetComp are Group-4 / E8 only.
- **Numerics convention (user-instructed):** **bits (log₂) everywhere — one global convention.**
  This overrides the sandbox's prior nats default (sandbox spec §6). Conversion is a Phase-A task:
  `util/numerics.hpp` gains base-2 entropy helpers (`xlog2x`, `safe_log2`); the existing
  `structural_entropy.cpp` plugin switches to them (value, `description`, `formula`/`unit`
  metadata → bits); sandbox spec §6 is updated to read "bits (log₂)"; the K_n unit test's expected
  value is rebased to `log₂`. No nats remain in the tree.
- **Node identity:** **global integer labels** (experiment spec §2.1 data contract). The merge
  requires anchors to align by shared ID across `G_A`, `G_B`, `G_M`. The kernel adds a thin
  label↔dense-index map; the core CSR `Graph` (dense `0..n-1`) is unchanged.
- **Memory correction:** the prior session memory referenced a plan
  `docs/superpowers/plans/2026-05-20-experiment-1-mi-overlap.md` that does **not** exist on disk.
  Only the skeleton plan `2026-05-15-graph-entropy-skeleton.md` exists. That memory is stale and
  is corrected as part of this work.

---

## 1. Current repository state (verified on disk)

- `graph-entropy-sandbox/` — C++ sandbox, **Phase-1 skeleton complete and building**
  (`build/entropy-cli` present). Core abstraction: `IEntropyAlgorithm::compute(Graph, Cache,
  Params) → scalar` (single graph, scalar out). Implemented: `structural_entropy` (1-D, nats),
  `edgelist` loader, algorithm/graph registries, `ExperimentRunner`, manifest/results/CSV/log.
- The skeleton's interface does **not** model pairs of graphs, partitions, merges, or message
  protocols. The premerge work therefore adds a **new module**, it does not bend the plugin
  interface.

---

## 2. Component architecture

```
graph-entropy-sandbox/
  src/premerge/              # NEW C++ kernel, namespace entropy::premerge (NOT IEntropyAlgorithm)
    labeling.{hpp,cpp}       #   global-label <-> dense-index map; build labeled Graph from (E,V)
    h1.{hpp,cpp}             #   P2  H1 (bits) + volume W
    partition_entropy.*      #   P3  H^P(graph, part); decomposition H^P = H1 - H(q) + S
    merge.*                  #   P4  simple union dedup; P5 overlaps o_i; merged degrees d^M
    h2_min.*                 #   P6  'agglo' (random-restart greedy) + 'exact' (brute/B&B)
    message.*                #   P10 build_message (M1-M4) + reconstruct (B.4 7-step)
    bias.*                   #   P11 sigma, seam_bound = 2*sigma*log2(W), renorm_cap = 1.06
  bindings/python/
    premerge_module.cpp      #   pybind11: thin (E,V,part) marshaling -> floats/dicts/tuples
  tests/premerge/            #   C++ unit + fixture tests (Catch2/GTest, matching sandbox choice)

experiments/                 # NEW Python package at repo root
  premerge_py/
    kernel.py                #   import shim around the pybind11 module (single import point)
    reference.py             #   Appendix-B pure-Python primitives — TEST ORACLE ONLY
    generators.py            #   P7 degree_preserving_rewire, P8 SBM/LFR (NetworkX) + re-snap
    estimators.py            #   P9 partition_standalone / _bridge / _oracle (Python orchestration)
    proxies.py               #   P12 proxy scores (wraps baselines; Group 4)
    results.py               #   §8.1 fixed-schema CSV/JSONL writer + manifest + seed derivation
    summary.py               #   §8.3 PASS/FAIL/NULL aggregate table
    baselines/               #   OPTIONAL lazy adapters (Group 4 only)
      codeseg.py gesim.py nhe.py netcomp.py
  configs/                   #   per-experiment TOML (params + seeds), mirrors sandbox config style
  e1_dichotomy.py            #   one runner per experiment ...
  e2_continuous_knob.py
  e3_reconstruction.py
  e4_irredundancy.py
  e5_bias_vs_sigma.py
  e6_standalone_vs_bridge.py
  ecert_certify_oracle.py
  e7_real_data.py            #   Group 4 (last)
  e8_proxy_ranking.py        #   Group 4 (last)
  tests/                     #   pytest: falsifiers as assertions + Appendix-B fixtures
```

### 2.1 Boundary contract (kernel ↔ Python)

The pybind11 boundary is deliberately thin and library-agnostic. **No C++ `Graph` object crosses
into Python.** Python passes the spec's data contract (edge list `E` = list of unordered int
pairs; node set `V` = int array; partition = `dict node_id -> module_id`); C++ builds its labeled
`Graph` internally and returns plain values:

```
kernel.h1(E, V)                              -> (H1: float, W: int)
kernel.h_partition(E, V, part)               -> float
kernel.merge(EA, VA, EB, VB)                 -> (EM, VM)
kernel.overlaps(EA, EB, S)                   -> dict{i: o_i}
kernel.h2_min(E, V, method, restarts, seed)  -> (H2: float, part: dict)
kernel.build_message(EB, VB, S, partB)       -> (M1, M2, M3, M4)
kernel.reconstruct(EA, VA, S, partA, message)-> float
kernel.bias_bounds(S, dM, W)                 -> (sigma, seam_bound, renorm_cap)
```

### 2.2 Why estimators (P9) are Python, not kernel

`partition_standalone/_bridge/_oracle` compose `h2_min` with a restricted local re-optimization
(bridge-aware = re-optimize only `S ∪ N_GM(S)`). They are orchestration, not hot numerics, and
will iterate during research. They live in Python and call the kernel for the entropy math. The
kernel exposes the one primitive they need beyond `h2_min`: a `local_reoptimize` move-loop with a
`movable` node set (the operational definition of bridge-aware, spec §2.4).

---

## 3. Extensibility (the user's keyword)

- **New entropy order / measure** → new file in `src/premerge/`, no change to existing kernel
  files; mirrors the sandbox's self-registration philosophy (here: free functions in a namespace,
  not the plugin registry, because these are not single-graph scalar algorithms).
- **New H² minimizer** (e.g. CoDeSEG) → add a `method` branch / a Python adapter implementing the
  `h2_min` signature `(E,V,...) -> (H2, part)`; estimators and experiments are unchanged.
- **New baseline proxy** → one adapter file in `experiments/premerge_py/baselines/` implementing a
  uniform `score(GA, GB, S) -> float`; E8 discovers it by name. Missing repo → `unavailable`,
  never silently faked.
- **New experiment** → one `eN_*.py` runner + one config; reuses `results.py` schema and
  `summary.py` aggregation. No edits to kernel or other runners.
- **New dataset** → one loader function returning `(GA, GB, S)`; the Pattern-A / Pattern-B recipes
  (spec §7.2) are the two supported shapes.

---

## 4. Reproducibility & results

Extends the sandbox reproducibility contract (sandbox spec §5) to the Python harness:

- Each runner writes a run directory `results/<experiment>/<utc_timestamp>/` containing:
  - `manifest.json`: git SHA + `git_dirty`, kernel build hash, config TOML verbatim +
    `config_sha256`, per-graph `content_hash`, base seed, and per-instance derived seeds
    (`splitmix64(base ^ hash(instance) ^ ...)`).
  - the results file in the **exact §8.1 schema** (one row per instance; always records
    `oracle_method` and `seed` — biases are only interpretable relative to the minimizer).
- **All randomness** flows through one seeded RNG (mirrors `entropy::util::Rng`); no bare
  `np.random`, no time-seeding.
- A top-level `summary.py` emits the §8.3 `experiment | claim | metric | threshold | observed |
  PASS/FAIL/NULL` table across all run directories. NULL results (esp. E8) are kept visible.

---

## 5. Testing strategy ("prove it really")

Three layers, all backed by the verified fixtures in `pre_merge_SI_theory.md` Appendix B:

1. **Kernel unit/fixture tests (C++, and again through the Python binding):**
   - 3-node path `E=[(0,1),(1,2)]`, partition `{0,1}|{2}`:
     `H^P=1.2924812, H¹=1.5, H(q)=0.81128, S=0.60381, benefit=0.20752` (spec §2.3 unit test).
   - Decomposition identity `H^P = H¹ − H(q) + S` to machine precision, as a runtime assertion
     and a test.
   - §B.4 reconstruction equals direct `H_partition` to `<1e-9` on the `|S|=3, 5` merges
     (`2.675732`, `2.783881`).
   - Volume-sum invariant `Σ_v d^M_v == 2|E_A ∪ E_B| == W` asserted inside `merge`/`reconstruct`.
2. **Cross-validation:** C++ kernel vs `reference.py` (Appendix-B Python) agree to `1e-9` on
   randomized SBM/LFR instances. This is the "two independent implementations agree" evidence,
   located in the test suite rather than in two live production paths.
3. **Falsifier / regression tests:** the five E4 witnesses as fixtures
   (`M3: 2.383605≠2.408767`, `M4-int: 2.187014≠2.574600`, `M4-vol: 2.529236≠2.542125`,
   `M1: 2.188297≠2.173108`, `M2: 2.125815≠2.049452`, to 1e-5); the E1/E3/E5/E6 pass criteria as
   assertions that can genuinely fail. The §7.4 cohesionless-module trap is explicitly tested
   (M4-vol must use cohesive, asymmetric modules `EB_w`).

---

## 6. Phasing (drives the implementation plan)

Each phase ends with a green, demonstrable checkpoint. Groups 1–3 require **no external repos**.

- **Phase A — C++ premerge kernel (+ global bits conversion).** First: convert the tree to bits —
  add `xlog2x`/`safe_log2` to `util/numerics.hpp`, switch `structural_entropy.cpp` and its unit
  test to bits, update sandbox spec §6. Then the kernel: `labeling`, `h1`, `partition_entropy`
  (+ identity), `merge` + `overlaps`, `h2_min` (`agglo` + `exact`), `message` (build +
  reconstruct), `bias` — all in bits. C++ tests: 3-node path, identity, reconstruction,
  volume-sum, E4 witnesses. *Exit:* all kernel fixtures green in C++; no nats remain.
- **Phase B — pybind11 module + cross-validation.** `premerge_module.cpp`, `kernel.py` shim,
  `reference.py` oracle, cross-validation test to 1e-9. *Exit:* Python can call every kernel
  function; kernel == reference on random instances.
- **Phase C — Python harness primitives.** `generators.py` (P7 rewire + re-snap, P8 SBM/LFR),
  `estimators.py` (P9 standalone/bridge/oracle via kernel + `local_reoptimize`), `results.py`
  (§8.1 schema + manifest + seeds), `summary.py`. *Exit:* a smoke instance flows end-to-end to a
  schema-valid results file.
- **Phase D — Group 1 (dichotomy).** E1 (binary witness, §3.5/B.5), E2 (continuous knob). *Exit:*
  E1 PASS (`|ΔH¹(B)−ΔH¹(B')|<1e-9` and `|H²(GM,B)−H²(GM,B')|>0.05`); E2 PASS (`Var(ΔH¹)` at noise
  floor, `|ρ|≥0.8`).
- **Phase E — Group 2 (protocol exact & tight).** E3 (reconstruction past toy size), E4 (five
  irredundancy witnesses). *Exit:* E3 `<1e-9`; E4 all five differ at fixture values.
- **Phase F — Group 3 (accuracy & privacy price).** E5 (bias vs σ), E6 (standalone/bridge/oracle
  ordering + seam-cut bias), E-cert (certified exact oracle on small instances). *Exit:* bound
  never violated; ordering holds; `gap = Ĥ²−H²_exact ≥ 0`.
- **Phase G — Group 4 (real data, LAST).** Baseline adapters (CoDeSEG/GESim/NHE/NetComp, lazy),
  dataset loaders (Pattern A/B, email-Eu-core first), E7 (replicate 1–3 on real merge +
  message-size-vs-|V_B| + modularity-margin), E8 (proxy ranking; null result reportable). *Exit:*
  Groups 1–3 criteria hold on real data; E8 ρ/τ/recall@k reported with honest PASS/NULL.

---

## 7. Risks / caveats carried from the theory (state at defense)

Inherited verbatim from spec §9 — these are reported, not engineered away:
1. Heuristic H² is an upper bound ⇒ E5/E6 biases are lower estimates; E-cert mitigates on small
   instances.
2. The clean `bias ≤ 2σ·log₂W` needs the modularity margin (δ_renorm=0); measure it on real data
   (E7 step 5), don't assume it.
3. Structural-information gain ≠ recommendation value; E8 proves only "cheap proxy tracks
   expensive proxy" — scope it explicitly.
4. Bridge-aware is a centralized experimental construct here; its distributed protocol is open.
5. Model scope: undirected/unweighted, connected, simple union; weighted/directed/disconnected
   are v2.

---

## 8. Out of scope (do not build for the proposal stage)

- Distributed/bridge-aware message protocol (theory open item §8).
- Formal privacy leakage bound (theory §8).
- Weighted/directed/disconnected merge generalizations.
- Heterogeneous / knowledge-graph alignment datasets (spec §7.2 stretch).
- Any reimplementation of CoDeSEG/GESim/NHE/NetComp in C++ — they are wrapped, not rebuilt.
