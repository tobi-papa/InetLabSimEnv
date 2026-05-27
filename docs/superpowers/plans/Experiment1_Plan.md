# Experiment Implementation Plan: Mutual Information Behaviour Under Variable Overlap Between Owner Graphs in a Distributed Information Network

INETLAB — Keio University — May 2026

This document specifies the complete experimental protocol. It is language-agnostic: all algorithms are described as mathematical operations and pseudocode. The goal is to eliminate all ambiguity so that the experiment can be implemented in any programming environment and produce reproducible results.

---

## 1. Research Question

This experiment answers one precise question:

> How do entropy-based and mutual-information-based similarity measures behave as the node overlap between two graphs changes?

Specifically, we want to determine:

1. Does MI increase monotonically with overlap, or does it exhibit non-trivial behaviour (plateaus, phase transitions)?
2. Is MI (as defined on V_∩) genuinely invariant to the surrounding structural context of the two graphs, as its V_∩-restricted definition implies? (Invariance test — see §4.1.4 and §10.)
3. Is there a minimum overlap threshold below which MI carries no useful structural signal?
4. Do different graph topologies (community-structured, scale-free, random) produce qualitatively different MI-overlap curves?
5. Does the method of subgraph extraction (induced subgraph vs random walk sampling) affect the results?

---

## 2. Primary Scenario: Owner vs Owner

### 2.1 Conceptual Setup

Two information service owners (Owner A and Owner B) each manage a graph of information resources. Some resources (nodes) exist in both services — for example, the same scientific article indexed by two different platforms. Each owner connects these resources with edges representing their own relational view (citations, topic similarity, co-authorship, etc.).

The key property: the two graphs share a subset of node IDs (the overlap), but may have completely different edge structures even on those shared nodes. This models the real INETLAB inter-networking scenario (Pillar 4) where different services provide different relational perspectives on shared resources.

### 2.2 Why This Scenario Is Primary

- It directly addresses the lab mission of cross-service information networking.
- The overlap is natural and observable (shared resource IDs), not dependent on walker encounter mechanics.
- It is the simplest scenario to control experimentally: overlap is a parameter we set directly.
- Results generalise to other scenarios (walker vs walker, temporal snapshots) because the mathematical setup is identical.

---

## 3. Graph Generation

### 3.1 Mother Graph G

All subgraphs are derived from a single mother graph G. This ensures controlled conditions. Three topologies are tested:

| Topology | Model | Parameters | Key Property |
|---|---|---|---|
| Community-structured | Stochastic Block Model (SBM) | K=4 communities, p_in=0.05, p_out=0.005 | Known ground-truth communities; most relevant to INETLAB |
| Scale-free | Barabási-Albert (BA) | m=5 (edges per new node) | Heavy-tailed degree distribution; models real information networks with hubs |
| Random (null model) | Erdős-Rényi (ER) | p = 2m/(N-1) where m matches BA edge count | No community structure; serves as null hypothesis baseline |

### 3.2 Graph Scales

Each topology is generated at three scales to test size sensitivity:

| Label | N (nodes) | Purpose |
|---|---|---|
| Small | 500 | Fast iteration during development; catch bugs early |
| Medium | 1000 | Primary results; good statistical power with reasonable runtime |
| Large | 5000 | Scalability check; confirms results hold at realistic sizes |

Total mother graphs: 3 topologies × 3 scales = 9 mother graphs.

---

## 4. Subgraph Extraction Protocol

From each mother graph G, we extract pairs (G_A, G_B) with controlled overlap. Two extraction methods are tested independently.

### 4.1 Method 1: Induced Subgraphs

#### 4.1.1 Node Selection

Given a target overlap ratio α ∈ [0.0, 0.1, 0.2, ..., 0.9, 1.0] (11 values), and a target subgraph size of N/4 nodes per owner:

1. Compute the number of shared nodes: `n_shared = floor(α × N/4)`.
2. Compute the number of exclusive nodes per owner: `n_exclusive = N/4 − n_shared`.
3. Select `n_shared` nodes from G to form the shared set S.
4. Select `n_exclusive` nodes from `G \ S` to form the exclusive set R_A for Owner A.
5. Select `n_exclusive` nodes from `G \ (S ∪ R_A)` to form the exclusive set R_B for Owner B.
6. `V_A = S ∪ R_A`. `V_B = S ∪ R_B`.

Verification check: `|V_A| = |V_B| = N/4`. `|V_A ∩ V_B| = n_shared`. `|V_A ∪ V_B| = N/2 − n_shared`. The Jaccard overlap = `n_shared / (N/2 − n_shared)`.

**Rationale for owner size N/4:** With K=4 SBM communities of N/4 nodes each, an owner size of N/4 lets the DIFF condition place S, R_A, R_B in three distinct communities for every α ∈ [0,1] (see §4.1.4). Owner size N/2 would force DIFF to span two communities and become infeasible for α > 0.5. BA and ER use the same owner size for consistency.

**CRITICAL NOTE on α vs Jaccard:** α is the parameter used to generate the pair (proportion of N/4 that is shared). Jaccard is the measured overlap used as the x-axis in all plots. They are NOT the same number. Closed form: Jaccard = α / (2 − α). Example: N=1000, α=0.5 gives n_shared=125, |V_A ∪ V_B|=375, Jaccard=125/375≈0.33. Always record both values. Always plot against Jaccard, not α.

#### 4.1.2 Edge Construction — Sub-experiment 1A: Shared Edges on Overlap

In this sub-experiment, the two owners see the SAME edges on shared nodes. This models the case where the shared resources have an objective, agreed-upon relational structure.

1. `G_A` = induced subgraph of G on V_A. That is: include every edge (u,v) from G where both u ∈ V_A and v ∈ V_A.
2. `G_B` = induced subgraph of G on V_B. Same rule.
3. Consequence: for any edge (u,v) where both u ∈ S and v ∈ S, the edge exists in BOTH G_A and G_B identically.

#### 4.1.3 Edge Construction — Sub-experiment 1B: Different Edges on Overlap

In this sub-experiment, the two owners see DIFFERENT edges even on shared nodes. This models the case where each service has its own relational perspective on the same resources.

1. For the exclusive nodes: G_A includes all edges from G among V_A nodes (same as 1A).
2. For edges between a shared node and an exclusive node: include normally in the relevant graph with no coin flip. Specifically, an edge (u ∈ S, v ∈ R_A) is included in G_A (it cannot appear in G_B since v ∉ V_B), and an edge (u ∈ S, v ∈ R_B) is included in G_B (it cannot appear in G_A since v ∉ V_A). These edges exist in only one graph by construction.
3. For the shared nodes only: for every edge (u,v) of G with BOTH u ∈ S and v ∈ S, include it in G_A with probability 0.5, and independently include it in G_B with probability 0.5. Use a separate random coin for each graph. (Only edges that already exist in G are candidates — we are subsampling existing edges, not generating new ones.)
4. This means some edges on shared nodes appear in both, some in only one, some in neither. The expected E_12 on shared nodes is approximately 0.25 × (number of edges among S in G).

This is crucial: sub-experiment 1B tests whether MI can detect that two owners have different relational views even when they share the same nodes. This is the harder and more realistic case.

#### 4.1.4 Structural Conditions (Invariance Test for NMI/DC-NMI)

For SBM graphs only, node selection is performed under two conditions designed as an **invariance test** for the V_∩-restricted measures (NMI and DC-NMI).

**Motivation.** NMI and DC-NMI (§5.3, §5.4) are restricted to V_∩ = V_A ∩ V_B. By construction R_A, R_B are disjoint from S and from each other, so V_∩ = S exactly. NMI and DC-NMI therefore cannot, even in principle, depend on the community origin of R_A or R_B — their values are determined entirely by the structure on S. The SAME vs DIFF conditions test this property empirically: they vary R_A and R_B while holding S's distribution fixed. The expected outcomes are:

- **NMI and DC-NMI:** invariant between SAME and DIFF at every α. Observed invariance confirms the measures behave as theory predicts. Observed deviation indicates an implementation bug (most likely in V_∩ restriction or edge intersection) and blocks downstream analysis.
- **D_H1 and D_JS:** *will* differ between SAME and DIFF, because these measures use the whole graph (including R_A and R_B), and the two conditions produce different expected edge densities outside S (see "Predicted baseline difference" below). This is not a defect of the baselines but a feature: the contrast demonstrates the **specificity** of V_∩-restricted MI — it responds only to shared-node structure, while degree-based summaries conflate shared and exclusive structure.

**Design.** Each SBM community has N/4 nodes. With owner size N/4:

In both conditions, S is drawn uniformly at random from community 1 (n_shared ≤ N/4, so S always fits). This holds the shared-node structure identical across conditions in expectation.

**Condition SAME:** R_A and R_B are drawn jointly and disjointly from the pool **community 2 ∪ community 4** (size N/2). Both owners therefore draw exclusive nodes from the same mixed two-community pool — in expectation each owner's exclusive set is ~50% community 2 and ~50% community 4. Feasible for all α: required disjoint pool size is `2·n_exclusive ≤ N/2`, which always holds since `n_exclusive ≤ N/4`.

**Condition DIFF:** R_A is drawn from community 2 (size N/4 ≥ n_exclusive); R_B is drawn from community 3 (size N/4 ≥ n_exclusive). Each owner's exclusive set comes from a single, distinct community.

**Predicted baseline difference (sanity for D_H1 / D_JS).** Let p_in = 0.05, p_out = 0.005 (§3.1).

- DIFF R_A is pure community 2: expected internal edge density = p_in.
- SAME R_A is ~50% community 2 and ~50% community 4: expected internal edge density ≈ 0.5·p_in + 0.5·p_out = 0.0275 (about 55% of DIFF's density).

So G_A's overall edge count is systematically lower in SAME than in DIFF (similarly for G_B). D_H1 and D_JS, being functions of degree distributions, should reflect this — typically as a small but consistent shift between conditions, not as a function of α. NMI and DC-NMI should not.

For BA and ER graphs, which have no ground-truth communities, only one condition exists (RANDOM: all nodes selected uniformly at random). The invariance test is not meaningful in the absence of community structure.

### 4.2 Method 2: Random Walk Sampling

#### 4.2.1 Walk Protocol

Instead of selecting nodes and inducing, we simulate exploration:

1. Choose a starting node s_A for Walker A and s_B for Walker B.
2. Each walker performs L steps of a simple random walk (at each step, move to a uniformly random neighbour).
3. Walker A's subgraph G_A consists of all edges traversed during the walk (not the induced subgraph on visited nodes — only edges actually walked).
4. Walker B's subgraph G_B is constructed the same way.

**Note on sub-experiments:** The 1A/1B distinction does not apply to random walk extraction. Each walker traverses its own path, so the edge sets are naturally different even on shared nodes. Random walk is inherently closest to sub-experiment 1B (different edges on overlap). In the output data, random walk pairs are labelled `sub_experiment = 'walk'`.

#### 4.2.2 Controlling Overlap

Overlap is not set directly; it emerges from the proximity of starting nodes and walk length:

- To get HIGH overlap: start both walkers from the same node or neighbouring nodes, with long walks.
- To get LOW overlap: start walkers from nodes in different communities (SBM) or at maximum graph distance.
- To sweep overlap: fix L and vary the starting distance d(s_A, s_B) from 0 to diameter(G).

After each pair of walks, the actual overlap is measured post-hoc: `overlap_ratio = |V_A ∩ V_B| / |V_A ∪ V_B|`. This means overlap is a measured variable, not a controlled parameter. Consequently, more repetitions are needed to cover the overlap range densely (at least 200 pairs per configuration).

#### 4.2.3 Walk Length

Walk length L controls both subgraph size and overlap probability. Test `L ∈ {N/10, N/4, N/2, N}`. For N=1000 this gives `L ∈ {100, 250, 500, 1000}`.

#### 4.2.4 Important Difference from Method 1

In Method 1 (induced), G_A contains ALL edges among its node set. In Method 2 (walk), G_A contains ONLY edges traversed. This means Method 2 subgraphs are typically much sparser. The MI computation is identical, but the values will be systematically different. Comparing Method 1 and Method 2 answers the question: does the extraction method matter, or does MI capture structural similarity regardless of how the subgraph was obtained?

---

## 5. Measures to Compute

For every pair (G_A, G_B), compute ALL of the following. This allows direct comparison of measures.

### 5.1 Baseline: Structural Entropy Difference

**Definition:**

```
D_H1 = |H₁(G_A) − H₁(G_B)|
```

where:

```
H₁(G) = −Σ_i (d_i / vol(G)) · log₂(d_i / vol(G))
```

summed over all nodes i in G. `vol(G) = Σ_i d_i = 2|E|`.

- **Input:** Only degree sequence of each graph separately.
- **Output:** A non-negative real number. 0 = identical degree distributions.
- **Limitation:** Compares global summaries; two structurally different graphs can yield D_H1 = 0.
- **Role in experiment:** Baseline. If NMI and DC-NMI perform no better than D_H1 at discriminating structural conditions, they add no value.

### 5.2 Jensen-Shannon Divergence of Degree Distributions

**Definition:**

```
D_JS = JS(P_A ‖ P_B)
```

where P_A is the normalised degree histogram of G_A (probability that a random node has degree k), and:

```
JS(P‖Q) = 0.5 × KL(P‖M) + 0.5 × KL(Q‖M)
M = 0.5 × (P + Q)
KL(P‖M) = Σ_k P(k) · log₂(P(k) / M(k))   for P(k) > 0
```

- **Input:** Degree histograms of each graph. No node alignment needed.
- **Output:** A value in [0, 1] (when using log base 2). 0 = identical distributions.
- **Role in experiment:** Second baseline. This is the Option B / permutation-invariant measure. It uses no node identity information. If NMI (which uses node identities) performs significantly better, that proves the value of node alignment.

### 5.3 NMI — Standard Graph Mutual Information (Felippe et al. 2024)

**Scope:** Restricted to V_∩ = V_A ∩ V_B. Only shared nodes participate.

**Step-by-step computation:**

```
1. Identify V_∩ = set of node IDs present in both G_A and G_B.

2. Set N_∩ = |V_∩|.
   (Use N_∩ to distinguish from N = mother graph size.)
   If N_∩ < 2, output NMI = 0 (undefined, insufficient overlap).

3. Restrict G_A to V_∩: keep only edges (u,v) where both u ∈ V_∩ and v ∈ V_∩.
   Call this G_A'. Count its edges: E_1.

4. Restrict G_B to V_∩ similarly. Call this G_B'. Count its edges: E_2.

5. Compute the edge intersection:
   E_12 = |{(u,v) : (u,v) ∈ G_A' AND (u,v) ∈ G_B'}|

6. Compute C = N_∩ × (N_∩ − 1) / 2
   (number of possible undirected edges on N_∩ nodes)

7. Compute probabilities:
   p1  = E_1 / C
   p2  = E_2 / C
   p12 = E_12 / C

8. Build contingency table:
   P = [p12, p1−p12, p2−p12, 1−p1−p2+p12]
   Verify: all four values ∈ [0,1] and sum to 1.
   If any value is negative due to floating point, clamp to 0.

9. Binary entropy function:
   Hb(x) = −x·log₂(x) − (1−x)·log₂(1−x)
   Convention: 0·log₂(0) = 0

10. Shannon entropy of contingency table:
    Hs(P) = −Σ p_i·log₂(p_i)   for all p_i > 0

11. Mutual information:
    I = Hb(p1) + Hb(p2) − Hs(P)

12. Normalise:
    NMI = 2 × I / (Hb(p1) + Hb(p2))
    If denominator = 0, output NMI = 0.
```

**How to compute E_12 in practice:** Represent each graph's edges as a set of sorted tuples `{(min(u,v), max(u,v))}`. E_12 = size of set intersection. Cost: O(E_1 + E_2) with hash sets.

**Edge cases:**

- α = 0: V_∩ is empty, N_∩ = 0. Output NMI = 0.
- α = 1, sub-experiment 1A: G_A = G_B, so E_12 = E_1 = E_2, and NMI = 1.0 exactly.
- α = 1, sub-experiment 1B: NMI < 1.0 because the coin flip produces different edges.
- Very sparse overlap: If E_1 = 0 or E_2 = 0 on V_∩, then Hb(p1) = 0 or Hb(p2) = 0. NMI = 0.

### 5.4 DC-NMI — Degree-Corrected Graph MI (Felippe et al. 2024)

**Scope:** Same restriction to V_∩.

**Step-by-step computation:**

```
1. For each node i ∈ V_∩, compute:
   k1[i]  = degree of i in G_A' (restricted to V_∩)
   k2[i]  = degree of i in G_B' (restricted to V_∩)
   k12[i] = number of neighbours of i that are the SAME in both G_A' and G_B'

2. How to compute k12[i]:
   Iterate over edges in E_intersection (edges present in both graphs).
   For each shared edge (i,j): increment k12[i] by 1 AND k12[j] by 1.

3. For each node i ∈ V_∩:
   p1_i  = k1[i] / (N_∩ − 1)
   p2_i  = k2[i] / (N_∩ − 1)
   p12_i = k12[i] / (N_∩ − 1)

4. Build per-node contingency table:
   P_i = [p12_i, p1_i−p12_i, p2_i−p12_i, 1−p1_i−p2_i+p12_i]
   Clamp any negative values to 0.

5. Per-node MI:
   MI_i    = Hb(p1_i) + Hb(p2_i) − Hs(P_i)
   denom_i = Hb(p1_i) + Hb(p2_i)

6. Aggregate over nodes:
   I_sum     = Σ_i MI_i
   denom_sum = Σ_i denom_i
   (The 1/N_∩ averaging factor cancels in the ratio below, so summing directly is equivalent and cheaper.)

7. Normalise:
   DC_NMI = 2 × I_sum / denom_sum
   If denom_sum = 0, output 0.
```

**Key difference from NMI:** NMI treats all edges equally. DC-NMI weights the contribution of each node by its degree. In scale-free graphs, hubs dominate the DC-NMI signal. This is desirable if hub behaviour is important; it is a potential confound if it is not.

### 5.5 Summary of Measures

| Measure | Uses node IDs? | Uses edge overlap? | Captures per-node structure? | Role |
|---|---|---|---|---|
| D_H1 (entropy diff) | No | No | No | Weakest baseline |
| D_JS (degree dist.) | No | No | No | Invariant baseline |
| NMI | Yes | Yes | No (global edge count) | Primary measure |
| DC-NMI | Yes | Yes | Yes (per-node degree) | Refined measure |

---

## 6. Complete Experimental Matrix

Every cell in the following table is one experimental configuration. Each configuration produces one MI-vs-overlap curve.

| Dimension | Values | Count |
|---|---|---|
| Graph topology | SBM, BA, ER | 3 |
| Graph scale N | 500, 1000, 5000 | 3 |
| Extraction method | Induced, Random Walk | 2 |
| Edge overlap sub-experiment | 1A (shared edges), 1B (different edges) | 2 (induced only) |
| Structural condition | SAME, DIFF (SBM only), RANDOM (BA/ER) | 2 for SBM, 1 for BA/ER |
| Overlap values α (induced only) | 0.0 to 1.0 in steps of 0.1 | 11 |
| Repetitions per α (induced) | 100 | 100 |
| Pairs per configuration (walk; α measured post-hoc, not swept) | 200 | 200 |
| Walk length L (walk only) | N/10, N/4, N/2, N | 4 |
| Measures per pair | D_H1, D_JS, NMI, DC-NMI | 4 |

Total configurations for induced extraction (conditions sum per topology, not multiplied across them):

- SBM: 3 scales × 2 sub-experiments × 2 conditions = 12
- BA:  3 scales × 2 sub-experiments × 1 condition  = 6
- ER:  3 scales × 2 sub-experiments × 1 condition  = 6
- **Total induced: 24 curves.**

Total configurations for random walk extraction:

- SBM: 3 scales × 4 walk lengths × 2 conditions = 24
- BA:  3 scales × 4 walk lengths × 1 condition  = 12
- ER:  3 scales × 4 walk lengths × 1 condition  = 12
- **Total walk: 48 curves.**

Total pairs to evaluate (rough estimate):

- Induced: 24 curves × 11 α values × 100 reps = 26,400 pairs.
- Walk: 48 curves × 200 pairs per configuration (α is measured post-hoc, not swept) = 9,600 pairs.
- **Total: ~36,000 graph pairs.** Each pair requires 4 measures. Computationally feasible on a modern workstation in hours, not days.

---

## 7. Output Specification

### 7.1 Raw Data Schema

For every graph pair, store one row with the following fields:

| Field | Type | Description |
|---|---|---|
| `topology` | String | `SBM` / `BA` / `ER` |
| `N` | Integer | Number of nodes in mother graph |
| `extraction` | String | `induced` / `walk` |
| `sub_experiment` | String | `1A` / `1B` / `walk` |
| `condition` | String | `SAME` / `DIFF` / `RANDOM` |
| `alpha_target` | Float | Target overlap α (induced) or NaN (walk) |
| `walk_length` | Integer | L (walk only) or NaN (induced) |
| `start_distance` | Integer | Graph distance between start nodes (walk only) |
| `n_nodes_A` | Integer | Number of nodes in G_A |
| `n_nodes_B` | Integer | Number of nodes in G_B |
| `n_shared_nodes` | Integer | \|V_A ∩ V_B\| |
| `overlap_jaccard` | Float | \|V_∩\| / \|V_A ∪ V_B\| |
| `E_A` | Integer | Edge count in G_A |
| `E_B` | Integer | Edge count in G_B |
| `E_A_restricted` | Integer | Edges of G_A within V_∩ |
| `E_B_restricted` | Integer | Edges of G_B within V_∩ |
| `E_12` | Integer | Edge intersection on V_∩ |
| `H1_A` | Float | Structural entropy of G_A |
| `H1_B` | Float | Structural entropy of G_B |
| `D_H1` | Float | \|H1_A − H1_B\| |
| `D_JS` | Float | Jensen-Shannon of degree distributions |
| `NMI` | Float | Normalised MI on V_∩ |
| `DC_NMI` | Float | Degree-corrected NMI on V_∩ |
| `repetition_id` | Integer | Index of this repetition |

Store as a flat tabular file (CSV or equivalent). One file per mother graph configuration is acceptable for organisation, but all must be mergeable into a single table for analysis.

### 7.2 Aggregated Plots

For each configuration produce one plot. A configuration is the tuple (topology, N, extraction, sub_experiment, condition) for induced extraction, and (topology, N, extraction, walk_length, condition) for walk extraction.

- X-axis: `overlap_jaccard` (measured, not target α).
- Y-axis: measure value (one curve per measure, or separate panels).
- Each point: median over repetitions. Error bars: 25th and 75th percentiles.
- For SBM: overlay SAME and DIFF conditions on the same plot with different colours.

**The most important plot of the entire experiment (invariance check):** SBM, N=1000, induced extraction, sub-experiment 1B (different edges), NMI on Y-axis, with SAME (blue) and DIFF (red) conditions overlaid. The experiment is a success if the two curves are **statistically indistinguishable** (overlapping IQR bands across all α). This empirically confirms that NMI's V_∩-restricted definition makes it invariant to the community origin of exclusive nodes — a property that follows from theory but must be verified before NMI can be trusted as a structural similarity measure for the INETLAB setting. Visible separation would indicate either an implementation bug or a hidden confound and would require investigation before any other result is trusted.

---

## 8. Validation Checks (Run Before Full Experiment)

Before running the full matrix, execute these sanity checks on the Small scale (N=500) to catch implementation bugs.

### 8.1 Identity Check

Set α = 1.0, sub-experiment 1A (shared edges). G_A and G_B should be identical graphs.

- Expected: `NMI = 1.0` exactly, `DC-NMI = 1.0` exactly, `D_H1 = 0.0` exactly, `D_JS = 0.0` exactly.
- If any value deviates: bug in edge restriction or intersection logic.

### 8.2 Zero Overlap Check

Set α = 0.0. V_∩ is empty.

- Expected: `NMI = 0.0`, `DC-NMI = 0.0` (undefined, convention = 0).
- D_H1 and D_JS may be non-zero (they do not use node alignment).

### 8.3 Symmetry Check

For any pair, verify `NMI(G_A, G_B) = NMI(G_B, G_A)` and `DC-NMI(G_A, G_B) = DC-NMI(G_B, G_A)`.

- Expected: identical to floating-point precision.
- If asymmetric: bug in contingency table construction.

### 8.4 Plateau Sanity Check

NMI is defined on V_∩, and V_∩ = S by construction (§4.1.4). As α grows, |V_∩| grows but the *structural agreement* between G_A|_{V_∩} and G_B|_{V_∩} does not necessarily grow. Expect plateau behaviour, not smooth monotonic increase.

On SBM, condition SAME, sub-experiment 1A: plot NMI vs α for 20 repetitions.

- Expected: NMI = 0 at α = 0 by convention (N_∩ < 2). Once N_∩ is large enough to contain edges, NMI jumps to **≈ 1.0** and stays there for all α > 0. This is because, in 1A, G_A|_{V_∩} and G_B|_{V_∩} are both equal to the induced subgraph of G on S (identical by construction), so I = Hb(p1) and NMI = 1. The curve is therefore a step function, not a ramp.
- If NMI is < 1 in the plateau region (away from the left edge): bug in V_∩ restriction or edge intersection.
- If NMI does *not* jump to ≈1 once N_∩ has edges: more serious bug — verify §8.5 contingency-table validity first.

Then repeat on sub-experiment 1B (different edges on overlap):

- Expected: NMI rises from 0 (at α = 0) to a plateau value strictly between 0 and 1, determined by the coin-flip statistics on edges in G|_S. The plateau height should be approximately independent of α (since p1, p2, p12 depend on the edge *density* on S, not on |S|), with variance that shrinks as α grows. The median curve should look like a quick rise then a flat plateau.
- If 1B's plateau equals 1: the coin-flip is not actually being applied independently in G_A and G_B — bug in §4.1.3 step 3.
- If 1B's plateau equals 0: the coin-flip is degenerate (always same outcome) — also a bug.

### 8.5 Contingency Table Validity

For every pair, verify:

- All four entries of P are in [0, 1].
- Sum of P = 1.0 (up to floating-point tolerance of 1e-10).
- `p12 ≤ min(p1, p2)`. If violated: E_12 was computed incorrectly.
- `p1 − p12 ≥ 0`, `p2 − p12 ≥ 0`. If violated: intersection count exceeds individual counts.

---

## 9. Known Risks and Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| NMI/DC-NMI separate SAME from DIFF (invariance fails) | Implementation bug somewhere in V_∩ restriction, edge intersection, or contingency table | Investigate before proceeding; rerun validation §8 (esp. §8.3 symmetry, §8.5 contingency validity); compare against a reference implementation |
| Baselines (D_H1, D_JS) do NOT differ between SAME and DIFF | The predicted density difference does not materialise, suggesting an issue with the SBM realisation | Verify p_in / p_out are correctly applied; check actual edge counts in G_A vs G_B before computing measures |
| Random walk subgraphs are too sparse for meaningful MI | NMI ≈ 0 for all walk-based pairs | Increase walk length L; if still zero, report as a negative result |
| SBM community boundaries are too sharp, unrealistic | Results do not generalise to real graphs | Test also on BA (realistic degree distribution); consider planted partition with softer boundaries |
| Sub-experiment 1B coin flip produces too much noise | Variance swamps signal | Increase repetitions to 200; consider higher inclusion probability (0.7) instead of 0.5 |
| Scale-free hubs dominate DC-NMI | DC-NMI reflects hub overlap, not community structure | Report separately on hub-removed subgraphs as robustness check |
| N=5000 runtime is too long | Cannot complete full matrix | Run N=5000 only for the most promising configurations identified at N=1000 |

---

## 10. Success Criteria

The experiment is considered successful if ALL of the following hold:

1. **Invariance (primary criterion):** On SBM at N=1000, sub-experiment 1B, the medians of NMI and DC-NMI for SAME and DIFF conditions are **statistically indistinguishable** at every overlap level α ∈ [0.1, 1.0]. Use Mann-Whitney U test, two-sided, with a non-rejection threshold at p > 0.05 (i.e., we are looking for failure to reject H0 of equality). If invariance holds, NMI behaves as theory predicts: it depends only on V_∩ = S and is unaffected by the community origin of exclusive nodes. If invariance fails, there is an implementation bug or an unmodelled confound, and no downstream result can be trusted until it is explained.

2. **Plateau behaviour:** Median NMI in sub-1A is ≈ 1.0 across the full plateau (by construction; §8.4). Median NMI in sub-1B reaches a stable plateau value strictly between 0 and 1 once V_∩ exceeds the warm-up threshold, with that plateau approximately independent of α. The substantive claim is that V_∩-restricted NMI measures structural *agreement* on shared nodes, not overlap quantity, and therefore plateaus rather than rising smoothly.

3. **Warm-up threshold:** There exists a minimum Jaccard overlap below which NMI is unstable or indistinguishable from zero (V_∩ is too small to estimate the contingency table reliably). Identifying this threshold — and the |V_∩| / edge-density combination at which it stabilises — is a scientific finding in its own right and bounds the regime in which the measure is usable.

4. **Topology sensitivity:** The MI-overlap curves are qualitatively different for SBM, BA, and ER (different shapes, slopes, or saturation behaviour). This confirms the measure is sensitive to topology, not just overlap count.

5. **Method comparison:** Induced and random walk extraction produce qualitatively similar curves (same trends, possibly shifted). Divergence is also a finding worth reporting.

**Note on dropped criterion.** A previous version of this plan included a criterion that NMI should *separate* SAME from DIFF. That criterion was incorrect: under the V_∩-restricted definition of NMI (§5.3) and the node-selection protocol (§4.1.4), SAME and DIFF differ only in the community origin of R_A and R_B, which are disjoint from V_∩ by construction. NMI cannot distinguish the two in principle. The current criterion 1 (invariance) is the logically correct test of this aspect of the design, and a passing invariance result is itself a publishable finding about the measure's behaviour. A separate experiment, with a measure that spans V_A ∪ V_B rather than V_∩, would be required to test the original separation hypothesis — that is left as future work.

If criterion 1 holds, the experiment justifies the use of NMI as a V_∩-specific structural similarity measure in the INETLAB system. If it fails, the implementation must be re-examined before any other conclusion is drawn.

---

## 11. Recommended Execution Order

Do not run the full matrix at once. Build up incrementally:

**Phase 0 — Implement and validate (days 1-3):** Implement all four measures. Run validation checks (Section 8) on N=500 SBM only. Fix all bugs before proceeding.

**Phase 1 — Invariance check (days 4-7):** Run induced extraction, sub-experiment 1B, SBM N=1000, SAME vs DIFF. This produces the single most important plot. If SAME and DIFF curves overlap (NMI/DC-NMI invariant to the community origin of R_A, R_B): proceed. If they separate visibly: stop and investigate — likely a bug in V_∩ restriction, edge-intersection counting, or contingency table construction.

**Phase 2 — Sub-experiment comparison (days 8-10):** Add sub-experiment 1A on the same configuration. Compare 1A and 1B curves. This shows how much edge disagreement on overlap affects MI.

**Phase 3 — Topology sweep (days 11-14):** Run BA and ER at N=1000, induced. Compare curves across topologies.

**Phase 4 — Scale sweep (days 15-18):** Run SBM at N=500 and N=5000. Check that core result holds across scales.

**Phase 5 — Random walk extraction (days 19-25):** Run walk-based extraction on the most promising configurations from Phases 1-4. Compare with induced results.

**Phase 6 — Analysis and writing (days 26-30):** Produce all plots, compute statistical tests, write experimental section of thesis.

---

*End of Experiment Implementation Plan — v1.0 — INETLAB, Keio University*

---

## Appendix A: Formula Congruence Review vs. Felippe et al. (2024)

This appendix records a formula-by-formula verification of §5.3 (NMI) and §5.4 (DC-NMI) against *Felippe, Battiston & Kirkley, "Network mutual information measures for graph similarity"*, Communications Physics 7:335 (2024).

### A.1 NMI — matches paper exactly

- Plan's `I = Hb(p1) + Hb(p2) − Hs(P)` ↔ paper Eq. (9).
- Plan's `P = [p12, p1−p12, p2−p12, 1−p1−p2+p12]` ↔ paper Eq. (7).
- Plan's `NMI = 2I / (Hb(p1) + Hb(p2))` ↔ paper Eq. (10).
- `C = N_∩(N_∩ − 1)/2` is the correct undirected, no-self-loop normalizer (paper §"Network mutual information", paragraph on directed/self-edge adaptations).
- Log base 2 throughout, matching the paper's stated convention.
- Edge-case derivations in §5.3 and §8 are algebraically correct (e.g., α=1 in 1A → p12=p1=p2 → I=Hb(p) → NMI=1).

### A.2 DC-NMI — matches paper exactly

- `p_k(i) = k_k(i) / (N_∩ − 1)` ↔ paper Eq. (14)–(15) (N−1 normalization, correct for graphs without self-edges).
- Per-node `MI_i = Hb(p1_i) + Hb(p2_i) − Hs(P_i)` ↔ summand in paper Eq. (14).
- The plan's optimization "the 1/N_∩ averaging factor cancels in the ratio" is correct: paper's Eq. (16) is `2·I_deg / [(1/N)·Σ(Hb(p1_i)+Hb(p2_i))]`, which equals `2·Σ MI_i / Σ(Hb(p1_i)+Hb(p2_i))` algebraically.
- `k_12(i)` definition (increment both endpoints for each shared edge) matches paper's `k_12(i) = |∂_i^(1) ∩ ∂_i^(2)|`.

### A.3 Jaccard closed form

`Jaccard = α / (2 − α)` verified algebraically from `n_shared = αN/4` and `|V_A ∪ V_B| = N/2 − αN/4`.

### A.4 Adaptation to be explicit about (academic rigor)

The paper (page 3) defines its measures on graphs `G₁, G₂ ⊆ 𝒢` on **the same set of N-labeled nodes** — fully node-aligned. This plan computes NMI/DC-NMI on `V_∩ = V_A ∩ V_B` only. This is a sound extension, not a contradiction: the paper's formulas are applied verbatim to the node-aligned pair `(G_A|_{V_∩}, G_B|_{V_∩})` with N → N_∩, and all derivations carry through unchanged.

However, this is a modeling choice of this experiment, not a result proved in the paper. For academic presentation:

- The invariance property in §4.1.4 / §10 (that NMI depends only on structure within V_∩) is a **tautological consequence of the V_∩-restriction**, not a theorem from Felippe et al. The §10 invariance test should therefore be framed as a check of **implementation correctness** (that the V_∩-restriction is wired up correctly end-to-end), not as a test of a paper claim.
- When citing the NMI/DC-NMI formulas, explicitly note the V_∩ restriction as an adaptation for the partially-overlapping setting (the paper assumes full alignment).

### A.5 Notable omission: MesoNMI

The paper's headline contribution for community-structured graphs (SBM) is **MesoNMI** (paper Eq. 30), which is specifically designed to capture mesoscale similarity that NMI/DC-NMI miss when edge positions don't overlap but mesoscale structure does (see paper Fig. 1, where NMI ≈ 0.06 but MesoNMI ≈ 0.94 on the same pair). Since a substantial portion of this experiment is on SBM, MesoNMI is the most theoretically motivated measure for that topology. Its exclusion from v1.0 is defensible (it would add a partition-choice degree of freedom that complicates the invariance test), but the plan should state this scoping decision explicitly so reviewers do not flag it as an oversight.

### A.6 Baselines (D_H1, D_JS)

D_H1 and D_JS are not from this paper — they are independent topology-summary baselines. Their definitions are standard and correctly stated. The plan correctly positions them as measures that should *not* exhibit V_∩ invariance (since they use the whole graph), so the contrast with NMI/DC-NMI is methodologically clean.

### A.7 Summary

| Item | Status |
|---|---|
| NMI formula vs. paper Eq. (9)–(10) | Exact match |
| DC-NMI formula vs. paper Eq. (14)–(16) | Exact match |
| Contingency table construction | Exact match |
| Log base, entropy definitions, normalizations | Match |
| V_∩-restriction | Sound extension — flag explicitly as adaptation |
| MesoNMI | Omitted — defensible, but add scoping justification |
| Invariance "test" framing | Reframe as implementation-correctness check, not paper-theorem test |

The plan is academically defensible. The two recommended edits — explicit framing of the V_∩ adaptation and one sentence justifying MesoNMI exclusion — would close the remaining rigor gaps before thesis submission.

*End of Appendix A.*