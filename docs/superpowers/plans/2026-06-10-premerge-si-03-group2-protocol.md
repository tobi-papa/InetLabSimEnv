# Pre-Merge SI — Plan 03: Group 2 — the protocol is exact, sufficient, and tight

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax. **Prerequisite:** Plan 01 (Foundation). Plan 02 not required.

**Goal:** Show (E3) A reconstructs the merge entropy from the M1–M4 message to `<1e-9` on non-toy graphs, and (E4) that all five message coordinates are irredundant — perturbing one at its stated granularity changes `H^{P_M}` while the other three are byte-identical.

**Architecture:** Python runners over the Plan-01 kernel. E3 generates SBM merges with anchor sets and checks `reconstruct == h_partition`. E4 re-packages the five §B.5 witnesses (already locked as C++ fixtures in Plan-01 Task 10) as a Python table + regression test against the published values.

**Tech Stack:** Python, NetworkX, `premerge_kernel`, pandas. Bits everywhere.

**Spec refs:** `preliminary_exp_spec.md` §4 (E3, E4); `pre_merge_SI_theory.md` §5, §7, §B.4, §B.5.

---

## File Structure

```
experiments/
  e3_reconstruction.py        # NEW runner
  e4_irredundancy.py          # NEW runner (table + fixtures)
  premerge_py/
    merge_build.py            # NEW: helper to build (GA,GB,S,partA,partB,P_M) SBM merges
  tests/
    test_e3.py                # NEW
    test_e4.py                # NEW (regression against §7.2 fixture values)
```

---

### Task 1: SBM merge builder (shared helper)

**Files:**
- Create: `experiments/premerge_py/merge_build.py`, `experiments/tests/test_e3.py` (uses it)

- [ ] **Step 1: Failing test** (anchors are shared ids; merge connected). `experiments/tests/test_e3.py`:

```python
from premerge_py import merge_build as mb, kernel as k

def test_builder_shares_anchors():
    inst = mb.build_sbm_merge(sizes_A=[8,8], sizes_B=[8,8], n_anchors=3, seed=1)
    assert len(inst["S"]) == 3
    assert set(inst["S"]).issubset(set(inst["VA"]) & set(inst["VB"]))
```

- [ ] **Step 2: Implement.** `experiments/premerge_py/merge_build.py`:

```python
"""Build a synthetic two-graph merge with a known anchor set S (aligned ids)."""
from premerge_py import generators as gen, kernel as k

def build_sbm_merge(sizes_A, sizes_B, n_anchors, seed, p_in=0.6, p_out=0.05):
    EA, VA, pA_planted = gen.gen_sbm(sizes_A, p_in, p_out, seed=seed)
    EB0, VB0, _ = gen.gen_sbm(sizes_B, p_in, p_out, seed=seed + 1)
    # shift B into its own id range, then remap n_anchors of B onto n_anchors of A
    shift = max(VA) + 1
    VB = [n + shift for n in VB0]; EB = [(u + shift, v + shift) for u, v in EB0]
    a_targets = sorted(VA)[:n_anchors]                 # A nodes to become anchors
    b_sources = sorted(VB)[:n_anchors]                 # B nodes remapped onto them
    remap = dict(zip(b_sources, a_targets))
    VB = sorted({remap.get(n, n) for n in VB})
    EB = [(remap.get(u, u), remap.get(v, v)) for u, v in EB]
    EB = [(u, v) for u, v in EB if u != v]
    S = sorted(set(VA) & set(VB))
    # partitions: agglo on each side
    _, pA = k.h2_min(EA, VA, method="agglo", restarts=200, seed=seed)
    _, pB = k.h2_min(EB, VB, method="agglo", restarts=200, seed=seed + 2)
    return {"EA": EA, "VA": VA, "EB": EB, "VB": VB, "S": S, "pA": pA, "pB": pB}
```

- [ ] **Step 3: Run.** `cd experiments && python -m pytest tests/test_e3.py::test_builder_shares_anchors -v` → passed.
- [ ] **Step 4: Commit.** `git commit -am "premerge_py: SBM merge builder with aligned anchors"`

---

### Task 2: E3 — exact reconstruction runner + pass test

**Files:**
- Create: `experiments/e3_reconstruction.py`
- Modify: `experiments/tests/test_e3.py`

E3 builds the M1–M4 message on the B side (with the Phase-2 anchor carve-out, handled inside `kernel.build_message`), reconstructs `H^{P_M}` on the A side, and compares to the direct `h_partition` on the merged standalone partition. PASS if `max abs error < 1e-9` over several instances (spec §4 E3).

- [ ] **Step 1: Failing pass-criterion test.** Append to `experiments/tests/test_e3.py`:

```python
import e3_reconstruction as e3

def test_e3_reconstruction_exact():
    res = e3.run(n_instances=5, seed=1)
    assert res["max_abs_error"] < 1e-9
    # volume-sum invariant held on every instance (else builder/merge bug)
    assert all(r["volume_ok"] for r in res["rows"])
```

- [ ] **Step 2: Implement.** `experiments/e3_reconstruction.py`:

```python
"""E3 — exact reconstruction from the M1–M4 message. Spec §4 E3."""
import argparse
from premerge_py import merge_build as mb, kernel as k, results as R

def _build_PM(inst):
    """Merged standalone partition P_M: A keeps pA; B-private keep pB; anchors fold to A-side."""
    Sset = set(inst["S"])
    pM = {}
    for n in inst["VA"]: pM[n] = ("A", inst["pA"][n])
    for n in inst["VB"]:
        if n not in Sset: pM[n] = ("B", inst["pB"][n])
    for s in inst["S"]: pM[s] = ("A", inst["pA"][s])
    labels = {lab: i for i, lab in enumerate(sorted(set(pM.values()), key=str))}
    return {n: labels[lab] for n, lab in pM.items()}

def run(n_instances=5, seed=1, sizes_A=(8, 8), sizes_B=(8, 8)):
    rows = []; max_err = 0.0
    for i in range(n_instances):
        inst = mb.build_sbm_merge(list(sizes_A), list(sizes_B), n_anchors=2 + i, seed=seed + i)
        msg = k.build_message(inst["EB"], inst["VB"], inst["S"], inst["pB"])
        recon = k.reconstruct(inst["EA"], inst["VA"], inst["S"], inst["pA"], msg)
        EM, VM = k.merge(inst["EA"], inst["VA"], inst["EB"], inst["VB"])
        pM = _build_PM(inst)
        direct = k.h_partition(EM, VM, pM)
        # volume-sum invariant
        deg = {}
        for u, v in EM: deg[u] = deg.get(u, 0) + 1; deg[v] = deg.get(v, 0) + 1
        volume_ok = sum(deg.values()) == 2 * len(EM)
        err = abs(recon - direct); max_err = max(max_err, err)
        rows.append({"instance": i, "n_S": len(inst["S"]), "recon": recon, "direct": direct,
                     "abs_error": err, "volume_ok": volume_ok,
                     "n_EB_S": len(msg["M2"]), "n_hist_bins": len(msg["M3"]), "n_B_modules": len(msg["M4"])})
    return {"rows": rows, "max_abs_error": max_err}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--n_instances", type=int, default=5); ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--out", default="results"); a = ap.parse_args()
    res = run(a.n_instances, a.seed)
    status = "PASS" if res["max_abs_error"] < 1e-9 else "FAIL"
    run = R.RunWriter("E3", base_seed=a.seed, out_root=a.out)
    for r in res["rows"]:
        run.add_row({"instance_id": str(r["instance"]), "n_S": r["n_S"],
                     "realized_bias": r["abs_error"], "within_bound": r["volume_ok"],
                     "n_EB_S": r["n_EB_S"], "n_hist_bins": r["n_hist_bins"], "n_B_modules": r["n_B_modules"]})
    run.close(config={**vars(a), "max_abs_error": res["max_abs_error"], "status": status})
    print(f"E3 {status}: max|recon-direct|={res['max_abs_error']:.2e} (<1e-9)")

if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Run.**
Run: `cd experiments && python -m pytest tests/test_e3.py -v && python e3_reconstruction.py`
Expected: `E3 PASS: max|recon-direct|=...e-15 (<1e-9)`. FALSIFIER (spec §4 E3): nonzero error ⇒ Phase-2 carve-out, `o_i` double-count, or inconsistent `W` — the volume-sum assertion localizes it.

- [ ] **Step 4: Commit.** `git commit -am "E3: exact M1-M4 reconstruction runner (PASS <1e-9 asserted)"`

---

### Task 3: E4 — irredundancy witnesses runner + regression test

**Files:**
- Create: `experiments/e4_irredundancy.py`, `experiments/tests/test_e4.py`

E4 packages the five §7.2 witnesses (the C++ regression in Plan-01 Task 10 already locks the math; here we present the table and assert the published values in Python). Each target `T ∈ {M1, M2, M3, M4-int, M4-vol}`: build two partners agreeing on the other three coordinates, differing only in `T`; assert the held-fixed coordinates equal and `H^{P_M}` differs at the fixture values, to 1e-5 (spec §4 E4).

- [ ] **Step 1: Transcribe the §B.5 witness fixtures.** `experiments/premerge_py/witness_e4.py`:

```python
"""Exact §B.5 / §7.2 irredundancy witnesses (verbatim edge lists)."""
# Fixed G_A: anchors S=[0,1,2,3], private {100,101,102,103}, two modules.
GA = dict(
    E=[(0,1),(0,100),(1,101),(100,101),(0,101),
       (2,3),(2,102),(3,103),(102,103),(2,103),(1,2)],
    V=[0,1,2,3,100,101,102,103],
    part={0:0,1:0,100:0,101:0, 2:1,3:1,102:1,103:1},
    S=[0,1,2,3],
)
# Separate 6-anchor G_A for the M2 witness.
GA6 = dict(
    E=[(0,1),(1,2),(0,2),(0,100),(1,100),
       (3,4),(4,5),(3,5),(3,101),(4,101),(2,3)],
    V=[0,1,2,3,4,5,100,101],
    part={0:0,1:0,2:0,100:0, 3:1,4:1,5:1,101:1},
    S=[0,1,2,3,4,5],
)
# M4-vol witness partner (cohesive, asymmetric modules so the volume field is necessary, §7.4):
EB_w = [(201,202),(201,0),(202,1),(203,204),(204,205),(203,205),
        (203,2),(204,3),(205,0),(200,0),(200,1)]

# Each witness: (target, GA_key, EB_v1, EB_v2, partB_v1, partB_v2, expected_HP_v1, expected_HP_v2).
# Edge lists + partitions + expected H^P values are verbatim from §B.5 / §7.2.
WITNESSES = [
    ("M1", "GA",
     [(200, 0)], [(200, 2)],
     {200: 5}, {200: 5},
     2.188297, 2.173108),
    ("M2", "GA6",
     [(0, 1), (1, 2), (2, 3), (3, 4), (4, 5), (5, 0)],           # 6-cycle on anchors
     [(0, 1), (1, 2), (0, 2), (3, 4), (4, 5), (3, 5)],           # two triangles (same anchor degrees)
     {}, {},                                                      # all anchors; no B-private nodes
     2.125815, 2.049452),
    ("M3", "GA",
     [(200, 0), (201, 0), (201, 1), (201, 2)],
     [(200, 0), (200, 1), (201, 0), (201, 2)],
     {200: 5, 201: 5}, {200: 5, 201: 5},
     2.383605, 2.408767),
    ("M4int", "GA",
     [(200, 201), (202, 203), (200, 202), (201, 203)],
     [(200, 202), (200, 203), (201, 202), (201, 203)],
     {200: 5, 201: 5, 202: 6, 203: 6}, {200: 5, 201: 5, 202: 6, 203: 6},
     2.187014, 2.574600),
    ("M4vol", "GA",
     EB_w, EB_w,                                                  # same edges; only partB differs
     {200: 5, 201: 5, 202: 5, 203: 6, 204: 6, 205: 6},
     {200: 6, 201: 5, 202: 5, 203: 6, 204: 6, 205: 6},
     2.529236, 2.542125),
]
```

> The Python expected values must match the C++ `test_witnesses.cpp` (Plan-01 Task 10) to 1e-5 — that C++ fixture is the authoritative cross-check.

- [ ] **Step 2: Build the merged-standalone H^P helper + runner.** `experiments/e4_irredundancy.py`:

```python
"""E4 — irredundancy witnesses (M1–M4 is tight). Spec §4 E4."""
import argparse
from premerge_py import kernel as k, results as R, witness_e4 as W

def _hp_merged(ga, EB, partB):
    VB = sorted({n for e in EB for n in e} | set(ga["S"]))
    EM, VM = k.merge(ga["E"], ga["V"], EB, VB)
    pM = {}
    for n in ga["V"]: pM[n] = ("A", ga["part"][n])
    Sset = set(ga["S"])
    for n in VB:
        if n not in Sset: pM[n] = ("B", partB[n])
    labels = {lab: i for i, lab in enumerate(sorted(set(pM.values()), key=str))}
    return k.h_partition(EM, VM, {n: labels[lab] for n, lab in pM.items()})

def run():
    rows = []
    for (target, ga_key, EB1, EB2, partB1, partB2, exp1, exp2) in W.WITNESSES:
        ga = W.GA6 if ga_key == "GA6" else W.GA
        h1 = _hp_merged(ga, EB1, partB1); h2 = _hp_merged(ga, EB2, partB2)
        rows.append({"target": target, "HP_v1": h1, "HP_v2": h2,
                     "differ": abs(h1 - h2) > 1e-6, "exp_v1": exp1, "exp_v2": exp2,
                     "match": abs(h1 - exp1) < 1e-5 and abs(h2 - exp2) < 1e-5})
    return rows

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--out", default="results"); a = ap.parse_args()
    rows = run()
    status = "PASS" if all(r["differ"] and r["match"] for r in rows) else "FAIL"
    run = R.RunWriter("E4", base_seed=0, out_root=a.out)
    for r in rows:
        run.add_row({"instance_id": r["target"], "H_standalone": r["HP_v1"], "H_bridge": r["HP_v2"]})
    run.close(config={"status": status, "rows": rows})
    for r in rows:
        print(f"  {r['target']:7s} {r['HP_v1']:.6f} vs {r['HP_v2']:.6f}  differ={r['differ']} match={r['match']}")
    print(f"E4 {status}")

if __name__ == "__main__":
    main()
```

- [ ] **Step 3: Regression test.** `experiments/tests/test_e4.py`:

```python
import e4_irredundancy as e4

def test_e4_all_witnesses_irredundant():
    rows = e4.run()
    assert len(rows) == 5
    for r in rows:
        assert r["differ"], f"{r['target']} did not change H^P"
        assert r["match"],  f"{r['target']} did not reproduce fixture value"
```

- [ ] **Step 4: Run.**
Run: `cd experiments && python -m pytest tests/test_e4.py -v && python e4_irredundancy.py`
Expected: 5 witnesses, all `differ=True match=True`, `E4 PASS`. FALSIFIER (spec §4 E4) for M4-vol: if it does not differ, a cohesionless/symmetric module was used — must be `EB_w` (cohesive, asymmetric), §7.4.

- [ ] **Step 5: Commit.** `git commit -am "E4: five irredundancy witnesses runner + regression (fixture values to 1e-5)"`

---

## Self-review notes (coverage vs spec §4)

- E3 build_message + reconstruct at non-toy size, max error, volume-sum assertion, message component sizes recorded → Tasks 1–2.
- E4 five §7.2 witnesses, held-fixed equality + target difference + fixture values, M4-vol cohesion trap noted → Task 3 (authoritative cross-check is the Plan-01 C++ `test_witnesses.cpp`).
- Placeholder check: `witness_e4.py` Step-1 note flags that the commented exact rows MUST be transcribed (not left as placeholder) — the implementing engineer copies them from §B.5 and the C++ fixture verifies them.
- Pass/FAIL are real assertions; the E4 fixture values double as regression guards.
