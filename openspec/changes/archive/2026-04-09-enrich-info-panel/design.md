## Context

satGraf's right panel currently shows minimal information: 4 global statistics labels (nodes, edges, communities, modularity), a community dropdown showing only node count + color, and a node detail label showing only ID + community + color. The underlying `CommunityResult` already computes `inter_edges` and `intra_edges` but these are never displayed. The `CommunityNode::bridge` field exists but is never populated. Node selection is mouse-only — there is no text search capability.

All GUI code is header-only in 3 files: `main_window.hpp` (886 lines), `graph_renderer.hpp` (538 lines), and `export.hpp`. The right panel is built in `MainWindow::build_right_panel()` (lines 710–765). Z-ordering is fixed: edges at 0.0, community regions at 0.5, nodes at 1.0, labels at 2.0, decision highlight at 3.0.

The layout system uses a `Coordinate` struct with `{x, y}` and a `LayoutFactory` with 7 registered algorithms. `community_forceatlas_centers()` already builds a quotient graph (communities as nodes, inter-community edges aggregated with weights) — this is the foundation for Simple 2D and 3D rendering modes.

## Goals / Non-Goals

**Goals:**
- Display internal/external edge counts at the graph level after community detection
- Enrich community selection detail with: internal nodes, bridge nodes, internal edges, external edges, linked communities count
- Enrich node selection detail with: internal neighbors, external neighbors, linked communities count
- Add text input for node selection by ID or name
- Promote selected community's nodes to a higher z-value in the canvas
- Compute bridge nodes (nodes with edges to other communities) as part of community analysis
- Add rendering mode selection (2D, Simple 2D, 3D) with a dropdown in the left panel
- Implement Simple 2D quotient graph view (communities as sized circles, weighted inter-community edges)
- Implement 3D quotient graph view using ForceAtlas2-based 3D layout with rotation support

**Non-Goals:**
- Modifying the core graph data model (Node, Edge, Graph templates)
- Adding new community detection algorithms
- Changing the left panel layout or controls
- Modifying export/headless rendering behavior
- Implementing graph-theoretic bridge-edge detection (Tarjan's algorithm) — we only mean "community bridge nodes"
- Adding autocomplete dropdown for node search (simple exact/prefix match suffices)
- Full 3D rendering of individual nodes (only quotient graph in 3D)
- Physics-based 3D simulation beyond ForceAtlas2 extension

## Decisions

### Decision 1: Bridge node computation as a standalone utility function

**Choice**: Add a free function `compute_bridge_nodes()` in `community_detector.hpp`'s `detail` namespace that populates a `std::unordered_set<NodeId>` of bridge nodes, rather than modifying `CommunityResult` or using `CommunityNode::bridge`.

**Rationale**: The main window's `MainWindow` uses `Graph<Node, Edge>` (not `Graph<CommunityNode, Edge>`), so `CommunityNode::bridge` is not accessible from the active data path. The community assignment flows through `CommunityResult::assignment` (an `unordered_map<NodeId, CommunityId>`). Storing bridge nodes as a separate set keeps the change localized to the GUI layer without requiring a graph type change.

**Alternative considered**: Extend `CommunityResult` with a `bridge_nodes` field. Rejected because `CommunityResult` is a core library struct and bridge detection is a GUI-driven concern. However, if future core-level features need bridge nodes, this can be migrated.

### Decision 2: Per-community statistics computed on demand in MainWindow

**Choice**: Compute per-community edge breakdowns, bridge node counts, and linked-community counts directly in `on_community_selected()` and `on_node_clicked()` rather than precomputing and storing them.

**Rationale**: These computations are O(edges) at worst, and are only triggered by user selection events (not per-frame). Precomputing would require extending `CommunityResult` with per-community data structures that are only needed in the GUI. On-demand computation keeps the core library unchanged.

**Alternative considered**: Precompute a `PerCommunityStats` map during `on_render_complete()` and cache it. This would be faster for repeated selections but adds memory and complexity. Can be optimized later if profiling shows latency.

### Decision 3: Node text search via QLineEdit with exact/prefix matching

**Choice**: Add a `QLineEdit` in the right panel's "Selected Node" section, connected to a slot that searches `graph_->getNodes()` by both numeric ID (exact match) and node name (prefix match, case-insensitive).

**Rationale**: The existing `graph_->getNodes()` provides direct access to all nodes. No new index is needed — iterating a few hundred/thousand nodes on text change is fast enough. The input field syncs bidirectionally with click selection.

**Alternative considered**: QComboBox with completion. Rejected because `QComboBox` requires pre-populating all items, which adds overhead for large graphs.

### Decision 4: Z-order manipulation via new GraphRenderer method

**Choice**: Add `GraphRenderer::highlight_community(std::optional<uint32_t> cid)` as a public method that iterates `node_items_` and `edge_items_`, setting z-values: selected community nodes to 2.0, others to 1.0; selected community intra-edges to 1.5, others remain at 0.0.

**Rationale**: `node_items_` and `edge_items_` are private to `GraphRenderer`. Exposing them would break encapsulation. A single method encapsulates the z-ordering logic cleanly.

**Alternative considered**: Expose `node_items_` via getter. Rejected — violates encapsulation and invites scattered z-ordering logic.

### Decision 5: Simple 2D uses existing quotient graph infrastructure

**Choice**: Reuse `community_forceatlas_centers()` (layout.hpp:1185–1234) which already builds a quotient graph (communities as nodes, inter-community edges aggregated with weights) to produce the layout for Simple 2D mode. Add a new `GraphRenderer::render_simple_2d()` method that renders each community as a single `QGraphicsEllipseItem` sized proportional to its node count, with edges whose width is proportional to inter-community edge weight.

**Rationale**: The quotient graph construction is already implemented and tested. The layout computation via `ForceAtlas2Layout` on the quotient is identical to what `CommunityForceAtlas2Layout` already does. Only the rendering path changes — instead of rendering individual nodes within community bounding boxes, we render one circle per community.

**Alternative considered**: Build a separate simplified graph model. Rejected — the quotient graph is already built inside `community_forceatlas_centers()` and the same data is needed for stats display.

### Decision 6: 3D rendering via QOpenGLWidget with perspective projection

**Choice**: Add a new `GraphView3D` widget subclassing `QOpenGLWidget` (from Qt 6 Gui) that renders the quotient graph in 3D using OpenGL. The layout is computed by a new `ForceAtlas3DLayout` class that extends the FA2 repulsion/attraction model to 3D coordinates (`x, y, z`). Mouse drag rotates the camera (arcball rotation), scroll wheel zooms.

**Rationale**: Qt's `QOpenGLWidget` is part of Qt 6 Gui (already a dependency) and provides a native OpenGL context without additional libraries. The FA2 algorithm naturally generalizes to 3D by adding a z-component to repulsion/attraction/gravity forces. The quotient graph is small (number of communities, typically < 100 nodes), so 3D rendering performance is not a concern.

**Alternative considered**: Use Qt 3D framework. Rejected — adds a heavy dependency for rendering a few dozen spheres and lines. QOpenGLWidget with raw OpenGL calls is lighter and more portable.

### Decision 7: Rendering mode as GUI-level concern, not layout algorithm

**Choice**: The rendering mode dropdown is a separate control from the layout algorithm dropdown. When "Simple 2D" or "3D" is selected, the system still runs community detection + layout, but then renders the quotient graph instead of the full graph. The layout algorithm dropdown still affects how individual community sub-layouts are computed (in 2D mode), while Simple 2D/3D always uses ForceAtlas2 on the quotient.

**Rationale**: The rendering mode affects what is displayed, not how individual nodes are positioned. Separating these concerns keeps the layout algorithm factory unchanged and avoids coupling rendering mode with layout selection.

## Risks / Trade-offs

**[Performance on large graphs]** → On-demand per-community computation iterates all edges each time a community is selected. For 100K+ edge graphs, this could take a few milliseconds. **Mitigation**: Acceptable for user-triggered events (not per-frame). Cache later if needed.

**[Right panel space]** → Adding 5+ new label rows to the statistics section and 2 new rows to community/node detail sections may require scrolling in the right panel (fixed 230px width). **Mitigation**: The right panel already uses `QVBoxLayout` with `addStretch()` — new items will push down naturally. Consider compact layout with side-by-side labels if space is tight.

**[Bridge node terminology]** → "Bridge node" in this context means a node with neighbors in other communities (community bridge), NOT a graph-theoretic bridge (edge whose removal disconnects the graph). **Mitigation**: Label clearly as "Bridge Nodes" in the UI with a tooltip explaining the definition.

**[3D OpenGL portability]** → OpenGL behavior varies across drivers and platforms. **Mitigation**: Use OpenGL ES 2.0 compatible subset (no advanced shaders). Test on Mesa (Linux), ANGLE (Windows). The 3D scene is simple (spheres + lines) — no complex GPU features needed.

**[3D view / 2D view switching]** → Switching between rendering modes requires swapping the central widget from `GraphView` (QGraphicsView) to `GraphView3D` (QOpenGLWidget). **Mitigation**: Both widgets exist in the splitter simultaneously; only one is visible at a time. The mode dropdown toggles visibility. This avoids destroying/recreating widgets on mode switch.

**[Rendering mode interaction with evolution]** → Evolution mode expects individual node highlighting (decision variables, conflict edges) which doesn't apply in Simple 2D/3D mode. **Mitigation**: Disable Simple 2D and 3D modes when evolution mode is active, or show a warning. Evolution mode forces 2D detailed rendering.
