# Core Modules

## Strong Typedefs (`types.hpp`)

Type-safe ID wrappers preventing accidental mixing of different ID namespaces at compile time.

| Type | Tag | Underlying | Sentinel | Invalid Constant |
|------|-----|-----------|----------|-----------------|
| `NodeId` | `NodeIdTag` | `uint32_t` | `UINT32_MAX` | `invalid_node_id` |
| `EdgeId` | `EdgeIdTag` | `uint32_t` | `UINT32_MAX` | `invalid_edge_id` |
| `CommunityId` | `CommunityIdTag` | `uint32_t` | `UINT32_MAX` | `invalid_community_id` |

Template `StrongId<Tag>` provides: `==`, `!=`, `<`, `<=`, `>`, `>=`, `explicit operator uint32_t()`, and `std::hash` specialization.

Namespace: `satgraf::graph`

---

## Node (`node.hpp`)

Represents a variable or literal in the SAT instance graph.

```cpp
enum class Assignment { True, False, Unassigned };
```

| Field | Type | Description |
|-------|------|-------------|
| `id` | `NodeId` | Unique identifier |
| `name` | `std::string` | Variable name (from DIMACS `c` lines or numeric) |
| `edges` | `std::vector<EdgeId>` | Adjacent edge IDs |
| `groups` | `std::vector<std::string>` | Regex-derived group labels |
| `assignment` | `Assignment` | SAT solver truth assignment |
| `activity` | `double` | Variable activity score (VSIDS-like) |
| `appearance_counts` | `std::vector<int>` | Clause appearance histogram |

---

## Edge (`edge.hpp`)

Represents a co-occurrence relationship between two variables in the same clause.

```cpp
enum class EdgeVisibility { Shown, Hidden };
enum class EdgeType { Normal, Conflict };
```

| Field | Type | Description |
|-------|------|-------------|
| `id` | `EdgeId` | Unique identifier |
| `source` / `target` | `NodeId` | Endpoint nodes |
| `bidirectional` | `bool` | Undirected edge flag (default `true`) |
| `weight` | `double` | Clause co-occurrence count |
| `visibility` | `EdgeVisibility` | Rendering filter |
| `type` | `EdgeType` | Normal vs. conflict classification |

---

## Clause (`clause.hpp`)

Stores a single CNF clause as a mapping from `NodeId` to boolean polarity.

| Method | Description |
|--------|-------------|
| `add_literal(NodeId, bool)` | Add literal with polarity |
| `remove_literal(NodeId)` | Remove literal |
| `has_literal(NodeId) -> bool` | Check membership |
| `get_polarity(NodeId) -> bool` | Get polarity of literal |
| `size() -> size_t` | Number of literals |
| `begin()/end()` | Iterate over `(NodeId, bool)` pairs |

---

## Graph (`graph.hpp`)

Templated adjacency-list graph container. Central data structure for the entire pipeline.

```cpp
template<typename NodeT, typename EdgeT>
class Graph {
    // NodeMap = unordered_map<NodeId, NodeT>
    // EdgeMap = unordered_map<EdgeId, EdgeT>
};
```

| Method | Description |
|--------|-------------|
| `createNode(id, name) -> NodeT&` | Create node (no-op if exists) |
| `createEdge(from, to) -> EdgeT&` | Create edge with auto-incremented EdgeId |
| `connect(eid, node)` | Register edge with endpoint node's adjacency list |
| `getNode(id) -> optional<ref<NodeT>>` | Mutable/const lookup |
| `getNodes() / getEdges()` | Direct map access |
| `getClauses()` | Clause storage |
| `removeNode(id)` | Remove node + all incident edges |
| `removeEdge(id)` | Remove edge, update both endpoints |
| `addClause() -> Clause&` | Append empty clause |
| `nodeCount() / edgeCount()` | Counts |

---

## Compressed Sparse Row (`csr.hpp`)

Converts graph adjacency list into CSR format for efficient neighbor iteration in layout algorithms.

CSR layout: `row_offsets_[i]` to `row_offsets_[i+1]` gives the range in `column_indices_` for node `i`.

| Method | Complexity | Description |
|--------|------------|-------------|
| `build(graph)` | O(E log E) | Build from graph, nodes sorted by NodeId |
| `neighbor_count(i)` | O(1) | Via `row_offsets_[i+1] - row_offsets_[i]` |
| `neighbors_begin(i)` | O(1) | Pointer to start of neighbor list |
| `neighbors_end(i)` | O(1) | Pointer past end |
| `num_nodes() / num_edges()` | O(1) | Size queries |

---

## Union-Find (`union_find.hpp`)

Disjoint set with path compression + union by rank. Used for connectivity checks in community detection.

| Method | Complexity | Description |
|--------|------------|-------------|
| `UnionFind(n)` | O(n) | Initialize n singleton sets |
| `find(x)` | O(α(n)) | Path compression find |
| `unite(x, y)` | O(α(n)) | Union by rank |
| `connected(x, y)` | O(α(n)) | Connectivity test |
| `count_components()` | O(1) | Current component count |

α(n) = inverse Ackermann function, effectively constant.

---

## DIMACS Parser (`dimacs_parser.hpp`)

Parses DIMACS CNF files into graph representation. Two modes:

| Mode | Node Mapping | Edge Semantics |
|------|-------------|----------------|
| `VIG` | `NodeId(abs(literal))` | Variables co-occurring in same clause |
| `LIG` | `+X → NodeId(2*X)`, `-X → NodeId(2*X+1)` | Literals co-occurring in same clause |

### Parsing Algorithm

1. `c` lines: Variable naming (`c <id> <name>`) or comments
2. `p cnf <nvars> <nclauses>`: Header validation
3. Clause lines: Integers until `0` terminator
   - For each variable/literal pair in clause: create or increment-weight edge
   - Edge deduplication via canonical `(min, max)` pair
4. Clause storage: `(NodeId, polarity)` pairs per clause
5. Regex grouping: Applied to node names after parse; first match wins

```cpp
class Parser {
    Parser(vector<regex> group_patterns = {}, ProgressCallback = nullptr);
    Graph<Node, Edge> parse(const string& file_path, Mode mode);
};
```

---

## DIMACS Writer (`dimacs_writer.hpp`)

Serializes graph back to DIMACS CNF format.

```
p cnf <node_count> <clause_count>
c <var_id> <var_name>        (sorted by var_id)
<literals...> 0              (per clause, sorted by |literal|)
```

---

## Community Node & Graph

### CommunityNode (`community_node.hpp`)

Extends `Node` with community membership:

| Field | Type | Description |
|-------|------|-------------|
| `community_id` | `CommunityId` | Community membership |
| `bridge` | `bool` | True if node connects different communities |

### CommunityGraph (`community_graph.hpp`)

Extends `Graph<CommunityNode, Edge>` with community statistics:

| Method | Description |
|--------|-------------|
| `modularity() / set_modularity(q)` | Modularity score Q |
| `communitySizes()` | Community ID → member count |
| `rebuild_community_stats()` | Recompute all statistics |
| `inter_community_edge_count()` | Edges between communities |
| `intra_community_edge_count()` | Edges within communities |
| `compute_community_stats()` | Returns `{min_size, max_size, mean_size, sd_size}` |
