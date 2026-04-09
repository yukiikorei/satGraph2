# Community Detection

Detects community structure in SAT instance graphs using multiple algorithms, producing community assignments and modularity statistics.

## ADDED Requirements

### Requirement: Louvain Algorithm
The system SHALL provide the Louvain community detection algorithm via igraph's `igraph_community_louvain`. It SHALL return a partition (community assignment per node) and the Q modularity score.

#### Scenario: Louvain on connected graph
- **WHEN** Louvain is run on a connected graph with clear community structure
- **THEN** each node is assigned a community ID, and the Q modularity score is a positive value between 0 and 1

#### Scenario: Louvain modularity threshold
- **WHEN** Louvain completes on a random graph with no community structure
- **THEN** the Q modularity score is near 0, indicating no meaningful partition

### Requirement: CNM Algorithm
The system SHALL provide the Clauset-Newman-Moore (CNM) algorithm via igraph's `igraph_community_fastgreedy`, producing a hierarchical clustering and allowing extraction of partitions at any level.

#### Scenario: CNM produces partition
- **WHEN** CNM is run on a graph with 200 nodes
- **THEN** a dendrogram is produced, and extracting a flat partition yields community assignments with a corresponding Q modularity score

#### Scenario: CNM versus Louvain consistency
- **WHEN** both CNM and Louvain are run on the same well-structured graph
- **THEN** both produce valid community assignments, though the number of communities may differ between algorithms

### Requirement: Online Community Detection
The system SHALL provide a custom online (incremental) community detection algorithm that assigns new nodes to existing communities without recomputing from scratch.

#### Scenario: Incremental node addition
- **WHEN** a new node with edges to nodes in communities 2 and 3 is added
- **THEN** the online algorithm assigns it to the community with the strongest connection (most edges) and updates statistics incrementally

#### Scenario: Community splitting during online detection
- **WHEN** the online algorithm detects that a community has grown too large or too loosely connected
- **THEN** it MAY split the community and reassign nodes within it

### Requirement: Algorithm Factory Registration
Each community detection algorithm SHALL register itself in a factory by a unique name string, allowing runtime selection by name.

#### Scenario: Register and select algorithm
- **WHEN** the user requests algorithm "Louvain" from the factory
- **THEN** the factory returns a Louvain detector instance ready to run

#### Scenario: Unknown algorithm name
- **WHEN** the user requests "bogus_algorithm" from the factory
- **THEN** the factory throws or returns an error indicating the algorithm is not registered

#### Scenario: List available algorithms
- **WHEN** the factory is queried for all registered names
- **THEN** it returns at minimum {"Louvain", "CNM", "Online"}

### Requirement: Community Assignment Output
Every community detection algorithm SHALL produce: (1) a mapping of node ID to community ID, (2) a Q modularity score, and (3) community size statistics (min, max, mean, and standard deviation of community sizes).

#### Scenario: Output structure
- **WHEN** Louvain completes on a 500-node graph yielding 5 communities of sizes {80, 120, 60, 150, 90}
- **THEN** the output reports min=60, max=150, mean=100, SD=~33.17, Q modularity, and the full node-to-community map

#### Scenario: Single community
- **WHEN** detection yields only one community containing all nodes
- **THEN** min=max=mean=total nodes, SD=0, and Q modularity is 0

### Requirement: Inter and Intra Community Edge Tracking
Each community object SHALL track the number of edges fully contained within the community (intra) and the number of edges crossing community boundaries (inter).

#### Scenario: Edge classification
- **WHEN** community detection produces communities {A: nodes 1-5, B: nodes 6-10} and edges (1,3), (6,8), (3,7) exist
- **THEN** community A has 1 intra edge (1,3), community B has 1 intra edge (6,8), and there is 1 inter edge (3,7)

#### Scenario: Edge ratio per community
- **WHEN** community A has 10 intra edges and 3 inter edges
- **THEN** community A's inter/intra ratio is reported as 0.3

### Requirement: Disjoint Graph Handling
Louvain cannot operate on disjoint (disconnected) graphs. The system SHALL detect disjoint graphs before running Louvain and either warn the user or fall back to a compatible algorithm.

#### Scenario: Disjoint graph warning
- **WHEN** Louvain is requested on a graph with 3 connected components
- **THEN** the system emits a warning that Louvain does not support disjoint graphs and suggests running CNM or online detection instead

#### Scenario: Automatic fallback
- **WHEN** Louvain is requested on a disjoint graph and auto-fallback is enabled
- **THEN** the system runs CNM instead and notes the fallback in its output

#### Scenario: Connected graph passes through
- **WHEN** Louvain is requested on a connected graph
- **THEN** no warning is emitted and Louvain runs normally

### Requirement: Community Statistics Aggregation
The system SHALL compute aggregate statistics across all communities: total inter-community edges, total intra-community edges, and the global inter/intra ratio.

#### Scenario: Global statistics
- **WHEN** a graph has communities A (20 intra, 5 inter) and B (30 intra, 5 inter)
- **THEN** global intra = 50, global inter = 10 (each inter edge counted once), global ratio = 0.2
