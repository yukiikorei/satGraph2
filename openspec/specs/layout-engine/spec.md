# Layout Engine

Force-directed layout algorithms that compute (x, y) coordinates for every node in the graph. Includes CPU and GPU variants, community-aware layouts, and a factory for runtime selection.

## ADDED Requirements

### Requirement: Fruchterman-Reingold CPU Layout
The system SHALL implement the Fruchterman-Reingold (FR) force-directed layout algorithm on CPU. It SHALL run O(n^2) per iteration with a configurable number of iterations (default 500), a cooling schedule that reduces displacement over time, optimal distance calculation based on graph area and node count, and rescaling of final positions to fit within specified bounds.

#### Scenario: Basic FR layout
- **WHEN** FR is applied to a 100-node graph with default 500 iterations and a 1024x1024 bounding box
- **THEN** every node has a unique (x, y) coordinate within [0, 1024] x [0, 1024], no two nodes occupy the same position, and connected nodes tend to be closer than unconnected nodes

#### Scenario: Custom iteration count
- **WHEN** FR is configured with 100 iterations instead of the default 500
- **THEN** the algorithm completes faster but may produce a less optimal layout

#### Scenario: Cooling schedule
- **WHEN** FR starts with temperature T and runs 500 iterations
- **THEN** the temperature decreases each iteration, reaching near-zero by the final iteration, reducing node displacement as the layout stabilizes

#### Scenario: Optimal distance calculation
- **WHEN** FR is run on a 200-node graph in a 1024x1024 area
- **THEN** the optimal distance k is computed as `C * sqrt(area / |V|)` where C is a tuning constant, area = 1024*1024, and |V| = 200

#### Scenario: Rescaling to bounds
- **WHEN** FR completes and some node positions fall outside the target bounding box
- **THEN** all positions are linearly rescaled so the extreme coordinates match the bounding box edges with a configurable margin

### Requirement: Fruchterman-Reingold GPU Layout
The system SHALL implement the FR algorithm parallelized via OpenCL compute kernels. This targets AMD integrated GPUs (no CUDA dependency). The implementation SHALL provide kernels for repulsion computation, attraction computation, force aggregation, and position adjustment.

#### Scenario: GPU FR produces same-quality layout
- **WHEN** GPU FR is run on the same graph as CPU FR with the same parameters
- **THEN** the resulting layout has comparable quality (similar average edge length, similar node spread), though exact coordinates may differ due to floating-point non-determinism

#### Scenario: GPU FR performance scaling
- **WHEN** GPU FR is run on a 10,000-node graph
- **THEN** the GPU implementation completes significantly faster than the CPU O(n^2) implementation on the same hardware

#### Scenario: OpenCL kernel compilation failure
- **WHEN** the OpenCL runtime is unavailable or kernel compilation fails
- **THEN** the system SHALL fall back to CPU FR and emit a warning

#### Scenario: AMD iGPU target
- **WHEN** the system detects an AMD integrated GPU as the only available OpenCL device
- **THEN** the GPU layout uses that device without requiring any CUDA libraries

### Requirement: ForceAtlas2 Layout
The system SHALL implement the ForceAtlas2 algorithm (originating from Gephi) with multi-threaded execution, Barnes-Hut approximation for large graphs (theta = 1.2 by default), auto-adjusted speed, optional lin-log mode, gravity, and configurable edge weight influence.

#### Scenario: Basic ForceAtlas2 layout
- **WHEN** ForceAtlas2 is applied to a 1,000-node graph
- **THEN** every node receives an (x, y) coordinate, and the layout converges to a stable state where connected communities form visible clusters

#### Scenario: Barnes-Hut optimization
- **WHEN** ForceAtlas2 is run on a 50,000-node graph with Barnes-Hut enabled (theta = 1.2)
- **THEN** the algorithm runs in O(n log n) per iteration instead of O(n^2), completing in practical time

#### Scenario: Lin-log mode
- **WHEN** lin-log mode is enabled in ForceAtlas2
- **THEN** the attraction force uses a logarithmic distance function, producing a more compact layout where dense clusters are tighter

#### Scenario: Multi-threaded execution
- **WHEN** ForceAtlas2 is configured with 8 threads on an 8-core machine
- **THEN** repulsion and attraction computations are parallelized across threads, achieving near-linear speedup

#### Scenario: Gravity prevents drift
- **WHEN** ForceAtlas2 is run with gravity enabled
- **THEN** nodes are attracted toward the center of the layout, preventing unconnected components from drifting to infinity

#### Scenario: Edge weight influence
- **WHEN** edge weights are set and edge weight influence is configured to 1.0
- **THEN** heavier edges exert stronger attraction between their endpoint nodes

### Requirement: Kamada-Kawai Layout
The system SHALL implement the Kamada-Kawai force-directed layout as an alternative algorithm based on all-pairs shortest-path distances.

#### Scenario: Kamada-Kawai layout
- **WHEN** Kamada-Kawai is applied to a 200-node connected graph
- **THEN** every node receives an (x, y) coordinate, and the layout reflects the graph-theoretic distance between nodes

#### Scenario: Disconnected graph handling
- **WHEN** Kamada-Kawai is applied to a graph with multiple connected components
- **THEN** the system SHALL handle each component separately and space the components apart in the final layout

### Requirement: Community-Level Circular Layout
The system SHALL provide a circular community layout that places communities in a circle and arranges nodes within each community using FR.

#### Scenario: Circular layout with 5 communities
- **WHEN** the circular layout is applied to a graph with 5 communities
- **THEN** the 5 community centers are placed at equal angles around a circle, and nodes within each community are laid out by FR within their community region

#### Scenario: Single community circular layout
- **WHEN** the circular layout is applied to a graph with 1 community
- **THEN** all nodes are laid out via FR centered at the origin, with no outer circle positioning needed

### Requirement: Community-Level Grid Layout
The system SHALL provide a grid community layout that arranges communities in a 2D grid, with nodes within each community laid out by FR.

#### Scenario: Grid layout with 9 communities
- **WHEN** the grid layout is applied to a graph with 9 communities
- **THEN** communities are arranged in a 3x3 grid, and each community's nodes are laid out via FR within their grid cell

#### Scenario: Non-square community count
- **WHEN** the grid layout is applied to a graph with 7 communities
- **THEN** communities are arranged in a grid that approximates a square (e.g., 3x3 with 2 empty cells)

### Requirement: Community-Level Grid+KK Hybrid Layout
The system SHALL provide a hybrid layout that uses a grid for community placement and Kamada-Kawai for intra-community node arrangement.

#### Scenario: Grid+KK layout
- **WHEN** the hybrid layout is applied to a graph with 4 communities
- **THEN** communities are placed on a 2x2 grid, and nodes within each community are arranged using Kamada-Kawai

### Requirement: Community ForceAtlas2 Layout
The system SHALL provide a layout that runs ForceAtlas2 on the community-level graph, then positions individual nodes within their community bounds.

#### Scenario: Community ForceAtlas2
- **WHEN** CommunityForceAtlas2 is applied to a graph with 6 communities
- **THEN** the 6 community centers are positioned via ForceAtlas2 on the quotient graph, and nodes within each community are arranged using FR

### Requirement: Layout Factory Registration
Every layout algorithm SHALL register itself in a factory by a short name string, enabling runtime selection.

#### Scenario: Factory lookup by short name
- **WHEN** the factory is queried with short name "f"
- **THEN** it returns a CPU Fruchterman-Reingold layout instance

#### Scenario: All expected short names registered
- **WHEN** the factory is queried for all registered names
- **THEN** it includes at minimum: "f" (FR CPU), "fgpu" (FR GPU), "forceAtlas2" (FA2), "kk" (Kamada-Kawai), "c" (circular), "grid" (grid), "gkk" (grid+KK)

#### Scenario: Unknown short name
- **WHEN** the factory is queried with an unrecognized short name
- **THEN** it returns an error or throws

### Requirement: Layout Progress Reporting
Every layout algorithm SHALL report progress as a float from 0.0 (not started) to 1.0 (complete) during computation.

#### Scenario: Progress callback during FR
- **WHEN** FR runs 500 iterations and the callback is invoked at iteration 250
- **THEN** the progress value is approximately 0.5

#### Scenario: Progress callback at completion
- **WHEN** any layout algorithm finishes
- **THEN** the final progress callback reports exactly 1.0

#### Scenario: No callback registered
- **WHEN** a layout runs without any registered progress callback
- **THEN** it completes normally without error

### Requirement: Coordinate Output
All layout algorithms SHALL produce exactly one (x, y) coordinate pair per node in the graph.

#### Scenario: Complete coordinate assignment
- **WHEN** any layout completes on a 300-node graph
- **THEN** exactly 300 coordinate pairs are produced, one per node ID

#### Scenario: No duplicate coordinates
- **WHEN** a layout completes on a graph where all nodes have degree >= 1
- **THEN** no two nodes share the exact same (x, y) coordinate (within floating-point epsilon)

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
