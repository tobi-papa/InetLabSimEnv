#!/usr/bin/env python3
"""
Generate golden reference values for Experiment A entropy tests.

Computes H1, H_P(single-block), and H2_upper_bound (= H1) for a small set of
hand-checkable graphs using the same log2 formulas as the C++ implementation.
Outputs tests/golden/reference_values.json (relative to sandbox root).
"""

import json
import math
import os
import sys


def compute_H1(edges, num_nodes):
    """H1 = -sum_i (d_i/2m) * log2(d_i/2m). Returns 0 if no edges."""
    degree = [0] * num_nodes
    for u, v in edges:
        degree[u] += 1
        degree[v] += 1
    m = len(edges)
    if m == 0:
        return 0.0
    total_volume = 2.0 * m
    H = 0.0
    for d in degree:
        if d == 0:
            continue
        p = d / total_volume
        H -= p * math.log2(p)
    return H


def compute_HP(edges, num_nodes, partition):
    """
    H_P = -sum_j (g_j/2m)*log2(V_j/2m) - sum_j sum_{i in X_j} (d_i/2m)*log2(d_i/V_j)
    Uses FULL graph degree d_i (not internal).
    partition: list of length num_nodes, partition[i] = community index of node i.
    """
    degree = [0] * num_nodes
    for u, v in edges:
        degree[u] += 1
        degree[v] += 1

    m = len(edges)
    if m == 0:
        return 0.0
    total_volume = 2.0 * m

    communities = set(partition)
    V_j = {c: 0.0 for c in communities}
    g_j = {c: 0.0 for c in communities}

    for i, c in enumerate(partition):
        V_j[c] += degree[i]

    for u, v in edges:
        if partition[u] != partition[v]:
            g_j[partition[u]] += 1.0
            g_j[partition[v]] += 1.0

    H = 0.0
    nodes_in = {c: [] for c in communities}
    for i, c in enumerate(partition):
        nodes_in[c].append(i)

    for c in communities:
        Vj = V_j[c]
        if Vj == 0.0:
            continue
        gj = g_j[c]
        if gj > 0.0:
            H -= (gj / total_volume) * math.log2(Vj / total_volume)
        for i in nodes_in[c]:
            di = float(degree[i])
            if di == 0.0:
                continue
            H -= (di / total_volume) * math.log2(di / Vj)
    return H


GRAPHS = {
    "path_4": {
        "num_nodes": 4,
        "edges": [(0, 1), (1, 2), (2, 3)],
    },
    "cycle_4": {
        "num_nodes": 4,
        "edges": [(0, 1), (1, 2), (2, 3), (3, 0)],
    },
    "K4": {
        "num_nodes": 4,
        "edges": [(i, j) for i in range(4) for j in range(i + 1, 4)],
    },
    "two_triangles_bridge": {
        "num_nodes": 6,
        # 0-1-2-0 (left triangle), bridge 2-3, 3-4-5-3 (right triangle)
        "edges": [(0, 1), (1, 2), (0, 2), (2, 3), (3, 4), (4, 5), (3, 5)],
    },
}


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    sandbox_root = os.path.dirname(os.path.dirname(script_dir))
    out_dir = os.path.join(sandbox_root, "tests", "golden")
    os.makedirs(out_dir, exist_ok=True)
    out_path = os.path.join(out_dir, "reference_values.json")

    reference = {}
    for name, spec in GRAPHS.items():
        n = spec["num_nodes"]
        edges = spec["edges"]
        partition_single_block = [0] * n

        H1 = compute_H1(edges, n)
        HP_sb = compute_HP(edges, n, partition_single_block)

        # Single-block H_P should equal H1 by construction
        assert abs(H1 - HP_sb) < 1e-12, (
            f"{name}: H1={H1} but HP_single_block={HP_sb}"
        )

        reference[name] = {
            "H1": H1,
            "HP_single_block": HP_sb,
            # H2_upper_bound: single-block is always a candidate, so H2_est <= H1
            "H2_upper_bound": H1,
        }

        print(f"{name}: H1={H1:.10f}  HP_sb={HP_sb:.10f}")

    with open(out_path, "w") as f:
        json.dump(reference, f, indent=2)

    print(f"\nWrote {out_path}")


if __name__ == "__main__":
    main()
