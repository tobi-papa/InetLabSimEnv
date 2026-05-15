# Structural Information (1-D Structural Entropy) of a Graph

## Definition

Given an undirected graph G = (V, E), the **structural entropy** is:

$$H_1(G) = -\sum_{i \in V} \frac{d_i}{\text{vol}(G)} \cdot \log_2\!\left(\frac{d_i}{\text{vol}(G)}\right)$$

Where:
- `d_i` — degree of node `i` (number of edges incident to it)
- `vol(G)` — sum of all node degrees = `2 * |E|`
- Convention: `0 * log2(0) = 0` (to handle isolated nodes)

---

## Algorithm

```
FUNCTION structural_entropy(G):

    // Step 1 — compute vol(G)
    vol = 0
    FOR each node i in G:
        vol = vol + degree(i)

    // Step 2 — accumulate entropy
    H = 0
    FOR each node i in G:
        p = degree(i) / vol
        IF p > 0:
            H = H - p * log2(p)

    RETURN H
```

**Complexity:** O(|V|) — a single pass over node degrees.

---

## Notes

- `vol(G) = 2 * |E|` because every edge contributes 1 to each of its two endpoints.
- `p = d_i / vol(G)` is the stationary probability of a random walk landing on node `i`, so H¹(G) is simply the **Shannon entropy of the random-walk stationary distribution**.
- Result is in **bits** (base-2 log). Use natural log for nats.
