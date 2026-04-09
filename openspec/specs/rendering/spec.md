# Rendering

Qt-based graph rendering using QGraphicsScene and QGraphicsView for displaying large SAT instance graphs with layered rendering, community coloring, zoom/pan, and scale-aware detail control.

## ADDED Requirements

### Requirement: Scene and View Setup
The system SHALL use Qt's QGraphicsScene for managing graph items and QGraphicsView for display, supporting large graphs (10,000+ nodes) without freezing the UI.

#### Scenario: Large graph scene creation
- **WHEN** a 20,000-node, 80,000-edge graph is loaded
- **THEN** the QGraphicsScene is populated without blocking the main thread for more than 500ms, and the view displays the graph interactively

### Requirement: Node Rendering
Nodes SHALL be rendered as filled circles with a configurable diameter (default 10 pixels). Node color SHALL be determined by community assignment. Node z-value SHALL be dynamically adjustable to support community selection highlighting.

#### Scenario: Default node appearance
- **WHEN** a node is rendered with default settings
- **THEN** it appears as a filled circle of 10px diameter, colored by its community ID using a community color palette, at z-value 1.0

#### Scenario: Custom node diameter
- **WHEN** the node diameter is set to 20px
- **THEN** all nodes render at 20px diameter

#### Scenario: Community-based coloring
- **WHEN** node 5 belongs to community 2 and community 2's color is blue
- **THEN** node 5 is rendered in blue

#### Scenario: Selected community z-order promotion
- **WHEN** community 3 is selected by the user
- **THEN** all nodes in community 3 have their z-value set to 2.0, rendering them above nodes at z-value 1.0

#### Scenario: Deselected community z-order restoration
- **WHEN** community 3 is deselected (user selects "— none —")
- **THEN** all nodes in community 3 have their z-value restored to 1.0

### Requirement: Edge Rendering
Edges SHALL be rendered as lines between connected nodes. Intra-community edges SHALL be colored by community; inter-community edges SHALL be rendered in white or gray. Edge z-value SHALL be dynamically adjustable for selected community highlighting.

#### Scenario: Intra-community edge
- **WHEN** an edge connects two nodes in community 3, and community 3's color is green
- **THEN** the edge line is rendered in green

#### Scenario: Inter-community edge
- **WHEN** an edge connects node in community 1 to a node in community 4
- **THEN** the edge line is rendered in white or gray, distinct from community colors

#### Scenario: Selected community edge promotion
- **WHEN** community 3 is selected
- **THEN** intra-community edges of community 3 are promoted to z-value 1.5, rendering them above edges at z-value 0.0

#### Scenario: Intra-community edge
- **WHEN** an edge connects two nodes in community 3, and community 3's color is green
- **THEN** the edge line is rendered in green

#### Scenario: Inter-community edge
- **WHEN** an edge connects node in community 1 to a node in community 4
- **THEN** the edge line is rendered in white or gray, distinct from community colors

#### Scenario: Conflict edge style
- **WHEN** an edge is marked as type "conflict"
- **THEN** it is rendered with a distinct style (e.g., red color or dashed pattern)

### Requirement: Multi-Threaded Rendering
Edge and node drawing SHALL be performed in parallel using multiple threads to reduce rendering time for large graphs.

#### Scenario: Parallel draw
- **WHEN** a graph with 50,000 edges is rendered on a 4-core machine
- **THEN** edge drawing is distributed across 4 threads, each handling a portion of edges, reducing total draw time compared to single-threaded rendering

#### Scenario: Thread-safe scene modification
- **WHEN** multiple threads add items to the QGraphicsScene concurrently
- **THEN** no data races or scene corruption occur (e.g., by batching additions and applying them on the main thread)

### Requirement: Double Buffering
The rendering pipeline SHALL use double buffering to prevent visual flicker during redraws.

#### Scenario: Flicker-free redraw
- **WHEN** the graph is updated (e.g., after evolution step)
- **THEN** the new frame is fully composed offscreen before being presented, with no visible flicker or partial renders

### Requirement: Layered Rendering
The rendering SHALL use discrete layers: edge layer (bottom), node layer (middle), highlight layer (above nodes), and decision variable layer (top).

#### Scenario: Layer ordering
- **WHEN** a node and an edge occupy the same screen region
- **THEN** the node is drawn on top of the edge, and highlights are drawn on top of nodes

#### Scenario: Decision variable layer
- **WHEN** in evolution mode and a decision variable is active
- **THEN** the decision variable node is rendered with a highlight in the topmost layer, clearly visible above all other elements

#### Scenario: Community highlight layer
- **WHEN** community outlines are enabled
- **THEN** community regions are outlined or shaded in a layer above edges but below individual node highlights

### Requirement: Zoom and Pan
The view SHALL support zoom via QGraphicsView transform and pan via scroll bars or mouse drag.

#### Scenario: Zoom in
- **WHEN** the user zooms in by a factor of 2x
- **THEN** all rendered items appear twice as large, centered on the viewport center (or mouse position if supported)

#### Scenario: Zoom out
- **WHEN** the user zooms out to show the entire graph
- **THEN** all nodes and edges are visible within the viewport

#### Scenario: Pan via scroll bars
- **WHEN** the user drags the horizontal or vertical scroll bar
- **THEN** the viewport shifts accordingly, showing different regions of the graph

#### Scenario: Pan via mouse drag
- **WHEN** the user clicks and drags on the canvas (with middle button or a designated modifier)
- **THEN** the view pans to follow the mouse movement

### Requirement: Node Click Detection
The system SHALL detect mouse clicks on nodes by performing a hit test against the circle bounds of each rendered node.

#### Scenario: Click on node center
- **WHEN** the user clicks within the circle bounds of node 42
- **THEN** node 42 is reported as the clicked node, and the highlight layer shows selection feedback

#### Scenario: Click between nodes
- **WHEN** the user clicks in empty space between two nodes
- **THEN** no node is selected, and any previous selection highlight is cleared

#### Scenario: Click at high zoom
- **WHEN** the user is zoomed in 10x and clicks near the edge of node 42's rendered circle
- **THEN** the hit test correctly identifies node 42 based on its actual circle bounds at the current zoom level

### Requirement: Scale-Aware Rendering
The renderer SHALL hide or show detail based on the current zoom level. At low zoom, labels and fine details are hidden. At high zoom, labels and node details are shown.

#### Scenario: Low zoom hides labels
- **WHEN** the zoom level is below a threshold (e.g., less than 1.0x, showing the entire graph)
- **THEN** node labels are not rendered, reducing visual clutter and improving performance

#### Scenario: High zoom shows labels
- **WHEN** the zoom level is above a threshold (e.g., greater than 2.0x)
- **THEN** node names or IDs are rendered as text labels next to each node

#### Scenario: Zoom-dependent edge rendering
- **WHEN** the zoom level is very low (showing thousands of nodes)
- **THEN** edge rendering MAY be simplified (thinner lines, reduced anti-aliasing) to maintain frame rate

### Requirement: Configurable Visibility Filters
The system SHALL support configurable filters to show or hide nodes and edges based on criteria such as community membership, assignment state, or edge type.

#### Scenario: Filter by community
- **WHEN** the user enables only communities 1 and 3 in the visibility filter
- **THEN** nodes belonging to other communities and their incident edges are hidden from the view

#### Scenario: Filter by assignment state
- **WHEN** the user filters to show only assigned (true/false) nodes
- **THEN** unassigned nodes and their incident edges are hidden

#### Scenario: Filter by edge type
- **WHEN** the user hides conflict edges
- **THEN** all edges of type "conflict" are removed from the visual, while normal edges remain visible
