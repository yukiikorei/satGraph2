## ADDED Requirements

### Requirement: 3D Coordinate Type
The layout engine SHALL support 3D coordinates with x, y, and z components, in addition to the existing 2D Coordinate type.

#### Scenario: 3D coordinate creation
- **WHEN** a layout algorithm produces 3D coordinates
- **THEN** each coordinate has x, y, and z double-precision floating point values

### Requirement: ForceAtlas2 3D Layout
The layout engine SHALL provide a ForceAtlas2-based 3D layout algorithm that generalizes the 2D ForceAtlas2 repulsion, attraction, and gravity forces to three dimensions.

#### Scenario: 3D layout computation
- **WHEN** a quotient graph with 20 community nodes and 30 inter-community edges is provided to the 3D layout
- **THEN** the algorithm produces a `Coordinate3DMap` with 20 entries, where connected communities are positioned closer together and disconnected communities are pushed apart in all three dimensions

#### Scenario: 3D layout convergence
- **WHEN** the 3D ForceAtlas2 layout runs for 200 iterations on a small graph
- **THEN** the node positions stabilize (displacement per iteration decreases) similar to the 2D variant

#### Scenario: Barnes-Hut optimization in 3D
- **WHEN** a large quotient graph (100+ communities) is laid out in 3D
- **THEN** the Barnes-Hut optimization extends to an octree structure for O(n log n) repulsion computation

### Requirement: Layout Factory Extension
The `LayoutFactory` SHALL register the new 3D layout algorithm so it can be selected programmatically.

#### Scenario: 3D layout available
- **WHEN** `LayoutFactory::available_algorithms()` is called
- **THEN** "fa3d" (or similar key) appears in the list alongside existing 2D algorithms
