## Why

The current info panel shows only basic graph-level statistics (nodes, edges, communities, modularity). When users select a community, they see only node count and color. When they click a node, they see only its ID and community. This is insufficient for understanding graph structure — users need internal/external edge counts, bridge node detection, and community connectivity information. Additionally, node selection is mouse-only, limiting usability for large graphs where finding a specific node by clicking is impractical.

## What Changes

- **Graph-level statistics enrichment**: Display internal edges (intra-community) and external edges (inter-community) counts in the statistics section after community detection completes.
- **Community selection enrichment**: When selecting a community from the dropdown, show: internal node count, bridge node count (nodes with edges to other communities), internal edge count, external edge count, and number of linked communities. After selection, visually promote the community's nodes to the top of the z-order in the canvas.
- **Text-based node selection**: Add a text input field allowing users to type a node ID or name to select it, in addition to click-based selection.
- **Node selection enrichment**: When a node is selected (by click or text input), show: number of linked internal-community nodes, number of linked nodes in other clusters, and number of distinct linked communities.
- **Rendering mode selection**: Add a rendering mode dropdown in the left panel with three options:
  - **2D** (default): Current detailed graph rendering with individual nodes and edges.
  - **Simple 2D**: Abstracted view where each community is a single circle whose size is proportional to node count, and edges between circles are weighted by inter-community edge count. Uses the existing `community_forceatlas_centers()` quotient graph approach for layout.
  - **3D**: Same abstraction as Simple 2D but rendered in 3D space using a ForceAtlas2-based 3D layout. Users can rotate the view to examine the community structure from all angles.

## Capabilities

### New Capabilities
- `info-panel-enrichment`: Enriched statistics display for graph-level, community-level, and node-level information in the right panel
- `node-text-selection`: Text input for selecting nodes by ID or name, with real-time search feedback
- `community-z-order`: Z-order promotion of selected community's nodes in the rendered graph
- `rendering-modes`: Three rendering modes (2D detailed, Simple 2D quotient graph, 3D quotient graph with rotation) selectable from the GUI

### Modified Capabilities
- `gui`: Info Panel requirement extended with richer statistics; community/node selection sections enhanced with detailed metrics; new rendering mode selector
- `rendering`: Node rendering extended to support dynamic z-order changes for community highlighting; new Simple 2D and 3D rendering paths
- `layout-engine`: New 3D coordinate type and ForceAtlas2-based 3D layout algorithm

## Impact

- **`src/gui/include/satgraf_gui/main_window.hpp`**: Statistics section expanded with internal/external edges; community detail section enriched with bridge nodes and linked communities; new text input widget for node search; new rendering mode dropdown; `on_community_selected()` and `on_node_clicked()` methods compute and display richer statistics.
- **`src/gui/include/satgraf_gui/graph_renderer.hpp`**: New method to adjust node z-values for community highlighting; new `render_simple_2d()` and `render_3d()` methods for quotient graph rendering.
- **`src/core/include/satgraf/community_detector.hpp`**: `CommunityResult` already contains `inter_edges`/`intra_edges` — no core changes needed for graph-level stats. Bridge node and per-community edge computation added as utility functions.
- **`src/core/include/satgraf/layout.hpp`**: New `Coordinate3D` struct, `Coordinate3DMap`, and `ForceAtlas3DLayout` class extending FA2 to 3D. New `rendering-modes` layout entries in `LayoutFactory`.
- **Dependencies**: Qt 6 QOpenGLWidget or QOpenGLExtraFunctions for 3D rendering (already available via Qt 6 Gui module). No new external dependencies.
