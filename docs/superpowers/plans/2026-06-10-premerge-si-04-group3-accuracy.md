# Pre-Merge SI — Plan 04: Group 3 — accuracy bound and the privacy price

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax. **Prerequisite:** Plan 01 (Foundation). Plan 03's `merge_build.py` is reused.

**Goal:** Show (E5) the standalone estimator's bias is bounded by `2σ·log₂W` and vanishes as σ→0; (E6) the ordering `H_standalone ≥ H_bridge ≥ H_oracle` and the seam-cut bias `= standalone − bridge`; and (E-cert) that certifying the exact oracle on small instances converts "realized bias (lower estimate)" into "true bias" with `gap = Ĥ²_heuristic − H²_exact ≥ 0`.

**Architecture:** Python runners over the Plan-01 kernel/estimators and Plan-03 `merge_build`. A σ-sweep raises the anchor count; each point computes standalone/bridge/oracle entropies and the bias bound. Results are written in the §8.1 schema with `within_bound` flags; plots overlay the `2σ·log₂W` bound line.

**Tech Stack:** Python, NetworkX, `premerge_kernel`, SciPy (Spearman), matplotlib. Bits everywhere.

**Spec refs:** `preliminary_exp_spec.md` §5 (E5, E6, E-cert); `pre_merge_SI_theory.md` §6, §4.2, §A.5.

---

## File Structure

```
experiments/
  e5_bias_vs_sigma.py        # NEW runner
  e6_standalone_vs_bridge.py # NEW runner
  ecert_certify_oracle.py    # NEW runner
  premerge_py/
    sigma_sweep.py           # NEW: build a family of merges with rising σ
  tests/
    test_e5.py test_e6.py test_ecert.py   # NEW (pass-criterion assertions)
```

---

### Task 1: σ-sweep builder

**Files:**
- Create: `experiments/premerge_py/sigma_sweep.py`, `experiments/tests/test_e5.py` (uses it)

- [ ] **Step 1: Failing test** (σ increases monotonically with anchor count). `experiments/tests/test_e5.py`:

```python
from premerge_py import sigma_sweep as ss

def test_sigma_increases_with_anchors():
    pts = ss.sweep(sizes_A=[6,6], sizes_B=[6,6], anchor_counts=[1,2,3,4], seed=1)
    sigmas = [p["sigma"] for p in pts]
    assert sigmas == sorted(sigmas)            # monotone non-decreasing
    assert all(0.0 <= s <= 1.0 for s in sigmas)
```

- [ ] **Step 2: Implement.** `experiments/premerge_py/sigma_sweep.py`:

```python
"""Build a family of merges with rising bridge density σ = vol_{G_M}(S)/W."""
from premerge_py import merge_build as mb, kernel as k

def _merged_degrees(EM, VM):
    d = {n: 0 for n in VM}
    for u, v in EM: d[u] += 1; d[v] += 1
    return d

def sweep(sizes_A, sizes_B, anchor_counts, seed, p_in=0.6, p_out=0.05):
    points = []
    for j, na in enumerate(anchor_counts):
        inst = mb.build_sbm_merge(sizes_A, sizes_B, n_anchors=na, seed=seed + j, p_in=p_in, p_out=p_out)
        EM, VM = k.merge(inst["EA"], inst["VA"], inst["EB"], inst["VB"])
        dM = _merged_degrees(EM, VM)
        W = sum(dM.values())
        volS = sum(dM[s] for s in inst["S"])
        sigma, seam, renorm = k.bias_bounds(inst["S"], dM, W)
        points.append({**inst, "EM": EM, "VM": VM, "dM": dM, "W": W, "volS": volS,
                       "sigma": sigma, "seam_bound": seam, "renorm_cap": renorm, "n_anchors": na})
    return points
```

- [ ] **Step 3: Run.** `cd experiments && python -m pytest tests/test_e5.py::test_sigma_increases_with_anchors -v` → passed.
- [ ] **Step 4: Commit.** `git commit -am "premerge_py: sigma-sweep merge builder"`

---

### Task 2: E5 — bias vs σ runner + pass test

**Files:**
- Create: `experiments/e5_bias_vs_sigma.py`
- Modify: `experiments/tests/test_e5.py`, `experiments/plots.py`

E5: per σ point compute `H_standalone`, oracle `Ĥ²`, `realized_bias = H_standalone − Ĥ²`, `seam_bound = 2σ·log₂W`. PASS if `realized_bias ≤ seam_bound + 1.06` on **every** instance and bias→0 as σ→0 (Spearman ρ(bias, σ) > 0). The oracle is a heuristic upper bound ⇒ `realized_bias` is a lower estimate; state it (spec §5 E5).

- [ ] **Step 1: Failing pass-criterion test.** Append to `experiments/tests/test_e5.py`:

```python
import e5_bias_vs_sigma as e5

def test_e5_bound_never_violated():
    res = e5.run(anchor_counts=[1,2,3,4,5], seed=1, restarts=300)
    assert all(r["within_bound"] for r in res["rows"])   # bias <= seam + 1.06 everywhere
    assert res["spearman_bias_sigma"] > 0                 # bias grows with σ
```

- [ ] **Step 2: Implement.** `experiments/e5_bias_vs_sigma.py`:

```python
"""E5 — bias vs bridge density σ. Spec §5 E5."""
import argparse
from scipy.stats import spearmanr
from premerge_py import sigma_sweep as ss, estimators as est, kernel as k, results as R

def run(anchor_counts=(1, 2, 3, 4, 5), seed=1, restarts=300, sizes_A=(6, 6), sizes_B=(6, 6)):
    pts = ss.sweep(list(sizes_A), list(sizes_B), list(anchor_counts), seed)
    rows = []
    for p in pts:
        p_stand = est.partition_standalone(p["EA"], p["VA"], p["EB"], p["VB"], p["S"], restarts, seed)
        h_stand = k.h_partition(p["EM"], p["VM"], p_stand)
        h_oracle, _ = est.partition_oracle(p["EM"], p["VM"], method="agglo", restarts=restarts * 2, seed=seed)
        bias = h_stand - h_oracle
        within = bias <= p["seam_bound"] + p["renorm_cap"] + 1e-9
        rows.append({"sigma": p["sigma"], "W": p["W"], "volS": p["volS"], "n_S": len(p["S"]),
                     "H_standalone": h_stand, "H2_oracle": h_oracle, "realized_bias": bias,
                     "seam_bound": p["seam_bound"], "renorm_cap": p["renorm_cap"], "within_bound": within})
    rho = spearmanr([r["sigma"] for r in rows], [r["realized_bias"] for r in rows]).correlation
    return {"rows": rows, "spearman_bias_sigma": float(rho) if rho == rho else 0.0}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--anchors", type=int, nargs="+", default=[1, 2, 3, 4, 5])
    ap.add_argument("--seed", type=int, default=1); ap.add_argument("--restarts", type=int, default=300)
    ap.add_argument("--out", default="results"); a = ap.parse_args()
    res = run(tuple(a.anchors), a.seed, a.restarts)
    ok = all(r["within_bound"] for r in res["rows"])
    status = "PASS" if (ok and res["spearman_bias_sigma"] > 0) else "FAIL"
    run = R.RunWriter("E5", base_seed=a.seed, out_root=a.out)
    for i, r in enumerate(res["rows"]):
        run.add_row({"instance_id": str(i), "n_S": r["n_S"], "W": r["W"], "sigma": r["sigma"],
                     "H_standalone": r["H_standalone"], "H2_oracle": r["H2_oracle"], "oracle_method": "agglo",
                     "realized_bias": r["realized_bias"], "seam_bound": r["seam_bound"],
                     "renorm_cap": r["renorm_cap"], "within_bound": r["within_bound"]})
    run.close(config={**vars(a), "spearman_bias_sigma": res["spearman_bias_sigma"], "status": status,
                      "note": "oracle is heuristic upper bound; realized_bias is a lower estimate"})
    import os
    from plots import e5_bias_scatter
    e5_bias_scatter(res["rows"], os.path.join(run.run_dir, "e5_bias.png"))
    print(f"E5 {status}: within_bound on all={ok}, ρ(bias,σ)={res['spearman_bias_sigma']:.3f} (>0)")

if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Plot helper.** Append to `experiments/plots.py`:

```python
def e5_bias_scatter(rows, out_path):
    import numpy as np
    s = [r["sigma"] for r in rows]; b = [r["realized_bias"] for r in rows]
    bound = [r["seam_bound"] for r in rows]
    order = np.argsort(s); s = np.array(s)[order]; b = np.array(b)[order]; bound = np.array(bound)[order]
    fig, ax = plt.subplots(figsize=(5, 4))
    ax.scatter(s, b, label="realized bias (lower est.)")
    ax.plot(s, bound, "r--", label="2σ·log₂W bound")
    ax.set_xlabel("σ = vol(S)/W"); ax.set_ylabel("bits"); ax.legend()
    ax.set_title("E5: standalone bias vs bridge density"); fig.tight_layout()
    fig.savefig(out_path, dpi=150); plt.close(fig)
```

- [ ] **Step 4: Run.**
Run: `cd experiments && python -m pytest tests/test_e5.py -v && python e5_bias_vs_sigma.py`
Expected: `E5 PASS: within_bound on all=True, ρ(bias,σ)=0.xx (>0)`. FALSIFIER (spec §5 E5): a violation ⇒ oracle returned above standalone (bug evaluating the same G_M) or σ/W inconsistent — recompute `vol(S)` on `G_M` via `d^M` (done in `sigma_sweep`).

- [ ] **Step 5: Commit.** `git commit -am "E5: bias vs σ runner + bound-overlay scatter (PASS asserted)"`

---

### Task 3: E6 — standalone vs bridge vs oracle runner + pass test

**Files:**
- Create: `experiments/e6_standalone_vs_bridge.py`, `experiments/tests/test_e6.py`
- Modify: `experiments/plots.py`

E6 adds the bridge-aware partition on the same σ-sweep. Reports `bias_standalone`, `bias_bridge`, `seam_cut_bias = H_standalone − H_bridge`. PASS if ordering `H_stand ≥ H_bridge ≥ Ĥ²` holds element-wise (tolerance −1e-9) and `seam_cut_bias → 0` as σ → 0 (spec §5 E6).

- [ ] **Step 1: Failing pass-criterion test.** `experiments/tests/test_e6.py`:

```python
import e6_standalone_vs_bridge as e6

def test_e6_ordering_holds():
    res = e6.run(anchor_counts=[1,2,3,4], seed=1, restarts=300)
    for r in res["rows"]:
        assert r["H_standalone"] >= r["H_bridge"] - 1e-9
        assert r["H_bridge"]     >= r["H2_oracle"] - 1e-9
    # seam-cut bias smallest at smallest σ
    by_sigma = sorted(res["rows"], key=lambda r: r["sigma"])
    assert by_sigma[0]["seam_cut_bias"] <= by_sigma[-1]["seam_cut_bias"] + 1e-9
```

- [ ] **Step 2: Implement.** `experiments/e6_standalone_vs_bridge.py`:

```python
"""E6 — standalone vs bridge-aware vs oracle (privacy price in bits). Spec §5 E6."""
import argparse
from premerge_py import sigma_sweep as ss, estimators as est, kernel as k, results as R

def run(anchor_counts=(1, 2, 3, 4), seed=1, restarts=300, sizes_A=(6, 6), sizes_B=(6, 6)):
    pts = ss.sweep(list(sizes_A), list(sizes_B), list(anchor_counts), seed)
    rows = []
    for p in pts:
        p_stand = est.partition_standalone(p["EA"], p["VA"], p["EB"], p["VB"], p["S"], restarts, seed)
        p_bridge = est.partition_bridge(p["EA"], p["VA"], p["EB"], p["VB"], p["S"], restarts, seed)
        h_stand = k.h_partition(p["EM"], p["VM"], p_stand)
        h_bridge = k.h_partition(p["EM"], p["VM"], p_bridge)
        h_oracle, _ = est.partition_oracle(p["EM"], p["VM"], method="agglo", restarts=restarts * 2, seed=seed)
        rows.append({"sigma": p["sigma"], "W": p["W"], "n_S": len(p["S"]),
                     "H_standalone": h_stand, "H_bridge": h_bridge, "H2_oracle": h_oracle,
                     "bias_standalone": h_stand - h_oracle, "bias_bridge": h_bridge - h_oracle,
                     "seam_cut_bias": h_stand - h_bridge})
    return {"rows": rows}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--anchors", type=int, nargs="+", default=[1, 2, 3, 4])
    ap.add_argument("--seed", type=int, default=1); ap.add_argument("--restarts", type=int, default=300)
    ap.add_argument("--out", default="results"); a = ap.parse_args()
    res = run(tuple(a.anchors), a.seed, a.restarts)
    ordering_ok = all(r["H_standalone"] >= r["H_bridge"] - 1e-9 and r["H_bridge"] >= r["H2_oracle"] - 1e-9
                      for r in res["rows"])
    status = "PASS" if ordering_ok else "FAIL"
    run = R.RunWriter("E6", base_seed=a.seed, out_root=a.out)
    for i, r in enumerate(res["rows"]):
        run.add_row({"instance_id": str(i), "n_S": r["n_S"], "W": r["W"], "sigma": r["sigma"],
                     "H_standalone": r["H_standalone"], "H_bridge": r["H_bridge"], "H2_oracle": r["H2_oracle"],
                     "realized_bias": r["bias_standalone"]})
    run.close(config={**vars(a), "ordering_ok": ordering_ok, "status": status})
    import os
    from plots import e6_curves
    e6_curves(res["rows"], os.path.join(run.run_dir, "e6_curves.png"))
    print(f"E6 {status}: ordering H_stand≥H_bridge≥Ĥ² holds on all={ordering_ok}")

if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Plot helper.** Append to `experiments/plots.py`:

```python
def e6_curves(rows, out_path):
    import numpy as np
    rows = sorted(rows, key=lambda r: r["sigma"]); s = [r["sigma"] for r in rows]
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(9, 4))
    ax1.plot(s, [r["H_standalone"] for r in rows], "o-", label="standalone")
    ax1.plot(s, [r["H_bridge"] for r in rows], "s-", label="bridge-aware")
    ax1.plot(s, [r["H2_oracle"] for r in rows], "^-", label="oracle Ĥ²")
    ax1.set_xlabel("σ"); ax1.set_ylabel("H^P (bits)"); ax1.legend(); ax1.set_title("E6: three estimators")
    ax2.plot(s, [r["seam_cut_bias"] for r in rows], "d-")
    ax2.set_xlabel("σ"); ax2.set_ylabel("seam-cut bias (bits)"); ax2.set_title("E6: privacy price")
    fig.tight_layout(); fig.savefig(out_path, dpi=150); plt.close(fig)
```

- [ ] **Step 4: Run.**
Run: `cd experiments && python -m pytest tests/test_e6.py -v && python e6_standalone_vs_bridge.py`
Expected: `E6 PASS: ordering ... holds on all=True`. FALSIFIER (spec §5 E6): `H_bridge > H_standalone` ⇒ bridge re-opt not seeded from standalone / accepted a non-improving move (see `estimators.partition_bridge`); `H_bridge < Ĥ²` ⇒ oracle weaker than restricted re-opt — raise oracle restarts.

- [ ] **Step 5: Commit.** `git commit -am "E6: standalone/bridge/oracle ordering + seam-cut bias runner (PASS asserted)"`

---

### Task 4: E-cert — certify the oracle on small instances + pass test

**Files:**
- Create: `experiments/ecert_certify_oracle.py`, `experiments/tests/test_ecert.py`

E-cert recomputes the oracle on the smallest E5/E6 instances with the **exact** minimizer (`|V_M| ≤ 12`), giving `gap = Ĥ²_heuristic − H²_exact ≥ 0` and corrected biases (spec §5 E-cert).

- [ ] **Step 1: Failing pass-criterion test.** `experiments/tests/test_ecert.py`:

```python
import ecert_certify_oracle as ec

def test_ecert_gap_nonnegative():
    res = ec.run(seed=1, max_nodes=12)
    assert res["n_instances"] >= 1
    for r in res["rows"]:
        assert r["gap"] >= -1e-9               # heuristic never below exact
        assert r["bias_exact"] >= -1e-9        # standalone never below true oracle
```

- [ ] **Step 2: Implement.** `experiments/ecert_certify_oracle.py`:

```python
"""E-cert — certify the oracle on small instances (de-risks E5/E6). Spec §5 E-cert."""
import argparse
from premerge_py import sigma_sweep as ss, estimators as est, kernel as k, results as R

def run(seed=1, max_nodes=12, restarts=300):
    # use small sizes so |V_M| <= max_nodes
    pts = ss.sweep([3, 3], [3, 3], anchor_counts=[1, 2], seed=seed)
    rows = []
    for p in pts:
        if len(p["VM"]) > max_nodes:
            continue
        p_stand = est.partition_standalone(p["EA"], p["VA"], p["EB"], p["VB"], p["S"], restarts, seed)
        h_stand = k.h_partition(p["EM"], p["VM"], p_stand)
        h_heur, _ = est.partition_oracle(p["EM"], p["VM"], method="agglo", restarts=restarts, seed=seed)
        h_exact, _ = k.h2_min(p["EM"], p["VM"], method="exact", max_nodes=max_nodes)
        rows.append({"n_VM": len(p["VM"]), "H2_heuristic": h_heur, "H2_exact": h_exact,
                     "gap": h_heur - h_exact, "bias_heuristic": h_stand - h_heur,
                     "bias_exact": h_stand - h_exact})
    return {"rows": rows, "n_instances": len(rows)}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=1); ap.add_argument("--max_nodes", type=int, default=12)
    ap.add_argument("--out", default="results"); a = ap.parse_args()
    res = run(a.seed, a.max_nodes)
    ok = all(r["gap"] >= -1e-9 for r in res["rows"]) and res["n_instances"] >= 1
    status = "PASS" if ok else "FAIL"
    run = R.RunWriter("Ecert", base_seed=a.seed, out_root=a.out)
    for i, r in enumerate(res["rows"]):
        run.add_row({"instance_id": str(i), "H2_oracle": r["H2_exact"], "oracle_method": "exact",
                     "realized_bias": r["bias_exact"]})
    run.close(config={**vars(a), "status": status,
                      "note": "gap = heuristic - exact >= 0 converts lower-estimate bias to true bias"})
    for r in res["rows"]:
        print(f"  |V_M|={r['n_VM']:2d}  Ĥ²={r['H2_heuristic']:.6f}  H²_exact={r['H2_exact']:.6f}  gap={r['gap']:.2e}")
    print(f"E-cert {status}: {res['n_instances']} certified instances, all gap≥0={ok}")

if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Run.**
Run: `cd experiments && python -m pytest tests/test_ecert.py -v && python ecert_certify_oracle.py`
Expected: `E-cert PASS: N certified instances, all gap≥0=True`. FALSIFIER (spec §5 E-cert): `Ĥ²_heuristic < H²_exact` ⇒ exact search incomplete (it must enumerate all set partitions, including singletons and all-in-one) — verify against the Plan-01 C++ `test_h2_min.cpp` exact tests.

- [ ] **Step 4: Commit.** `git commit -am "E-cert: certified exact oracle on small instances (gap≥0 asserted)"`

---

### Task 5: Group-3 summary table

**Files:**
- Create: `experiments/run_group3_summary.py`

- [ ] **Step 1: Aggregate PASS/FAIL/NULL across E5/E6/E-cert into the §8.3 table.** `experiments/run_group3_summary.py`:

```python
"""Run E5, E6, E-cert and emit the §8.3 PASS/FAIL summary table."""
import e5_bias_vs_sigma as e5, e6_standalone_vs_bridge as e6, ecert_certify_oracle as ec
from premerge_py import summary

def main():
    r5 = e5.run(); r6 = e6.run(); rc = ec.run()
    rows = [
        {"experiment": "E5", "claim": "bias ≤ 2σ·log₂W + 1.06", "metric": "within_bound (all)",
         "threshold": "all True", "observed": all(r["within_bound"] for r in r5["rows"]),
         "status": "PASS" if all(r["within_bound"] for r in r5["rows"]) else "FAIL"},
        {"experiment": "E6", "claim": "H_stand ≥ H_bridge ≥ Ĥ²", "metric": "ordering (all)",
         "threshold": "all True",
         "observed": all(r["H_standalone"] >= r["H_bridge"] - 1e-9 and r["H_bridge"] >= r["H2_oracle"] - 1e-9 for r in r6["rows"]),
         "status": "PASS" if all(r["H_standalone"] >= r["H_bridge"] - 1e-9 and r["H_bridge"] >= r["H2_oracle"] - 1e-9 for r in r6["rows"]) else "FAIL"},
        {"experiment": "E-cert", "claim": "gap = Ĥ² − H²_exact ≥ 0", "metric": "min gap",
         "threshold": "≥ 0", "observed": min(r["gap"] for r in rc["rows"]),
         "status": "PASS" if all(r["gap"] >= -1e-9 for r in rc["rows"]) else "FAIL"},
    ]
    out = summary.write_summary(rows, "results/group3_summary.csv")
    print("wrote", out)
    for r in rows: print(f"  {r['experiment']:7s} {r['status']}  ({r['claim']})")

if __name__ == "__main__":
    main()
```

- [ ] **Step 2: Run.**
Run: `cd experiments && python run_group3_summary.py && cat results/group3_summary.csv`
Expected: three rows, each PASS (or an honest FAIL/NULL if a claim does not hold on the data).

- [ ] **Step 3: Commit.** `git commit -am "Group 3: PASS/FAIL summary table (E5/E6/E-cert)"`

---

## Self-review notes (coverage vs spec §5)

- E5 σ-sweep, realized_bias vs seam_bound, monotonicity, "oracle is heuristic upper bound" note → Tasks 1–2.
- E6 bridge-aware partition, ordering, seam-cut bias, FALSIFIER conditions → Task 3.
- E-cert exact oracle, gap ≥ 0, corrected biases → Task 4.
- §8.3 summary table (PASS/FAIL/NULL, kept visible) → Task 5.
- All pass criteria are real assertions in `test_e5/e6/ecert.py`; the heuristic-vs-exact caveat (spec §9.1) is recorded in each manifest `note`.
- vol(S) computed on G_M via d^M (sigma_sweep) — matches the spec §5 E5 falsifier guard.
```

> **Next (separate, LAST):** Group 4 — real data (E7, E8) and the CoDeSEG/GESim/NHE/NetComp baseline adapters — will be its own plan (`...-05-group4-real-data.md`) per the instruction to leave real data for the end. It depends on Plans 01–04 plus the external repos confirmed reachable on 2026-06-10.
