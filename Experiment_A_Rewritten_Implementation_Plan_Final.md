# Experiment A - Does Community Structure Matter for Gain?

**Final Anti-Artifact Implementation Specification**  
Kaneko Laboratory / INETLAB / Keio University  
Status: revised pre-implementation specification for real, non-forced results  
Date: May 2026

## 0. What changed in this final revision

This revision is designed specifically to avoid the failure mode where the experiment accidentally builds the desired conclusion into the metric, graph generator, or analysis procedure.

The main changes are:

1. `gain_true` is renamed to `gain_oracle_est`, because the 2D entropy minimum is approximated by heuristic partition search.
2. The exact 1D diagnostic baseline is now `H1(G_merge) - H1(G_A)` computed directly on the actual merged graph. The main experiment no longer modifies graph generation just to make a degree-histogram summary exact.
3. `G_A` and `G_B` are generated independently. Duplicate shared-shared edges are allowed and handled by graph union. They are recorded, not artificially removed.
4. The `mu_B` grid is extended up to the ER-like region around 0.75. The old grid ending at 0.50 only tested strong-to-moderate modularity.
5. Absolute error is no longer sufficient for conclusions. The experiment must report absolute, relative, and normalized errors, plus diagnostics showing whether the gain magnitude itself scales with modularity.
6. Inference is based on cell-level summaries and clustered/cell bootstrap, not only on 360 pooled trial points.
7. A degree-preserving rewired null control is added as a required anti-artifact diagnostic.
8. The single-block partition is always injected into candidate partitions so the estimated H2 cannot exceed H1 because of missing candidates.
9. SMI and non-negativity checks are diagnostics with tolerance, not hard proof obligations.
10. The conclusion is softened from "K-dimensional entropy is necessary" to "2D structural entropy is necessary under the tested conditions" if the evidence supports it.

## 1. Purpose of Experiment A

Experiment A tests whether a degree-only structural entropy estimate is enough to estimate Alice's merge gain, or whether a 2D community-aware structural entropy estimate is necessary.

The experiment must answer this question:

> When Bob's graph has stronger realized community structure, does a 1D degree-only estimate become worse at estimating the 2D structural gain Alice would obtain from merging?

The experiment must not be designed to force a positive answer.

A valid result can be:

- positive: 2D structural entropy is needed under the tested conditions;
- negative: degree-only entropy is adequate under the tested conditions;
- mixed: compact summaries fail, but full degree information works;
- ambiguous: the measured effect is confounded by gain magnitude, graph size, overlap, optimization error, or sampling noise.

All four outcomes are valid. The implementation must report the actual outcome.

## 2. Core graphs and target quantity

Alice owns:

```text
G_A = (V_A, E_A)
```

Bob owns:

```text
G_B = (V_B, E_B)
```

Shared nodes:

```text
V_shared = V_A ∩ V_B
```

Merged graph:

```text
G_merge = (V_A ∪ V_B, E_A ∪ E_B)
```

`G_merge` is a simple graph union:

- no self-loops;
- no multi-edges;
- if an edge appears in both graphs, it appears once in the union.

The target quantity estimated by the oracle procedure is:

```text
gain_oracle_est = H2_est(G_merge) - H2_est(G_A)
```

Use `H2_est`, not `H2_true`, because `H2` is obtained by heuristic partition search.

Important:

```text
H2_est(G) is an upper bound on the true minimum H2(G), because it is the best value found among candidate partitions.
However, gain_oracle_est is not guaranteed to be an upper or lower bound on the true gain, because it subtracts two approximate quantities.
```

## 3. Entropy definitions

### 3.1 Graph notation

For an undirected simple graph `G = (V,E)`:

```text
n = |V|
m = |E|
total_volume = 2m
d_i = full degree of node i in the whole graph G
```

For a partition `P = {X_1, ..., X_L}`:

```text
X_j = module/community j
V_j = module volume = sum of full degrees d_i for nodes i in X_j
g_j = number of edges with exactly one endpoint in X_j
```

Critical rule:

```text
The node-level H2 term uses full graph degree d_i.
It does not use internal degree inside the community.
```

### 3.2 1D structural entropy

```text
H1(G) = - sum_i (d_i / 2m) log2(d_i / 2m)
```

If `m = 0`, define `H1(G) = 0`.

Pseudocode:

```text
function compute_H1(graph):
    m = number_of_edges(graph)
    if m == 0:
        return 0.0

    total_volume = 2 * m
    H = 0.0

    for each node i in graph:
        d_i = degree(i)
        if d_i == 0:
            continue
        p_i = d_i / total_volume
        H -= p_i * log2(p_i)

    return H
```

### 3.3 2D structural entropy for a fixed partition

For a fixed flat partition `P`:

```text
H_P(G) = - sum_j (g_j / 2m) log2(V_j / 2m)
         - sum_j sum_{i in X_j} (d_i / 2m) log2(d_i / V_j)
```

Equivalent implementation form:

```text
H_P(G) = - sum_j (g_j / total_volume) log2(V_j / total_volume)
         - sum_j sum_{i in X_j} (d_i / total_volume) log2(d_i / V_j)
```

Correct pseudocode:

```text
function compute_HP(graph, partition):
    m = number_of_edges(graph)
    if m == 0:
        return 0.0

    total_volume = 2 * m
    H = 0.0

    for each module j in partition:
        nodes_j = nodes assigned to module j
        V_j = sum(degree(i) for i in nodes_j)   // full graph degrees

        if V_j == 0:
            continue

        g_j = number of edges with exactly one endpoint in nodes_j

        if g_j > 0:
            H -= (g_j / total_volume) * log2(V_j / total_volume)

        for each node i in nodes_j:
            d_i = degree(i)   // full graph degree, not internal degree
            if d_i == 0:
                continue
            H -= (d_i / total_volume) * log2(d_i / V_j)

    return H
```

Forbidden implementation:

```text
d_i_internal = number of edges from i to other nodes in module j
```

Do not use `d_i_internal` in the node-level term.

### 3.4 2D structural entropy estimate

The mathematical definition is:

```text
H2(G) = min_P H_P(G)
```

The implementation computes:

```text
H2_est(G) = min over candidate partitions P of H_P(G)
```

The candidate set must be large enough to make the oracle estimate credible.

Minimum candidate set:

```text
1. single-block partition: all nodes in one module
2. structural-entropy-minimization candidates from multiple seeds
3. optional Louvain/Leiden candidates, evaluated only by H_P
4. optional random/refined candidate partitions
```

The single-block partition is mandatory because it gives:

```text
H_P(G) = H1(G)
```

Therefore:

```text
H2_est(G) <= H1(G)
```

by construction.

Optional all-singletons partition may also be included as a diagnostic candidate.

Do not choose the oracle partition by modularity. Modularity-based methods may generate candidate partitions, but the selected partition must be the one with lowest `H_P(G)`.

## 4. Oracle search quality requirements

The H2 oracle estimate is the most fragile part of the experiment. The implementation must record enough information to detect poor minimization.

For each graph `G_A`, `G_B`, and `G_merge`, record:

```text
H2_est
H2_best_partition_id
H2_method_of_best_partition
H2_seed_of_best_partition
H2_n_candidates
H2_n_unique_scores
H2_score_best
H2_score_second_best
H2_score_median
H2_score_std
H2_best_minus_second
H2_n_within_1e_minus_6_of_best
H2_n_within_1e_minus_4_of_best
n_communities_best_partition
```

Search effort must scale with graph size. A fixed number of seeds can under-optimize the larger merged graph.

Recommended rule:

```text
base_seeds = 10
seeds_G = max(base_seeds, ceil(number_of_nodes(G) / 50))
```

This gives at least:

```text
G_A, G_B: at least 10 seeds
G_merge: at least 10-11 seeds depending on n_shared
```

If runtime allows, use more seeds. Do not reduce search effort after seeing results.

Fragile oracle warning:

```text
If H2_best_minus_second is very small, this is not automatically bad; many near-tied partitions may exist.
If H2_n_candidates is low, H2_score_std is high, and the best score appears only once, flag the trial as oracle-fragile.
```

## 5. Connectedness rules

The main formulas are used in the connected-graph setting.

Each accepted trial must satisfy:

```text
G_A is connected
G_B is connected
G_merge is connected
```

Use rejection sampling:

```text
Generate G_A. If disconnected, reject and resample.
Generate G_B. If disconnected, reject and resample.
Construct G_merge. If disconnected, reject and resample.
```

Record:

```text
rejected_draws_before_acceptance
rejected_A_disconnected
rejected_B_disconnected
rejected_merge_disconnected
```

Connectivity rejection can change the distribution of accepted graphs. Therefore report rejection counts by `mu_B` and `n_shared`.

Do not silently compute connected-graph entropy on disconnected graphs.

## 6. Graph generation protocol

### 6.1 SBM purpose

Use stochastic block models to generate graphs with controlled community structure.

Each graph is simple, undirected, and unweighted.

### 6.2 Correct meaning of the mixing parameter

Use `mu` as the expected external-degree fraction:

```text
mu = E[external degree] / E[total degree]
```

Do not define it as:

```text
p_out / (p_in + p_out)
```

That definition is inconsistent with the probability conversion below.

### 6.3 Probability conversion

Given:

```text
n = number of nodes
k = number of equal-size communities
avg_degree = target expected average degree
mu = expected external-degree fraction
```

Use:

```text
p_in  = avg_degree * (1 - mu) / (n/k - 1)
p_out = avg_degree * mu       / (n - n/k)
```

Check:

```text
0 <= p_in <= 1
0 <= p_out <= 1
```

### 6.4 Important ER-like point

Under this parameterization, `mu = 0.50` is not necessarily Erdos-Renyi-like.

For equal blocks, the ER-like value is approximately:

```text
mu_ER = (n - n/k) / (n - 1)
```

For `n = 300`, `k = 4`:

```text
mu_ER = 225 / 299 ≈ 0.753
```

Therefore the experiment must include `mu_B` values up to approximately 0.75.

Recommended main grid:

```text
mu_B values = {0.05, 0.075, 0.10, 0.15, 0.20, 0.30, 0.40, 0.50, 0.60, 0.70, 0.75}
```

This gives strong, moderate, weak, and near-ER community regimes.

The old grid ending at 0.50 is insufficient because it does not test near-zero modularity.

### 6.5 Fixed parameters

Default parameters:

```text
n_A = 300
n_B = 300
k_A = 4
k_B = 4
avg_degree_A = 10
avg_degree_B = 10
mu_A = 0.20
n_shared values = {15, 30, 90}
repetitions per configuration = 20
```

With the recommended grid:

```text
11 mu_B values * 3 n_shared values * 20 repetitions = 660 accepted main trials
```

If runtime is too high, reduce the number of `mu_B` values only before running the experiment, not after inspecting results.

### 6.6 Global node labels

Use one global ID space.

```text
V_shared = first n_shared nodes
V_B = V_shared union V_B_only
V_A = V_shared union V_A_only
```

Ensure:

```text
|V_A| = n_A
|V_B| = n_B
|V_A ∩ V_B| = n_shared
|V_A ∪ V_B| = n_A + n_B - n_shared
```

Shared nodes may have different planted community labels in `G_A` and `G_B`. This is intentional.

### 6.7 Primary edge-generation rule

Generate `G_A` and `G_B` independently.

Do not prevent duplicate shared-shared edges in the primary experiment.

Rationale:

```text
Preventing duplicate shared-shared edges changes the joint graph-generation process and can introduce an artifact.
The primary experiment must measure what happens under independent graph generation and true graph union.
```

After union, record edge overlap instead of removing it artificially.

### 6.8 Merged graph edge count

Correct edge-count identity:

```text
|E_merge| = |E_A| + |E_B| - |E_A ∩ E_B|
```

Record separately:

```text
edge_overlap_AB = |E_A ∩ E_B|
shared_shared_edge_overlap = number of overlapping edges whose two endpoints are both in V_shared
```

In this construction, edge overlap can only occur between shared nodes.

## 7. Realized graph statistics

Nominal parameters are not enough. Record realized values for every trial.

Required:

```text
actual_avg_degree_A
actual_avg_degree_B
actual_avg_degree_merge
actual_mu_realized_B
modularity_B_planted
modularity_B_H2_partition
edges_A
edges_B
edges_merge
edge_overlap_AB
shared_shared_edge_overlap
```

Realized external-degree fraction for Bob:

```text
actual_mu_realized_B = number of half-edges in G_B crossing planted communities / total_volume_B
```

Use `modularity_B_planted` as the primary x-variable, not nominal `mu_B`.

Nominal `mu_B` is used for grouping and experimental design. Realized modularity is used for analysis.

## 8. Modularity measurement

For a partition `P`:

```text
Q(G,P) = sum_j [ e_jj / m - (V_j / 2m)^2 ]
```

where:

```text
e_jj = number of edges with both endpoints inside module j
V_j = sum of full degrees of nodes in module j
m = number of edges
```

Record:

```text
modularity_B_planted = Q(G_B, planted_partition_B)
modularity_B_H2_partition = Q(G_B, best_H2_partition_B)
```

Use `modularity_B_planted` as the primary community-strength variable because it corresponds to the controlled SBM structure.

Do not use modularity as the H2 oracle objective.

## 9. Estimators and diagnostics

The experiment must separate three ideas:

1. the 2D oracle-estimated target;
2. the original compact degree summary;
3. the best possible 1D degree-only diagnostic on the actual merged graph.

### 9.1 Target: oracle-estimated 2D gain

```text
gain_oracle_est = H2_est(G_merge) - H2_est(G_A)
```

This is the quantity all 1D methods attempt to approximate.

### 9.2 Estimator A: compact 1D summary

Bob summary:

```text
summary_B_compact = {
    vol_B: 2 * |E_B|,
    degrees_shared: degree of each shared node in G_B
}
```

Summary size:

```text
summary_compact_size = n_shared + 1
```

Approximation:

```text
Start with degrees of G_A.
For shared nodes, add Bob's shared-node degrees.
Collapse all unknown Bob-only volume into one virtual node.
Compute H1 from this approximate degree sequence.
```

Pseudocode:

```text
function compute_gain_1D_compact(G_A, summary_B_compact):
    merged_degrees = degrees of nodes in G_A

    for v in V_shared:
        merged_degrees[v] += summary_B_compact.degrees_shared[v]

    vol_shared_in_B = sum(summary_B_compact.degrees_shared.values())
    vol_unshared_B = summary_B_compact.vol_B - vol_shared_in_B

    if vol_unshared_B < 0:
        flag invalid summary

    if vol_unshared_B > 0:
        merged_degrees["virtual_B"] = vol_unshared_B

    H1_merge_compact = entropy_from_degree_dictionary(merged_degrees)
    gain_1D_compact = H1_merge_compact - H1_A

    return gain_1D_compact, H1_merge_compact
```

Important:

```text
This estimator is intentionally lossy.
Large error here does not by itself prove that all 1D methods fail.
```

### 9.3 Estimator B: oracle 1D degree diagnostic

This is the clean diagnostic for whether 1D entropy itself is sufficient.

```text
gain_1D_oracle_degree = H1(G_merge) - H1(G_A)
```

This uses the actual merged graph and is not privacy-preserving.

It is included to answer:

```text
Even with perfect degree information for the actual merge, does 1D entropy approximate the 2D gain?
```

This diagnostic must not modify graph generation.

### 9.4 Optional Estimator C: degree-histogram summary

A summary-based exact 1D degree estimator can be tested as a secondary experiment, but only if edge-overlap correction is handled explicitly.

Because duplicate shared-shared edges are allowed in the primary experiment, a simple degree histogram plus shared degrees is not always enough to reconstruct `H1(G_merge)` exactly. Duplicate edges between shared nodes affect merged degrees.

Therefore, optional summary-based degree reconstruction must either:

```text
1. include explicit overlap correction information for shared-shared duplicate edges;
```

or

```text
2. be run in a separate robustness experiment where duplicate shared-shared edges are prevented and the distortion is reported.
```

Estimator C is optional. It is not the main anti-artifact diagnostic.

## 10. Error metrics

For each estimator `est` in `{compact, oracle_degree}` compute:

```text
error_est_abs = |gain_oracle_est - gain_est|
```

Relative error:

```text
if |gain_oracle_est| > TOL_GAIN:
    error_est_rel = error_est_abs / |gain_oracle_est|
else:
    error_est_rel = undefined
```

Use:

```text
TOL_GAIN = 1e-9
```

Normalized error by Bob's structural entropy:

```text
if H2_est(G_B) > TOL_GAIN:
    error_est_norm_B = error_est_abs / H2_est(G_B)
else:
    error_est_norm_B = undefined
```

Optional normalized error by oracle 1D scale:

```text
error_est_norm_H1merge = error_est_abs / H1(G_merge)   if H1(G_merge) > TOL_GAIN
```

Do not base the headline conclusion on absolute error alone.

## 11. Anti-artifact controls

This section is mandatory. Its purpose is to prevent the desired conclusion from being built into the metric or graph generator.

### 11.1 Gain-magnitude diagnostic

Compute and plot:

```text
gain_oracle_est vs modularity_B_planted
```

Also compute:

```text
Spearman(modularity_B_planted, gain_oracle_est)
```

If absolute error grows with modularity but gain magnitude also grows strongly, then absolute error alone is not evidence that the estimator is relatively worse.

The result must also be supported by relative or normalized errors.

### 11.2 Exact 1D oracle-degree diagnostic

Compare both:

```text
error_compact_abs
error_oracle_degree_abs
```

Interpretation:

```text
If compact error grows but oracle-degree error does not, the compact summary is the problem, not necessarily 1D entropy.
If oracle-degree error also grows, that supports a real 1D-vs-2D dimension gap.
```

### 11.3 Degree-preserving rewired null control

For each accepted main trial, create at least one null version of Bob's graph:

```text
G_B_null = degree-preserving random rewiring of G_B
```

Use double-edge swaps or another simple-graph degree-preserving method.

Requirements:

```text
G_B_null preserves the degree of every Bob node, including shared nodes.
G_B_null is simple, undirected, and connected.
Use enough swaps to substantially randomize topology, e.g. at least 10 * |E_B| attempted swaps.
Record number of successful swaps.
```

Then construct:

```text
G_merge_null = union(G_A, G_B_null)
```

Compute the same quantities:

```text
H1(G_merge_null)
H2_est(G_B_null)
H2_est(G_merge_null)
gain_oracle_est_null
error_compact_null
error_oracle_degree_null
modularity_B_null_planted
```

Purpose:

```text
This null keeps Bob's degree sequence fixed while reducing or changing community structure.
If the real graph and null graph show similar errors, then the claimed effect may be caused by degree distribution, graph size, or gain magnitude, not community structure.
```

This null is a diagnostic, not a replacement for the main experiment.

Required reporting:

```text
mean(error_real - error_null) by mu_B and n_shared
modularity_real vs modularity_null
```

### 11.4 No post-hoc threshold tuning

Thresholds, grids, metrics, and exclusion rules must be fixed before running the experiment.

Do not change success criteria after seeing plots.

### 11.5 Report negative and ambiguous results

The implementation must not filter out valid trials because they weaken the expected conclusion.

Only reject trials for predeclared reasons:

```text
disconnected graph
invalid probability
failed hard sanity check
failed graph construction invariant
```

Do not reject trials because:

```text
error is too small
modularity is not high enough
trend is weak
result disagrees with hypothesis
```

## 12. Statistical analysis

### 12.1 Raw trial data

Save every accepted trial before aggregation.

Raw trial data are used for diagnostics and plots.

### 12.2 Main inferential unit: cells

Define a cell as:

```text
cell = (mu_B, n_shared)
```

For each cell compute:

```text
cell_mean_modularity_B_planted
cell_mean_gain_oracle_est
cell_mean_error_abs
cell_mean_error_rel
cell_mean_error_norm_B
cell_std_error_abs
cell_n
```

The main trend analysis must use cell-level summaries or clustered resampling, not only pooled trial rows.

### 12.3 Correlations

Compute trial-level correlations as descriptive statistics:

```text
Spearman_trial(modularity_B_planted, error_abs)
Spearman_trial(modularity_B_planted, error_rel)
Spearman_trial(modularity_B_planted, error_norm_B)
```

Compute cell-level correlations as main trend statistics:

```text
Spearman_cell(cell_mean_modularity_B_planted, cell_mean_error_abs)
Spearman_cell(cell_mean_modularity_B_planted, cell_mean_error_rel)
Spearman_cell(cell_mean_modularity_B_planted, cell_mean_error_norm_B)
```

Use cluster/cell bootstrap confidence intervals. Resample cells, not individual rows, for the main CI.

### 12.4 Regression diagnostics

Fit separately for compact and oracle-degree estimators:

```text
error_abs ~ modularity_B_planted + n_shared + gain_oracle_est
error_norm_B ~ modularity_B_planted + n_shared
error_rel ~ modularity_B_planted + n_shared
```

Optional interaction:

```text
error_metric ~ modularity_B_planted * n_shared
```

The coefficient on modularity is only credible if it remains positive after accounting for overlap and gain magnitude.

### 12.5 Null-control comparison

For the rewired null control, compute:

```text
delta_error = error_real - error_null
```

Analyze:

```text
delta_error_abs by mu_B and n_shared
delta_error_norm_B by mu_B and n_shared
```

Interpretation:

```text
If delta_error is consistently positive at high modularity, community structure contributes beyond degree sequence.
If delta_error is near zero, the claimed community effect is weak.
```

## 13. Decision logic

Avoid a single forced pass/fail threshold. Use the following interpretation cases.

### Case A: strong evidence that 2D structure matters

Conditions:

```text
1. compact and oracle-degree errors both increase with realized modularity;
2. the trend appears in absolute and at least one scale-adjusted metric, either relative error or H2_B-normalized error;
3. gain_oracle_est vs modularity does not fully explain the error trend;
4. real graphs show larger errors than degree-preserving rewired null graphs, especially at high modularity;
5. oracle-quality diagnostics do not show systematic degradation for G_merge relative to G_A.
```

Conclusion:

```text
Under the tested SBM conditions, 2D structural entropy captures merge-gain information that degree-only entropy misses.
```

Do not claim this directly proves all K-dimensional entropy is necessary. It supports 2D and motivates higher-dimensional methods.

### Case B: compact summary fails, but oracle-degree 1D works

Conditions:

```text
compact error increases with modularity or is large;
oracle-degree error is small or not systematically related to modularity.
```

Conclusion:

```text
The original compact summary is too lossy. The experiment does not prove that 1D entropy itself is insufficient.
```

### Case C: neither 1D estimator fails clearly

Conditions:

```text
compact and oracle-degree errors are both small or not related to modularity.
```

Conclusion:

```text
The tested conditions do not support the need for 2D structural entropy.
```

This is a valid negative result.

### Case D: ambiguous / confounded

Use this if:

```text
absolute error increases but relative/normalized errors do not;
gain magnitude strongly explains the trend;
null controls show similar errors to real graphs;
oracle quality is fragile or systematically worse for G_merge;
cell-level confidence intervals are wide or cross zero.
```

Conclusion:

```text
Experiment A is inconclusive. More trials, better oracle optimization, or redesigned graph controls are required.
```

## 14. Required plots

### 14.1 Main error plots

For each estimator:

```text
x-axis = modularity_B_planted
y-axis = error_abs
```

and separately:

```text
y-axis = error_rel
y-axis = error_norm_B
```

Show:

```text
raw trial points lightly
cell means prominently
trend line over cell means
n_shared by color or marker
```

### 14.2 Gain magnitude diagnostic

```text
x-axis = modularity_B_planted
y-axis = gain_oracle_est
```

This plot must appear near the main error plot.

### 14.3 Estimator-vs-target plots

For each estimator:

```text
x-axis = gain_oracle_est
y-axis = gain_est
reference line = y = x
```

### 14.4 Real vs rewired null plots

Plot:

```text
x-axis = modularity_B_planted
y-axis = error_real - error_null
```

Use separate panels for compact and oracle-degree estimators.

### 14.5 Error by nominal mu_B

Box/violin plot:

```text
x-axis = mu_B
y-axis = error metric
```

Use this as a design diagnostic, not the main proof.

### 14.6 Modularity coverage plot

Plot distribution of realized modularity by nominal `mu_B`.

Purpose:

```text
Verify that the experiment actually spans high, moderate, weak, and near-zero modularity.
```

## 15. Required trial output fields

Every accepted main trial must output:

```text
trial_id
trial_seed
mu_A
mu_B
n_shared
n_A
n_B
k_A
k_B
avg_degree_A
avg_degree_B
p_in_A
p_out_A
p_in_B
p_out_B

rejected_draws_before_acceptance
rejected_A_disconnected
rejected_B_disconnected
rejected_merge_disconnected

nodes_A
nodes_B
nodes_merge
edges_A
edges_B
edges_merge
edge_overlap_AB
shared_shared_edge_overlap

actual_avg_degree_A
actual_avg_degree_B
actual_avg_degree_merge
actual_mu_realized_B

connected_A
connected_B
connected_merge

H1_A
H1_B
H1_merge

H2_est_A
H2_est_B
H2_est_merge

SMI_est
gain_oracle_est

H1_merge_compact
gain_1D_compact
error_compact_abs
error_compact_rel
error_compact_norm_B

gain_1D_oracle_degree
error_oracle_degree_abs
error_oracle_degree_rel
error_oracle_degree_norm_B

modularity_B_planted
modularity_B_H2_partition

H2_n_candidates_A
H2_n_candidates_B
H2_n_candidates_merge
H2_score_best_A
H2_score_best_B
H2_score_best_merge
H2_score_second_best_A
H2_score_second_best_B
H2_score_second_best_merge
H2_best_minus_second_A
H2_best_minus_second_B
H2_best_minus_second_merge
H2_n_unique_scores_A
H2_n_unique_scores_B
H2_n_unique_scores_merge
H2_n_within_1e_minus_6_A
H2_n_within_1e_minus_6_B
H2_n_within_1e_minus_6_merge
H2_n_within_1e_minus_4_A
H2_n_within_1e_minus_4_B
H2_n_within_1e_minus_4_merge

H2_method_A
H2_method_B
H2_method_merge
H2_seed_A
H2_seed_B
H2_seed_merge
n_communities_H2_A
n_communities_H2_B
n_communities_H2_merge

summary_compact_size

sanity_passed
sanity_warnings
oracle_fragility_warning
```

For each rewired null trial, output the same fields with suffix `_null` or a separate CSV keyed by `trial_id`.

## 16. Sanity checks

### 16.1 Hard checks

Fail or reject if these fail.

Graph sizes:

```text
|V_A| = n_A
|V_B| = n_B
|V_A ∩ V_B| = n_shared
|V_A ∪ V_B| = n_A + n_B - n_shared
```

Connectedness:

```text
G_A connected
G_B connected
G_merge connected
```

Edge union:

```text
edges_merge = edges_A + edges_B - edge_overlap_AB
```

Entropy inequality:

```text
H2_est_A <= H1_A + TOL
H2_est_B <= H1_B + TOL
H2_est_merge <= H1_merge + TOL
```

This should hold because the single-block partition is included.

Compact summary size:

```text
summary_compact_size = n_shared + 1
```

Probability validity:

```text
0 <= p_in <= 1
0 <= p_out <= 1
```

### 16.2 Warning checks

Do not automatically reject for these. Record warnings.

```text
SMI_est < -TOL
```

```text
gain_oracle_est < -TOL
```

```text
oracle_fragility_warning = true
```

```text
realized modularity range is too narrow
```

```text
very high rejection rate for a parameter cell
```

### 16.3 Qualitative diagnostics only

Do not use these as hard trial rejection rules:

```text
H2_B generally decreases as realized modularity increases.
```

Expected sign is negative, but finite-size randomness can create exceptions.

Do not require strict monotonicity per trial.

## 17. Minimal valid experiment

A minimal valid main experiment requires:

```text
660 accepted main trials if using the recommended 11-value mu grid
connected G_A, G_B, and G_merge
correct H1 implementation
correct H_P implementation using full degrees
H2 candidate search with mandatory single-block partition
compact 1D estimator
oracle 1D degree diagnostic
absolute, relative, and normalized error metrics
cell-level statistical analysis
cluster/cell bootstrap confidence intervals
rewired degree-preserving null diagnostic
raw CSV output
plots listed in Section 14
```

If runtime prevents 660 trials, define a smaller grid before running. Do not reduce it after seeing results.

## 18. Do-not-do list

The implementation must not do any of the following:

```text
Do not use internal community degree in the H2 node term.
Do not choose the H2 oracle partition by maximum modularity.
Do not call gain_oracle_est a true exact value.
Do not prevent duplicate shared-shared edges in the primary experiment.
Do not conclude from absolute error alone.
Do not ignore gain_oracle_est vs modularity.
Do not treat 360 or 660 raw rows as fully independent x-axis evidence without cell/cluster analysis.
Do not tune thresholds after seeing the plots.
Do not remove trials because they weaken the expected result.
Do not claim K-dimensional entropy is proven necessary when only K=2 was tested.
Do not treat SMI or gain non-negativity warnings as automatic proof of implementation failure.
Do not use mu_B = 0.50 as the near-ER endpoint.
```

## 19. Final interpretation template

Use this template after running the experiment:

```text
Experiment A tested whether degree-only entropy can estimate Alice's 2D structural merge gain when Bob's graph varies in realized community strength.

The compact 1D estimator [did/did not] show increasing error with realized Bob modularity.

The oracle 1D degree diagnostic [did/did not] show increasing error with realized Bob modularity.

The gain-magnitude diagnostic [did/did not] explain the absolute-error trend.

The degree-preserving rewired null control [did/did not] show comparable errors to the original graphs.

Therefore, the result is classified as [Case A / Case B / Case C / Case D].

This means [2D structural entropy is supported / the compact summary is too lossy / 1D may be sufficient / the experiment is inconclusive] under the tested SBM conditions.
```

## 20. One-sentence final purpose

Experiment A is a bias-controlled test of whether 2D community-aware structural entropy provides merge-gain information beyond degree-only entropy, without forcing the expected conclusion through the error metric, graph-generation constraints, or post-hoc analysis choices.
