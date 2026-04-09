## MODIFIED Requirements

### Requirement: Info Panel
The GUI SHALL display an info panel with graph statistics: node count, edge count, clause count, community statistics (when available), internal edge count (intra-community), and external edge count (inter-community). Community selection SHALL display internal nodes, bridge nodes, internal edges, external edges, and linked communities. Node selection SHALL display the node's community, internal neighbors, external neighbors, and linked communities. A text input SHALL allow node selection by ID or name.

#### Scenario: Basic statistics display
- **WHEN** a graph with 500 nodes, 1200 edges, and 400 clauses is loaded
- **THEN** the info panel shows "Nodes: 500", "Edges: 1200", "Clauses: 400"

#### Scenario: Community statistics
- **WHEN** community detection produces 4 communities with Q = 0.42, 800 intra-community edges and 400 inter-community edges
- **THEN** the info panel shows "Communities: 4", "Modularity: 0.420", "Internal Edges: 800", "External Edges: 400"

#### Scenario: Community detail on selection
- **WHEN** the user selects community 2 which has 120 nodes (15 of which are bridge nodes), 200 internal edges, 80 external edges connecting to 3 other communities
- **THEN** the community detail section shows: "Internal Nodes: 120", "Bridge Nodes: 15", "Internal Edges: 200", "External Edges: 80", "Linked Communities: 3"

#### Scenario: Node detail on selection
- **WHEN** the user selects node 42 in community 3 which has 5 neighbors in community 3, 2 neighbors in other communities spanning 2 distinct communities
- **THEN** the node detail section shows: "Node 42", "Community: 3", "Internal Neighbors: 5", "External Neighbors: 2", "Linked Communities: 2"

#### Scenario: Node text search
- **WHEN** the user types "42" in the node search field and presses Enter
- **THEN** node 42 is selected and highlighted, and the node detail section updates with enriched information
