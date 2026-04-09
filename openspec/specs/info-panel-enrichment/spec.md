# Info Panel Enrichment

## ADDED Requirements

### Requirement: Graph-Level Internal and External Edge Display
After community detection completes, the statistics section SHALL display internal edge count (intra-community) and external edge count (inter-community) alongside the existing node count, edge count, community count, and modularity score.

#### Scenario: Statistics after rendering
- **WHEN** a graph is loaded and community detection + layout rendering completes
- **THEN** the statistics section shows "Internal Edges: <intra_count>" and "External Edges: <inter_count>" derived from `CommunityResult::intra_edges` and `CommunityResult::inter_edges`

#### Scenario: Before community detection
- **WHEN** a graph is loaded but community detection has not yet run
- **THEN** the internal edges and external edges labels display "—" (em dash placeholder)

### Requirement: Community Selection Detail Enrichment
When a community is selected from the community dropdown, the detail section SHALL display: internal node count, bridge node count, internal edge count, external edge count, and number of linked communities.

#### Scenario: Selecting a community shows full detail
- **WHEN** the user selects "Community 3" from the dropdown
- **THEN** the detail section displays:
  - Internal Nodes: count of nodes assigned to community 3
  - Bridge Nodes: count of nodes in community 3 that have at least one edge to a node in a different community
  - Internal Edges: count of edges where both endpoints belong to community 3
  - External Edges: count of edges where exactly one endpoint belongs to community 3
  - Linked Communities: count of distinct communities connected to community 3 via external edges

#### Scenario: Deselecting community clears detail
- **WHEN** the user selects "— none —" from the community dropdown
- **THEN** the community detail section is cleared (empty text)

#### Scenario: Bridge node detection
- **WHEN** node N in community C has at least one edge to a node in community D where C ≠ D
- **THEN** node N is classified as a bridge node for community C

### Requirement: Node Selection Detail Enrichment
When a node is selected (by click or text input), the detail section SHALL display: the node's community, number of linked internal-community nodes, number of linked nodes in other clusters, and number of distinct linked communities.

#### Scenario: Selecting a node shows enriched detail
- **WHEN** the user selects node N which belongs to community C
- **THEN** the detail section displays:
  - Node ID and name
  - Community ID
  - Internal Neighbors: count of adjacent nodes also in community C
  - External Neighbors: count of adjacent nodes not in community C
  - Linked Communities: count of distinct communities among external neighbors

#### Scenario: Node with no external connections
- **WHEN** node N belongs to community C and all its neighbors are also in community C
- **THEN** "External Neighbors: 0" and "Linked Communities: 0" are displayed

#### Scenario: No community assigned
- **WHEN** a node is selected before community detection has run
- **THEN** the detail section shows node ID and name only, with community-related fields showing "—"
