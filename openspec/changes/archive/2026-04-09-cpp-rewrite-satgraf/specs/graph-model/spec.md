# Graph Model

Core graph data structures that underpin all satGraf operations: nodes, edges, clauses, adjacency storage, connectivity queries, community extensions, and DIMACS output.

## ADDED Requirements

### Requirement: Template-Based Graph Interface
The graph SHALL be implemented as a class template `Graph<Node, Edge, Clause>` parameterized on node, edge, and clause types. This allows the same graph mechanics to serve both base and community-extended use cases.

#### Scenario: Instantiate base graph
- **WHEN** `Graph<Node, Edge, Clause>` is instantiated
- **THEN** it provides `createNode`, `createEdge`, `connect`, `getNode`, `getNodes`, `getEdges`, `getClauses`, `removeNode`, and `removeEdge` operations

#### Scenario: Instantiate community graph
- **WHEN** `Graph<CommunityNode, ...>` is instantiated with community-aware node types
- **THEN** all standard graph operations are available, plus community-specific queries

### Requirement: Node Data
Each node SHALL carry an ID, a display name, an adjacency list of incident edges, a set of group labels, an assignment state (true, false, or unassigned), an activity counter, and appearance count statistics.

#### Scenario: Node creation and defaults
- **WHEN** a node is created with ID 7 and name "solver_backtrack_count"
- **THEN** its assignment state is "unassigned", activity counter is 0, appearance counts are empty, and group set is empty

#### Scenario: Assignment state update
- **WHEN** node 7's assignment is set to true
- **THEN** subsequent reads of node 7's assignment return true

#### Scenario: Activity counter increment
- **WHEN** node 7's activity counter is incremented 5 times
- **THEN** `node.activity()` returns 5

### Requirement: Edge Data
Each edge SHALL connect exactly two nodes, be bidirectional, and carry a weight, a visibility state (shown or hidden), a type (normal or conflict), and a degrees counter.

#### Scenario: Edge creation
- **WHEN** an edge is created between node 3 and node 5
- **THEN** the edge appears in the adjacency lists of both node 3 and node 5, with default weight 1.0, visibility "shown", and type "normal"

#### Scenario: Edge weight increment
- **WHEN** the same variable pair co-occurs in multiple clauses
- **THEN** the connecting edge's weight increases by 1 for each additional shared clause

#### Scenario: Conflict edge type
- **WHEN** an edge is marked as type "conflict" during solver evolution
- **THEN** the edge's type reads "conflict" and renderers can style it distinctly from normal edges

#### Scenario: Edge visibility toggle
- **WHEN** an edge's visibility is set to hidden
- **THEN** the edge remains in the graph structure but is excluded from rendering and layout calculations

### Requirement: Clause Representation
Each clause SHALL be represented as a set of (NodeId, polarity) pairs, where polarity is a boolean (true for positive, false for negated). The implementation SHALL use `std::unordered_map<NodeId, bool>` for storage.

#### Scenario: Clause creation
- **WHEN** clause `{ (1, true), (3, false), (5, true) }` is created
- **THEN** iterating the clause yields exactly those three literal pairs

#### Scenario: Clause literal lookup
- **WHEN** the code queries whether node 3 appears in a clause with negated polarity
- **THEN** the lookup returns true in O(1) average time via the unordered_map

#### Scenario: Clause modification during evolution
- **WHEN** a literal is added to or removed from a clause
- **THEN** the clause's internal map is updated accordingly

### Requirement: CSR Adjacency Storage
The graph SHALL support Compressed Sparse Row (CSR) adjacency representation for performance-critical paths such as community detection and layout.

#### Scenario: CSR construction
- **WHEN** CSR adjacency is requested from a graph with 100 nodes and 300 edges
- **THEN** a CSR structure is built with `row_offsets` of size 101 and `column_indices` of size 600 (bidirectional), allowing O(1) neighbor count lookup and cache-friendly iteration

#### Scenario: CSR consistency after graph mutation
- **WHEN** a node or edge is removed from the graph after CSR construction
- **THEN** the CSR is invalidated and MUST be rebuilt before the next use

### Requirement: Union-Find Connectivity
The graph SHALL provide a Union-Find (disjoint set union) data structure for efficient connectivity queries.

#### Scenario: Connected components query
- **WHEN** the graph has nodes {1,2,3,4,5} with edges (1,2), (2,3), (4,5)
- **THEN** a connectivity query reports two components: {1,2,3} and {4,5}

#### Scenario: Dynamic edge addition
- **WHEN** edge (3,4) is added to the graph above
- **THEN** the Union-Find structure merges the two components into one: {1,2,3,4,5}

### Requirement: Graph Operations
The graph SHALL expose the following operations: `createNode(id, name)`, `createEdge(from, to)`, `connect(edge, node)`, `getNode(id)`, `getNodes()`, `getEdges()`, `getClauses()`, `removeNode(id)`, `removeEdge(id)`.

#### Scenario: Create and retrieve node
- **WHEN** `createNode(42, "x42")` is called followed by `getNode(42)`
- **THEN** the returned node has ID 42 and name "x42"

#### Scenario: Remove node cascades to edges
- **WHEN** `removeNode(5)` is called and node 5 had edges to nodes 3 and 7
- **THEN** those edges are also removed from the graph and from the adjacency lists of nodes 3 and 7

#### Scenario: Remove edge leaves nodes intact
- **WHEN** `removeEdge(e)` is called where e connects nodes 10 and 20
- **THEN** nodes 10 and 20 remain in the graph but their adjacency lists no longer contain edge e

### Requirement: CommunityNode Extension
`CommunityNode` SHALL extend the base `Node` type with a community ID field and a boolean bridge flag indicating whether the node connects multiple communities.

#### Scenario: Community assignment
- **WHEN** a CommunityNode is assigned community ID 3
- **THEN** `node.community_id()` returns 3

#### Scenario: Bridge node detection
- **WHEN** node 5 has edges to community 1 and community 2
- **THEN** node 5's bridge flag is set to true

#### Scenario: Non-bridge node
- **WHEN** node 7 has edges only within community 1
- **THEN** node 7's bridge flag remains false

### Requirement: CommunityGraph Extension
`CommunityGraph` SHALL extend `Graph` with community-level statistics: Q modularity score, community size distribution (min, max, mean, standard deviation), inter-community edge count, intra-community edge count, and inter/intra edge ratio.

#### Scenario: Community statistics after detection
- **WHEN** community detection assigns 100 nodes to 4 communities
- **THEN** CommunityGraph reports Q modularity, each community's size, inter-community edge count, intra-community edge count, and the ratio between them

#### Scenario: Edge ratio calculation
- **WHEN** a graph has 80 intra-community edges and 20 inter-community edges
- **THEN** the inter/intra ratio is reported as 0.25

### Requirement: DIMACS Output
The graph SHALL support writing itself back to DIMACS CNF format, producing a valid file that can be re-parsed.

#### Scenario: Round-trip fidelity
- **WHEN** a DIMACS file is parsed into a graph and then written back to DIMACS format
- **THEN** the output file contains the same variable count, clause count, variable names (as c-lines), and clause literals as the original

#### Scenario: Variable names preserved
- **WHEN** nodes have display names assigned
- **THEN** the DIMACS output includes `c <id> <name>` lines for all named nodes
