## 1. Project Setup & Build Infrastructure

- [x] 1.1 Create C++ project directory structure: `src/core/`, `src/gui/`, `src/app/`, `tests/`, `third_party/`, `resources/`
- [x] 1.2 Create top-level `CMakeLists.txt` with C++17 standard, using `find_package()` for all system dependencies
- [x] 1.3 Verify all dependencies are discoverable via CMake find_package: igraph, Qt6, OpenCL, Eigen3, nlohmann_json, CLI11, Catch2
- [x] 1.4 Add Catch2 testing framework setup in `tests/CMakeLists.txt`
- [x] 1.5 Create `src/core/CMakeLists.txt` — static library with no Qt dependency
- [x] 1.6 Create `src/gui/CMakeLists.txt` — Qt 6 + QML linked against core
- [x] 1.7 Create `src/app/CMakeLists.txt` — main executable linking core + gui
- [x] 1.8 Verify full build succeeds with empty main.cpp (dependency resolution validated)

## 2. Core Graph Model

- [x] 2.1 Define `NodeId`, `EdgeId`, `CommunityId` as strong typedefs (type-safe integer wrappers)
- [x] 2.2 Implement `struct Node` with id, name, adjacency list, groups, assignment state (true/false/unassigned), activity counter, appearance counts
- [x] 2.3 Implement `struct Edge` with source/target NodeId, bidirectional flag, weight, visibility state, type (normal/conflict)
- [x] 2.4 Implement `struct Clause` as set of (NodeId, polarity) pairs using `std::unordered_map<NodeId, bool>`
- [x] 2.5 Implement template `class Graph<NodeT, EdgeT>` with operations: createNode, createEdge, connect, getNode, getNodes, getEdges, getClauses, removeNode, removeEdge
- [x] 2.6 Implement `CommunityNode` extending Node with community ID and bridge flag
- [x] 2.7 Implement `CommunityGraph` extending Graph with community statistics (Q modularity, community sizes, edge ratios, inter/intra community edges)
- [x] 2.8 Implement CSR (Compressed Sparse Row) adjacency structure for performance-critical paths
- [x] 2.9 Implement Union-Find (disjoint set) for connectivity queries with path compression and union by rank
- [x] 2.10 Add DIMACS output (write graph to DIMACS format — reverse of parsing)
- [x] 2.11 Write unit tests for Graph CRUD operations, CSR construction, Union-Find connectivity

## 3. DIMACS CNF Parser

- [x] 3.1 Implement `dimacs::Parser` class with `parse(file_path, mode)` returning a Graph
- [x] 3.2 Implement p-line parsing (`p cnf <vars> <clauses>`) with error handling for missing/duplicate problem lines
- [x] 3.3 Implement c-line parsing for variable naming (`c <id> <name>`)
- [x] 3.4 Implement regex-based variable grouping from named variables (user-provided regex patterns)
- [x] 3.5 Implement clause line parsing (space-separated literals, terminated by 0, multi-line clauses)
- [x] 3.6 Implement VIG mode: create edges between variables co-occurring in same clause, edge weight = co-occurrence count
- [x] 3.7 Implement LIG mode: create edges between signed literals, preserving polarity in node identity
- [x] 3.8 Implement progress reporting callback (byte position / file size → 0.0 to 1.0)
- [x] 3.9 Add graceful error handling for malformed input (invalid literals, unterminated clauses)
- [x] 3.10 Write unit tests with sample DIMACS files covering: basic clauses, unit clauses, empty clauses, VIG vs LIG output, named variables, regex grouping, error cases

## 4. Community Detection

- [x] 4.1 Create `community::Detector` interface with virtual method `detect(Graph&) -> CommunityResult`
- [x] 4.2 Implement community factory: register detectors by name ("louvain", "cnm", "online")
- [x] 4.3 Implement Louvain detector wrapping `igraph_community_louvain` — convert Graph to igraph graph, run algorithm, extract partition + Q modularity
- [x] 4.4 Implement CNM detector wrapping `igraph_community_fastgreedy` — same conversion flow
- [x] 4.5 Implement Online (OL) community detector — custom incremental assignment (~300 lines based on satlib OLCommunityMetric.java)
- [x] 4.6 Implement disjoint graph detection: warn and handle gracefully when Louvain encounters disconnected components
- [x] 4.7 Implement community statistics computation: min/max/mean/SD of community sizes, inter/intra community edge counts, edge ratio
- [x] 4.8 Write unit tests with known community structures, verify Q modularity ranges, test disjoint graph handling

## 5. Layout Engine (CPU)

- [x] 5.1 Create `layout::Layout` interface with virtual method `compute(Graph&, progress_callback) -> CoordinateMap`
- [x] 5.2 Implement layout factory: register by short name ("f", "forceAtlas2", "kk", "c", "grid", "gkk")
- [x] 5.3 Implement Fruchterman-Reingold CPU layout: O(n²) per iteration, configurable iterations (default 500), cooling schedule, optimal distance k = C * sqrt(area/n), rescaling to bounds
- [x] 5.4 Implement Kamada-Kawai layout: all-pairs shortest-path based, handle disconnected components separately
- [x] 5.5 Implement ForceAtlas2 layout: multi-threaded, Barnes-Hut optimization (theta=1.2), auto-adjusted speed, lin-log mode, gravity, edge weight influence
- [x] 5.6 Implement Community Circular layout: place community centers on circle, run FR within each community
- [x] 5.7 Implement Community Grid layout: arrange communities in 2D grid, FR within each cell
- [x] 5.8 Implement Community Grid+KK hybrid: grid for community placement, KK for intra-community
- [x] 5.9 Implement Community ForceAtlas2: run FA2 on quotient graph, then FR within communities
- [x] 5.10 Implement progress reporting (0.0 to 1.0) for all layouts
- [x] 5.11 Write unit tests: verify all layouts produce coordinates for every node, no NaN/inf, coordinates within bounds, factory returns correct types

## 6. Layout Engine (GPU / OpenCL)

- [x] 6.1 Create OpenCL context initialization: enumerate platforms, find AMD device, create context + command queue
- [x] 6.2 Port FR repulsion kernel from Java OpenCL to C OpenCL kernel source
- [x] 6.3 Port FR attraction kernel from Java OpenCL to C OpenCL kernel source
- [x] 6.4 Port FR aggregation and position adjustment kernels
- [x] 6.5 Implement GPU FR placer: upload graph data to OpenCL buffers, run kernel pipeline, read back positions
- [x] 6.6 Implement CPU fallback when OpenCL is unavailable (detect at runtime, emit warning, delegate to CPU FR)
- [x] 6.7 Register "fgpu" in layout factory
- [x] 6.8 Write integration test: GPU FR on small graph, compare output quality with CPU FR; test fallback path

## 7. SAT Solver Integration

- [x] 7.1 Implement `solver::ExternalSolver` class: spawn solver binary as child process, pass CNF file and FIFO path as arguments
- [x] 7.2 Implement named FIFO creation (`mkfifo`) with RAII cleanup (remove FIFO on destruction)
- [x] 7.3 Implement solver process lifecycle management: start, monitor (detect SAT exit code 10 / UNSAT exit code 20), stop (SIGTERM on user cancel)
- [x] 7.4 Implement crash handling: detect unexpected process termination, report error, clean up FIFO and pipes
- [x] 7.5 Add CLI option `-s <solver_path>` — mandatory for evolution mode, error if missing
- [x] 7.6 Add GUI file selector for solver binary path, persist selection in application settings
- [x] 7.7 Write unit tests: launch a mock solver script that writes known events to FIFO, verify FIFO IPC end-to-end, verify cleanup on normal/crash exit

## 8. Evolution Engine

- [x] 8.1 Define event types: VariableAssignment (id, state, activity), ClauseEvent (add/remove, literal IDs), ConflictEvent (conflict number)
- [x] 8.2 Implement event stream parser: parse 'v', 'c', '!' lines from pipe/callback into typed events
- [x] 8.3 Implement forward evolution: apply events sequentially, update node/edge states in graph
- [x] 8.4 Implement backward evolution: maintain state history stack, revert to previous state
- [x] 8.5 Implement file buffering: split solver output into ~1M line chunks, buffer next file in background thread
- [x] 8.6 Implement conflict scanning: jump to specific conflict number by forward/backward replay
- [x] 8.7 Implement decision variable tracking: highlight current decision variable, clear on next decision
- [x] 8.8 Implement observer pattern: registered observers receive nodeAssigned, addEdge, removeEdge, updateGraph, newFileReady callbacks
- [x] 8.9 Support both VIG and LIG graph modes during evolution
- [x] 8.10 Write unit tests: parse sample event stream, verify forward/backward state transitions, verify conflict scanning lands on correct state

## 9. Rendering Layer

- [x] 9.1 Create `rendering::GraphRenderer` class wrapping QGraphicsScene
- [x] 9.2 Implement node rendering: circles with configurable diameter (default 10px), community-based color palette
- [x] 9.3 Implement edge rendering: lines with community coloring (intra=community color, inter=white/gray), conflict edge style
- [x] 9.4 Implement layered rendering: edge layer (bottom), node layer, highlight layer, decision variable layer (top)
- [x] 9.5 Implement multi-threaded draw: distribute edge/node drawing across threads, batch-add to scene on main thread
- [x] 9.6 Implement double buffering for flicker-free redraws
- [x] 9.7 Implement zoom via QGraphicsView transform, pan via scroll bars and mouse drag
- [x] 9.8 Implement node click detection via QGraphicsItem hit testing
- [x] 9.9 Implement scale-aware rendering: hide labels at low zoom, show at high zoom, simplify edges at low zoom
- [x] 9.10 Implement visibility filters: show/hide by community, assignment state, edge type
- [x] 9.11 Implement community highlight layer: outline or shade community regions
- [x] 9.12 Write integration tests: render a 10K-node graph, verify layers, verify click detection, verify filter toggling

## 10. GUI (Qt 6 + QML)

- [x] 10.1 Create QML main window with split pane layout: canvas (left ~70%) + control panel (right ~30%), resizable divider
- [x] 10.2 Implement menu bar: File (Open, Save, Export), View (zoom in/out/fit/reset), Help (About)
- [x] 10.3 Implement community mode control panel: file selector, community algorithm dropdown, layout algorithm dropdown, node coloring options, edge coloring options
- [x] 10.4 Implement community filter: checkboxes per community (labeled by ID and size), sortable by size/count/ID
- [x] 10.5 Implement evolution mode control panel: playback controls (play/pause/step forward/step backward), timeline slider, conflict counter display
- [x] 10.6 Implement decision variable toggle and temperature coloring option in evolution panel
- [x] 10.7 Implement implication mode control panel: clause display panel, node interaction (click to set value)
- [x] 10.8 Implement implication propagation visualization (animate/highlight affected nodes)
- [x] 10.9 Implement info panel: node count, edge count, clause count, community statistics
- [x] 10.10 Apply modern QML styling for clean, consistent cross-platform appearance
- [x] 10.11 Implement responsive layout: control panel reflows vertically when narrow
- [x] 10.12 Implement CLI headless mode: `--headless --export output.png --input instance.cnf` (no GUI window)
- [x] 10.13 Implement `--help` CLI usage message
- [x] 10.14 Write integration tests: launch GUI, load a CNF file, verify controls appear, verify headless export produces image

## 11. Export

- [x] 11.1 Implement static image export: render to offscreen QImage, save as JPEG/PNG with configurable quality
- [x] 11.2 Implement animated GIF export for evolution mode: render each step as frame, write animated GIF via frame sequence
- [x] 11.3 Implement headless rendering: create offscreen QImage (default 1024x1024), render without showing window
- [x] 11.4 Add export progress reporting callback
- [x] 11.5 Write unit tests: export a rendered graph to PNG, verify file size > 0; export GIF with 10 frames, verify animated format

## 12. Application Integration & End-to-End

- [x] 12.1 Implement `AppController` orchestrating the full pipeline: parse → community detect → layout → render
- [x] 12.2 Wire GUI controls to core engine: file open triggers parse + community + layout + render
- [x] 12.3 Wire evolution controls: play/pause step through evolution engine, update rendering in real time
- [x] 12.4 Implement end-to-end community mode test: load CNF → detect communities → compute layout → render → export PNG
- [x] 12.5 Implement end-to-end evolution mode test: load CNF → start solver → capture events → replay → render → export GIF
- [x] 12.6 Performance benchmark: run full pipeline on a large graph (1K+ vars), verify layout completes in < 30s
- [x] 12.7 Final CLI integration: support all original satGraf modes (com/imp/evo/exp) via CLI arguments
