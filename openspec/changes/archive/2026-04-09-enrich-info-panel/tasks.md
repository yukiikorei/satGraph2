## 1. Bridge Node Computation Utility

- [x] 1.1 Add `compute_bridge_nodes()` free function in `community_detector.hpp` (`detail` namespace) that takes `const CommunityResult&` and `const Graph<Node, Edge>&`, returns `std::unordered_set<NodeId>` of nodes that have at least one edge to a node in a different community
- [x] 1.2 Add a `std::unordered_set<NodeId> bridge_nodes` member to `MainWindow` (populated in `on_render_complete()` after community detection)
- [x] 1.3 Verify bridge node computation with existing test suite (test_community_detection.cpp, test_graph_model.cpp)

## 2. Graph-Level Statistics Enrichment

- [x] 2.1 Add two new `QLabel*` members to `MainWindow`: `stats_internal_edges_` and `stats_external_edges_`
- [x] 2.2 Add these labels to the statistics `QFormLayout` in `build_right_panel()` (after "Modularity:" row) with placeholder "—"
- [x] 2.3 Update `on_render_complete()` to populate these labels from `communities_.intra_edges` and `communities_.inter_edges`
- [x] 2.4 Reset these labels to "—" in `open_file_path()` (before community detection)

## 3. Community Selection Enrichment

- [x] 3.1 Expand `on_community_selected()` to compute: internal node count, bridge node count (from `bridge_nodes_` set), internal edge count, external edge count, and linked community count for the selected community
- [x] 3.2 Update `community_detail_->setText()` to display all 5 enriched metrics in a formatted multi-line string
- [x] 3.3 Ensure selecting "— none —" clears the community detail label

## 4. Community Z-Order Highlighting

- [x] 4.1 Add `GraphRenderer::highlight_community(std::optional<uint32_t> cid)` public method that iterates `node_items_` and `edge_items_`, setting z-value to 2.0 for selected community nodes (1.0 default for others) and 1.5 for selected community intra-edges (0.0 default for others)
- [x] 4.2 Call `renderer_->highlight_community(selected_cid)` from `on_community_selected()` when a community is chosen
- [x] 4.3 Call `renderer_->highlight_community(std::nullopt)` when "— none —" is selected to restore defaults
- [x] 4.4 Ensure z-order is re-applied after re-render operations (`on_render_complete()`, `re_render_evolution()`, node size slider change) if a community is currently selected

## 5. Node Text Search Input

- [x] 5.1 Add `QLineEdit* node_search_edit_` member to `MainWindow`
- [x] 5.2 Add a "Search Node:" label and the QLineEdit to the "Selected Node" section in `build_right_panel()` (before `node_detail_` label)
- [x] 5.3 Connect `node_search_edit_->returnPressed()` to a new slot `on_node_search()` that searches `graph_->getNodes()` by numeric ID (exact) then by name (prefix match)
- [x] 5.4 When a match is found, call `on_node_clicked()` with the matched NodeId to select and highlight it
- [x] 5.5 When no match is found, display "Node not found: <input>" in the log
- [x] 5.6 Update `node_search_edit_->setText()` in `on_node_clicked()` to sync the text field with click selections
- [x] 5.7 Disable node search field when no graph is loaded; enable after graph load

## 6. Node Selection Enrichment

- [x] 6.1 Expand `on_node_clicked()` to compute: internal neighbor count (neighbors in same community), external neighbor count (neighbors in different communities), linked community count (distinct communities among external neighbors)
- [x] 6.2 Update `node_detail_->setText()` to display: Node ID, name, Community ID, Internal Neighbors, External Neighbors, Linked Communities
- [x] 6.3 Handle the case where community detection hasn't run (show "—" for community-related fields)

## 7. Rendering Mode Infrastructure

- [x] 7.1 Add a `Coordinate3D` struct to `layout.hpp` with `x`, `y`, `z` double members and a `Coordinate3DMap` typedef (`unordered_map<NodeId, Coordinate3D>`)
- [x] 7.2 Add a rendering mode enum (`enum class RenderMode { Detailed2D, Simple2D, Mode3D }`) and `render_mode_` member to `MainWindow`
- [x] 7.3 Add a "Render Mode:" dropdown in the left panel (`build_left_panel()`) with items "2D", "Simple 2D", "3D", connected to a slot that updates `render_mode_`
- [x] 7.4 Modify `on_render_complete()` to dispatch to the appropriate rendering path based on `render_mode_`

## 8. Simple 2D Quotient Graph Rendering

- [x] 8.1 Extract the quotient graph building logic from `community_forceatlas_centers()` into a reusable `build_quotient_graph()` function in `layout.hpp` that returns the quotient graph and its community-to-node-id mapping
- [x] 8.2 Add `GraphRenderer::render_simple_2d()` method that takes the quotient graph, its layout coordinates, and community sizes — renders each community as a filled circle (diameter ∝ √node_count) with a label showing community ID and node count
- [x] 8.3 Render inter-community edges in Simple 2D with line width proportional to edge weight (inter-community edge count), and display weight labels on edges
- [x] 8.4 Enable click detection on community circles in Simple 2D mode — clicking a circle selects that community in the dropdown and shows enriched detail
- [x] 8.5 Ensure switching from Simple 2D to 2D or 3D correctly clears and re-renders the scene

## 9. ForceAtlas2 3D Layout

- [x] 9.1 Implement `ForceAtlas3DLayout` class in `layout.hpp` extending the FA2 repulsion/attraction/gravity model to 3D: add z-component to all force calculations, use octree (or brute-force for small quotient graphs) instead of quadtree for Barnes-Hut optimization
- [x] 9.2 Register "fa3d" in `LayoutFactory` constructor
- [x] 9.3 Write unit tests for `ForceAtlas3DLayout` in `test_layout.cpp` verifying: output has correct number of 3D coordinates, positions are non-degenerate for connected graphs, convergence behavior

## 10. 3D Quotient Graph Rendering

- [x] 10.1 Create `GraphView3D` class subclassing `QOpenGLWidget` in a new header `graph_view_3d.hpp` under `src/gui/include/satgraf_gui/`
- [x] 10.2 Implement OpenGL initialization, perspective projection matrix, and arcball camera rotation (mouse drag) + scroll zoom in `GraphView3D`
- [x] 10.3 Implement 3D sphere rendering (using `gluSphere` or manually tessellated icosphere) sized by community node count (∛node_count for volume-proportional sizing), colored by community color palette
- [x] 10.4 Implement 3D line rendering for inter-community edges with line width proportional to weight, rendered between sphere centers
- [x] 10.5 Add community ID + node count text labels rendered as billboard text (always facing camera) above each sphere
- [x] 10.6 Add `GraphView3D*` member to `MainWindow`, create it in `setup_central_widget()`, add to the splitter alongside the existing `GraphView`, initially hidden
- [x] 10.7 Implement mode switching: show `view_` and hide `view_3d_` for 2D/Simple 2D; show `view_3d_` and hide `view_` for 3D mode
- [x] 10.8 Force 2D rendering mode when evolution mode is active (disable Simple 2D and 3D in `set_controls_enabled()`)

## 11. Verification (Extended)

- [x] 11.1 Build the project with `cmake --build build -j$(nproc)` and verify zero compilation errors
- [x] 11.2 Run existing test suite `ctest --test-dir build --output-on-failure` and verify no regressions
- [x] 11.3 Manually verify: load a CNF file, render with community detection, check internal/external edges in statistics
- [x] 11.4 Manually verify: select a community, check enriched detail (nodes, bridge nodes, edges, linked communities) and z-order promotion in canvas
- [x] 11.5 Manually verify: type a node ID in search field, press Enter, check node selection and enriched detail
- [x] 11.6 Manually verify: click a node, check enriched detail and search field sync
- [x] 11.7 Manually verify: switch to Simple 2D mode, check community circles are sized by node count, edges are weighted, click on circle selects community
- [x] 11.8 Manually verify: switch to 3D mode, check spheres render correctly, drag to rotate, scroll to zoom, community labels visible
- [x] 11.9 Manually verify: start solver in evolution mode, verify rendering mode switches to 2D and Simple 2D/3D are disabled
- [x] 11.10 Manually verify: switch between all three rendering modes and verify no visual artifacts or memory leaks
