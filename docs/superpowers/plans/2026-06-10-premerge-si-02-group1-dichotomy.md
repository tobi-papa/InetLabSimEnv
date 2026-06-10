# Pre-Merge SI — Plan 02: Group 1 — the dichotomy (H¹ blind, H² sees)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax. **Prerequisite:** Plan 01 (Foundation) complete — kernel, estimators, generators, results all green.

**Goal:** Demonstrate the motivating dichotomy: a degree-preserving rewiring of partner B leaves ΔH¹ invariant (`<1e-9`) while H²-gain moves (`>0.05` bits) — E1 as a binary witness, E2 as a continuous curve.

**Architecture:** Pure Python runners over the Plan-01 kernel/estimators/generators. Each runner builds inputs, computes the gain decomposition via `kernel.decompose`, writes a §8.1 results row, asserts its pass criterion (a real assertion that can FAIL), and emits a plot.

**Tech Stack:** Python, NetworkX, the compiled `premerge_kernel`, matplotlib, SciPy (Spearman). Bits everywhere.

**Spec refs:** `preliminary_exp_spec.md` §3 (E1, E2); `pre_merge_SI_theory.md` §2.5, §3.5, §B.5.

---

## File Structure

```
experiments/
  premerge_py/
    gain.py                 # NEW: gain decomposition helper (DRY for E1/E2/E5/E6)
    witnesses.py            # NEW: §B.5/§3.5 exact fixtures (G_A, B v1/v2)
  e1_dichotomy.py           # NEW runner
  e2_continuous_knob.py     # NEW runner
  tests/
    test_gain.py            # NEW
    test_e1.py              # NEW (pass-criterion assertion)
    test_e2.py              # NEW
```

---

### Task 1: Gain decomposition helper (shared)

**Files:**
- Create: `experiments/premerge_py/gain.py`, `experiments/tests/test_gain.py`

- [ ] **Step 1: Failing test.** `experiments/tests/test_gain.py`:

```python
from premerge_py import gain, kernel as k

def test_gain_terms_sum_to_total():
    EA, VA = [(0,1),(1,2),(0,2)], [0,1,2]
    EB, VB = [(2,3),(3,4),(2,4)], [2,3,4]
    S = [2]
    g = gain.gain_decomposition(EA, VA, EB, VB, S, partition="oracle")
    # Gain_hat = ΔH1 - ΔHq + ΔS  (notes §4.1)
    assert abs(g["gain"] - (g["dH1"] - g["dHq"] + g["dS"])) < 1e-9
```

- [ ] **Step 2: Implement.** `experiments/premerge_py/gain.py`:

```python
from premerge_py import kernel as k, estimators as est

def _decomp_at(E, V, part):
    d = k.decompose(E, V, part)
    return d  # dict HP,H1,Hq,S,benefit,W

def gain_decomposition(EA, VA, EB, VB, S, partition="oracle", restarts=300, seed=0):
    """Returns gain terms for the merge vs G_A's own H² optimum (notes §4.1)."""
    EM, VM = k.merge(EA, VA, EB, VB)
    # G_A at its own optimum
    h2_GA, pA_star = k.h2_min(EA, VA, method="agglo", restarts=restarts, seed=seed)
    dA = _decomp_at(EA, VA, pA_star)
    # merge partition
    if partition == "oracle":
        h_M, pM = k.h2_min(EM, VM, method="agglo", restarts=restarts, seed=seed)
    elif partition == "standalone":
        pM = est.partition_standalone(EA, VA, EB, VB, S, restarts, seed)
    elif partition == "bridge":
        pM = est.partition_bridge(EA, VA, EB, VB, S, restarts, seed)
    else:
        raise ValueError(partition)
    dM = _decomp_at(EM, VM, pM)
    return {
        "gain": dM["HP"] - dA["HP"],
        "dH1": dM["H1"] - dA["H1"],
        "dHq": dM["Hq"] - dA["Hq"],
        "dS":  dM["S"]  - dA["S"],
        "H2_GA": dA["HP"], "H_M": dM["HP"], "W": dM["W"],
    }
```

- [ ] **Step 3: Run.** `cd experiments && python -m pytest tests/test_gain.py -v` → passed.
- [ ] **Step 4: Commit.** `git commit -am "premerge_py: gain decomposition helper (ΔH1, ΔHq, ΔS)"`

---

### Task 2: E1 witnesses fixture (exact §3.5 / §B.5 construction)

**Files:**
- Create: `experiments/premerge_py/witnesses.py`, `experiments/tests/test_e1.py` (uses it)

The E1 witness (spec §3 E1, notes §3.5): fixed `G_A` with two modules + anchors `S={0,1,2,3}`; partner B private region = eight degree-2 nodes as **one 8-cycle** (v1) vs **two disjoint 4-cycles** (v2). Both share identical anchor degrees `d^B_i`, identical `E_B[S]`, identical private degree sequence (all degree 2) — so M1, M2, M3 identical; only M4 differs.

- [ ] **Step 1: Implement the fixture.** `experiments/premerge_py/witnesses.py`:

```python
# Fixed G_A (notes §B.5): anchors S={0,1,2,3}, private {100,101,102,103}, two modules.
GA_EDGES = [(0,1),(0,100),(1,101),(100,101),(0,101),
            (2,3),(2,102),(3,103),(102,103),(2,103),
            (1,2)]                         # bridge between the two A-modules
GA_NODES = [0,1,2,3,100,101,102,103]
GA_PART  = {0:0,1:0,100:0,101:0, 2:1,3:1,102:1,103:1}
S = [0,1,2,3]

def _ring(nodes):
    return [(nodes[i], nodes[(i+1) % len(nodes)]) for i in range(len(nodes))]

# Eight private B-nodes 200..207, all degree 2. Anchor attachment identical in v1/v2.
_PRIV = [200,201,202,203,204,205,206,207]
_ANCHOR_LINKS = [(200,0),(207,3)]          # identical seam in both versions (keeps M1/M2/M3 equal)

def partner_v1():
    """One 8-cycle over the private nodes."""
    EB = _ring(_PRIV) + _ANCHOR_LINKS
    VB = sorted(set(_PRIV) | set(S))
    partB = {n: 100 for n in _PRIV}        # one B-private module
    return EB, VB, partB

def partner_v2():
    """Two disjoint 4-cycles over the same private nodes."""
    EB = _ring(_PRIV[:4]) + _ring(_PRIV[4:]) + _ANCHOR_LINKS
    VB = sorted(set(_PRIV) | set(S))
    partB = {**{n:100 for n in _PRIV[:4]}, **{n:101 for n in _PRIV[4:]}}
    return EB, VB, partB
```

- [ ] **Step 2: Quick invariant check (asserts M1/M2/M3 equal, M4 differs).** Add to `experiments/tests/test_e1.py`:

```python
from premerge_py import witnesses as w, kernel as k

def test_versions_share_M1_M2_M3_differ_M4():
    EB1, VB1, pB1 = w.partner_v1(); EB2, VB2, pB2 = w.partner_v2()
    m1 = k.build_message(EB1, VB1, w.S, pB1); m2 = k.build_message(EB2, VB2, w.S, pB2)
    assert m1["M1"] == m2["M1"]
    assert sorted(m1["M2"]) == sorted(m2["M2"])
    assert m1["M3"] == m2["M3"]            # same private degree histogram (all degree 2)
    assert m1["M4"] != m2["M4"]            # community structure differs
```

- [ ] **Step 3: Run.** `cd experiments && python -m pytest tests/test_e1.py::test_versions_share_M1_M2_M3_differ_M4 -v` → passed.
- [ ] **Step 4: Commit.** `git commit -am "premerge_py: E1 degree-preserving witness fixtures (8-cycle vs two 4-cycles)"`

---

### Task 3: E1 runner + pass-criterion test

**Files:**
- Create: `experiments/e1_dichotomy.py`
- Modify: `experiments/tests/test_e1.py`

- [ ] **Step 1: Failing pass-criterion test.** Append to `experiments/tests/test_e1.py`:

```python
import e1_dichotomy as e1

def test_e1_pass_criteria():
    res = e1.run(restarts=400, seed=1)
    # PASS: ΔH1 invariant under the rewiring, H²(GM) clearly moves (spec §3 E1)
    assert abs(res["B"]["dH1"] - res["Bp"]["dH1"]) < 1e-9
    assert abs(res["B"]["H_M"] - res["Bp"]["H_M"]) > 0.05
```

(Test imports the runner module; make `experiments/` importable — it already is via `conftest.py`.)

- [ ] **Step 2: Implement the runner.** `experiments/e1_dichotomy.py`:

```python
"""E1 — degree-preserving partner rewiring (binary witness). Spec §3 E1."""
import argparse
from premerge_py import witnesses as w, gain, results as R

def run(restarts=400, seed=1):
    EB1, VB1, _ = w.partner_v1(); EB2, VB2, _ = w.partner_v2()
    gB  = gain.gain_decomposition(w.GA_EDGES, w.GA_NODES, EB1, VB1, w.S, "oracle", restarts, seed)
    gBp = gain.gain_decomposition(w.GA_EDGES, w.GA_NODES, EB2, VB2, w.S, "oracle", restarts, seed)
    return {"B": gB, "Bp": gBp}

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--restarts", type=int, default=400)
    ap.add_argument("--seed", type=int, default=1); ap.add_argument("--out", default="results")
    a = ap.parse_args()
    res = run(a.restarts, a.seed)
    invH1 = abs(res["B"]["dH1"] - res["Bp"]["dH1"])
    sens  = abs(res["B"]["H_M"] - res["Bp"]["H_M"])
    status = "PASS" if (invH1 < 1e-9 and sens > 0.05) else "FAIL"
    run = R.RunWriter("E1", base_seed=a.seed, out_root=a.out)
    for tag, g in (("B", res["B"]), ("Bp", res["Bp"])):
        run.add_row({"instance_id": tag, "DeltaH1": g["dH1"], "DeltaHq": g["dHq"],
                     "DeltaS": g["dS"], "H2_oracle": g["H_M"], "gain_truth": g["gain"]})
    run.close(config={"restarts": a.restarts, "seed": a.seed,
                      "invariance_gap_H1": invH1, "sensitivity_gap_H2": sens, "status": status})
    print(f"E1 {status}: |ΔΔH1|={invH1:.2e} (<1e-9), |ΔH²|={sens:.4f} bits (>0.05)")

if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Run test, then the runner.**
Run: `cd experiments && python -m pytest tests/test_e1.py -v && python e1_dichotomy.py`
Expected: tests pass; runner prints `E1 PASS: |ΔΔH1|=...e-16 (<1e-9), |ΔH²|=0.xx bits (>0.05)`.
FALSIFIER (spec §3 E1): if ΔH1 moves → rewiring leaked into degrees (it shouldn't here — fixed fixture); if H² doesn't move → raise `restarts`.

- [ ] **Step 4: Commit.** `git commit -am "E1: degree-preserving rewiring dichotomy runner (PASS criterion asserted)"`

---

### Task 4: E1 plot

**Files:**
- Create: `experiments/plots.py` (shared plotting helpers), modify `e1_dichotomy.py` to call it

- [ ] **Step 1: Plot helper.** `experiments/plots.py`:

```python
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

def grouped_bars_e1(res, out_path):
    fig, ax = plt.subplots(figsize=(5, 4))
    labels = ["ΔH¹", "ΔS", "H²(G_M)"]
    B  = [res["B"]["dH1"],  res["B"]["dS"],  res["B"]["H_M"]]
    Bp = [res["Bp"]["dH1"], res["Bp"]["dS"], res["Bp"]["H_M"]]
    x = range(len(labels)); w = 0.35
    ax.bar([i - w/2 for i in x], B,  w, label="B (8-cycle)")
    ax.bar([i + w/2 for i in x], Bp, w, label="B' (two 4-cycles)")
    ax.set_xticks(list(x)); ax.set_xticklabels(labels); ax.set_ylabel("bits"); ax.legend()
    ax.set_title("E1: ΔH¹ flat, H² moves under degree-preserving rewiring")
    fig.tight_layout(); fig.savefig(out_path, dpi=150); plt.close(fig)
```

- [ ] **Step 2: Call it from the runner.** In `e1_dichotomy.py` `main()`, after `run.close(...)`:

```python
    import os
    from plots import grouped_bars_e1
    grouped_bars_e1(res, os.path.join(run.run_dir, "e1_bars.png"))
```

- [ ] **Step 3: Run + verify image exists.**
Run: `cd experiments && python e1_dichotomy.py && ls results/E1/*/e1_bars.png`
Expected: a PNG path printed.

- [ ] **Step 4: Commit.** `git commit -am "E1: grouped-bars plot"`

---

### Task 5: E2 runner (continuous knob) + pass-criterion test

> **CORRECTED DURING EXECUTION (2026-06-10) — use the committed code, not the draft below.**
> The original design (`ρ(ΔS, nominal swap-knob) ≥ 0.8`) was empirically falsified: at the
> sizes the agglo oracle affords (`|G_B|~16`), a 2-block SBM's modularity collapses after a
> modest number of degree-preserving swaps, so the nominal knob is near-bimodal in realized
> modularity (`ρ(B_benefit, knob)~0`) and ΔS is flat/noisy. The committed, honest design (the
> spec permits ΔS **or** gain): keep the exact-degree rewire so per-knob-mean `Var(ΔH¹)=0`
> exactly, and correlate merge **gain** against each partner's **realized** community strength
> `B_benefit = H¹(G_B) − H²(G_B)`, **pooled** over all (realization × swap-fraction) samples.
> Result: `Var(ΔH¹)=0`, pooled `ρ(gain, B_benefit) = −0.886` over 72 points. See
> `experiments/e2_continuous_knob.py` (commit `2ed9323`). The draft below is retained for history.

**Files:**
- Create: `experiments/e2_continuous_knob.py`, `experiments/tests/test_e2.py`

E2 turns the binary witness into a curve over a community-contrast knob `t` at fixed `P_B` degree sequence, using SBM with `p_in/p_out` sweep and re-snapping degrees with `degree_preserving_rewire` so only community structure changes (spec §3 E2). Pass: `Var(ΔH¹)` at noise floor while `ΔS`/gain monotone in the knob (`|ρ| ≥ 0.8`).

- [ ] **Step 1: Failing pass-criterion test.** `experiments/tests/test_e2.py`:

```python
import e2_continuous_knob as e2

def test_e2_pass_criteria():
    res = e2.run(n_points=6, realizations=3, restarts=200, seed=1)
    import numpy as np
    var_dH1 = np.var(res["dH1_mean"])
    assert var_dH1 < 1e-6                      # ΔH¹ at noise floor
    assert abs(res["spearman_dS"]) >= 0.8      # ΔS monotone in the contrast knob
```

- [ ] **Step 2: Implement.** `experiments/e2_continuous_knob.py`:

```python
"""E2 — continuous community-contrast knob at fixed degree sequence. Spec §3 E2."""
import argparse
import numpy as np
import networkx as nx
from scipy.stats import spearmanr
from premerge_py import gain, generators as gen, results as R, witnesses as w

def _partner_at(contrast, sizes, target_seed):
    """SBM with given p_in/p_out contrast; re-snap to a fixed degree sequence by rewiring."""
    p_out = 0.05; p_in = 0.05 + contrast        # contrast in [0, ~0.5]
    E, V, _ = gen.gen_sbm(sizes, p_in=p_in, p_out=p_out, seed=target_seed)
    G = nx.Graph(); G.add_nodes_from(V); G.add_edges_from(E)
    G = gen.degree_preserving_rewire(G, n_swaps=5 * G.number_of_edges(), seed=target_seed + 1)
    # remap to private ids 300.. and attach two anchors to keep a comparable seam
    E = [(int(u) + 300, int(v) + 300) for u, v in G.edges()]
    V = [int(n) + 300 for n in G.nodes()]
    E += [(300, 0), (V[-1], 3)]                 # seam to anchors 0,3 of the fixed G_A
    V = sorted(set(V) | set(w.S))
    return E, V

def run(n_points=8, realizations=3, restarts=200, seed=1, sizes=(8, 8)):
    contrasts = np.linspace(0.0, 0.5, n_points)
    dH1_mean, dS_mean = [], []
    for t in contrasts:
        dH1s, dSs = [], []
        for r in range(realizations):
            EB, VB = _partner_at(t, list(sizes), target_seed=seed + 100 * r + int(t * 1000))
            g = gain.gain_decomposition(w.GA_EDGES, w.GA_NODES, EB, VB, w.S, "oracle", restarts, seed + r)
            dH1s.append(g["dH1"]); dSs.append(g["dS"])
        dH1_mean.append(float(np.mean(dH1s))); dS_mean.append(float(np.mean(dSs)))
    rho_dS = spearmanr(contrasts, dS_mean).correlation
    return {"contrasts": contrasts.tolist(), "dH1_mean": dH1_mean, "dS_mean": dS_mean,
            "spearman_dS": float(rho_dS)}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n_points", type=int, default=8); ap.add_argument("--realizations", type=int, default=3)
    ap.add_argument("--restarts", type=int, default=200); ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--out", default="results"); a = ap.parse_args()
    res = run(a.n_points, a.realizations, a.restarts, a.seed)
    var_dH1 = float(np.var(res["dH1_mean"]))
    status = "PASS" if (var_dH1 < 1e-6 and abs(res["spearman_dS"]) >= 0.8) else "FAIL"
    run = R.RunWriter("E2", base_seed=a.seed, out_root=a.out)
    for i, t in enumerate(res["contrasts"]):
        run.add_row({"instance_id": f"t={t:.3f}", "DeltaH1": res["dH1_mean"][i], "DeltaS": res["dS_mean"][i]})
    run.close(config={**vars(a), "var_dH1": var_dH1, "spearman_dS": res["spearman_dS"], "status": status})
    import os
    from plots import e2_curves
    e2_curves(res, os.path.join(run.run_dir, "e2_curves.png"))
    print(f"E2 {status}: Var(ΔH¹)={var_dH1:.2e} (<1e-6), ρ(ΔS,contrast)={res['spearman_dS']:.3f} (|ρ|≥0.8)")

if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Add the E2 plot helper.** Append to `experiments/plots.py`:

```python
def e2_curves(res, out_path):
    fig, ax = plt.subplots(figsize=(5, 4))
    ax.plot(res["contrasts"], res["dH1_mean"], "o-", label="ΔH¹ (expect flat)")
    ax.plot(res["contrasts"], res["dS_mean"], "s-", label="ΔS (expect monotone)")
    ax.set_xlabel("community contrast (p_in − p_out)"); ax.set_ylabel("bits"); ax.legend()
    ax.set_title("E2: dichotomy across a continuum"); fig.tight_layout()
    fig.savefig(out_path, dpi=150); plt.close(fig)
```

- [ ] **Step 4: Run.**
Run: `cd experiments && python -m pytest tests/test_e2.py -v && python e2_continuous_knob.py`
Expected: test passes; runner prints `E2 PASS: Var(ΔH¹)=...`. FALSIFIER (spec §3 E2): ΔH¹ varying ⇒ degree sequence not held fixed — increase `n_swaps` in the re-snap.

- [ ] **Step 5: Commit.** `git commit -am "E2: continuous-knob dichotomy runner + curve plot (PASS criterion asserted)"`

---

## Self-review notes (coverage vs spec §3)

- E1 inputs/procedure/outputs/metrics/pass-criteria/falsifier → Tasks 2–4. Exact §B.5 fixture used (cohesionless trap avoided: witness cycles have internal edges).
- E2 SBM-with-rewire generator, ΔH¹ variance + Spearman → Task 5.
- Slides: E1 grouped bars (Task 4), E2 curves (Task 5) — match spec §8.2.
- Pass/FAIL are real assertions (`test_e1.py`, `test_e2.py`); a genuine failure surfaces, not tuned away.
