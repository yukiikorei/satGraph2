# Algorithms

## Community Detection

### Modularity

All algorithms optimize Newman-Girvan modularity Q:

```
two_m = 2 × |E|
sum_intra = count of edges where both endpoints in same community
sum_expected = Σ_c (degree_sum(c))²
Q = (2 × sum_intra − sum_expected / two_m) / two_m
```

Q ranges from −0.5 to 1.0. Higher is better; Q > 0.3 indicates significant community structure.

### Louvain (`"louvain"`)

Multi-level modularity optimization via igraph `igraph_community_multilevel`.

**Algorithm:**
1. **Phase 1 — Local Moving**: Each node moves to the community of its neighbor that yields the largest modularity gain. Repeated until no improvement.
2. **Phase 2 — Aggregation**: Communities become super-nodes; edges weighted by inter-community edge count.
3. **Repeat** phases 1–2 until convergence.

**Complexity**: O(N log N) typical, O(N²) worst case.

**Fallback**: If the graph is disconnected (checked via `UnionFind`), automatically falls back to CNM.

### Clauset-Newman-Moore (`"cnm"`)

Greedy agglomerative hierarchical clustering via igraph `igraph_community_fastgreedy`.

**Algorithm:**
1. Start with each node in its own community.
2. Build a max-heap of modularity gains for all community pair merges.
3. Iteratively merge the pair with highest gain.
4. Scan the full dendrogram to find the merge index with maximum modularity.

**Complexity**: O(E × d × log N) where d is dendrogram depth.

### Online (`"online"`)

Single-pass deterministic community assignment. No igraph dependency.

**Algorithm:**
1. Sort nodes by NodeId.
2. For each node: count community IDs among already-assigned neighbors.
3. Join the community with highest count. If no neighbors assigned, create a new community.

**Complexity**: O(V + E) — single pass.

---

## Layout Algorithms

All layouts implement `Layout::compute(graph, progress_callback) -> CoordinateMap`.

### Fruchterman-Reingold (`"f"`)

Spring-embedder model. Nodes repel all others; edges attract connected nodes.

**Forces:**

| Force | Formula | Applied to |
|-------|---------|-----------|
| Repulsion | `F_repel = k² / d` (along displacement vector) | All node pairs |
| Attraction | `F_attract = d² / k` (along displacement vector) | Edge-connected pairs |

**Optimal distance**: `k = k_scale × √(area / n)`

**Cooling**: Linear temperature decay from `t₀ = 0.1 × max(w,h)` to 0.

**Complexity**: O(I × (N² + E)) — the O(N²) repulsion is the bottleneck.

**Parameters**: `iterations` (default 500), `k_scale` (default 0.46), canvas size.

---

### GPU Fruchterman-Reingold (`"fgpu"`)

Same FR physics executed on GPU via OpenCL 1.2. Six kernels per iteration:

| Kernel | Work Items | Purpose |
|--------|-----------|---------|
| `fr_repel` | N×(N−1)/2 | Compute repulsive force per unique pair |
| `fr_repel_aggregate` | N | Sum pair displacements into per-node |
| `fr_attract` | E | Compute attractive force per edge |
| `fr_attract_aggregate1` | E | Scatter attraction into per-node chunks |
| `fr_attract_aggregate2` | N | Reduce chunks into per-node displacement |
| `fr_adjust` | N | Apply displacement with temperature clamping |

**Cooling**: After 30 iterations, temperature /= 1.1 per pass. Stops when temp < 1.0.

**Fallback**: On any OpenCL failure (no platform, no GPU, compile error), transparently falls back to CPU FR.

**Complexity**: O(I × (N²/parallelism + E)) on GPU.

---

### ForceAtlas2 (`"forceAtlas2"`)

Improved force-directed layout with Barnes-Hut approximation and adaptive speed.

**Forces:**

| Force | Formula | Note |
|-------|---------|------|
| Repulsion | `F_repel = scaling_ratio × mass / d²` | Barnes-Hut QuadTree when N ≥ 32 |
| Attraction | `weight^α × d` (linear) or `weight^α × log(1+d)` (lin-log) | Per edge |
| Gravity | `F_grav = gravity × 0.01 × (pos − center)` | Prevents drift |

**Barnes-Hut**: QuadTree aggregates distant nodes. Opening criterion: `size/distance < theta` (default 1.2).

**Adaptive speed** (prevents oscillation):
```
swinging(node) = |displacement − old_displacement|
traction(node) = |displacement + old_displacement| / 2
target_speed = max((Σ traction + ε) / (Σ swinging + ε), 0.01)
per_node_factor = speed / (1 + speed × √(swinging + ε))
```

**Multi-threaded**: Repulsion and attraction parallelized via `std::thread` pool.

**Complexity**: O(I × (N log N + E)) with Barnes-Hut.

---

### Kamada-Kawai (`"kk"`)

Energy-based layout using graph-theoretic distances.

**Algorithm:**
1. Compute connected components via DFS.
2. For each component: Floyd-Warshall all-pairs shortest paths (O(N³)).
3. Set ideal spring length: `L₀ = canvas_size / diameter`.
4. Spring force between all pairs: `F = k × (1 − L₀ × d_path / d_euclidean)`.
5. Gradient descent with linear learning rate decay.

**Complexity**: O(C × (N³ + I × N²)) where C = number of components.

---

### Community-Aware Layouts

Three meta-layouts that combine a community placement strategy with a per-community layout:

| Layout | Key | Community Placement | Per-Community Layout |
|--------|-----|---------------------|---------------------|
| Circular | `"c"` | Circle (`R = 35% × min(w,h)`) | FR (150 iters) |
| Grid | `"grid"` | Grid (`cols = ⌈√count⌉`) | FR (150 iters) |
| GridKK | `"gkk"` | Grid | Kamada-Kawai (120 iters) |
| CommunityFA2 | — | ForceAtlas2 on quotient graph | FR (120 iters) |

**Quotient graph**: One node per community, edges weighted by inter-community edge count.

**Two-level compose**:
1. Run layout on each community's induced subgraph independently.
2. Offset local coordinates by community center position.
3. Global rescale to canvas bounds.

---

### Layout Factory

```cpp
auto layout = LayoutFactory::instance().create("forceAtlas2");
auto coords = layout->compute(graph, [](double p) { /* progress */ });
```

Registered keys: `"f"`, `"fgpu"`, `"forceAtlas2"`, `"kk"`, `"c"`, `"grid"`, `"gkk"`.
