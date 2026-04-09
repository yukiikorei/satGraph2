## ADDED Requirements

### Requirement: Community Weighted Layout (2D)
A force-directed layout algorithm SHALL operate on quotient graphs where nodes represent communities. Node mass SHALL equal community size. Attraction force between connected communities SHALL be proportional to inter-community edge weight. Repulsion force SHALL be proportional to the product of community masses.

#### Scenario: Layout of 10-community quotient graph
- **WHEN** a quotient graph with 10 communities and inter-community edge weights is laid out using CommunityWeightedLayout
- **THEN** communities with many inter-community edges are positioned closer together, and larger communities occupy more visual space due to repulsion

#### Scenario: Edge weight drives attraction
- **WHEN** communities A and B share 50 edges and communities A and C share 5 edges
- **THEN** A is positioned closer to B than to C

### Requirement: Community Weighted Layout (3D)
A 3D variant of the community weighted layout SHALL extend the same force model into three dimensions, producing Coordinate3DMap output.

#### Scenario: 3D community layout
- **WHEN** the same quotient graph is laid out using CommunityWeighted3DLayout
- **THEN** the spatial relationships mirror the 2D variant but with z-axis separation for communities with few shared edges
