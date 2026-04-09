## Why

satGraf is a Java-based SAT solver visualization tool that parses DIMACS CNF files, builds clause-variable interaction graphs, detects communities, and renders force-directed layouts. The current implementation suffers from fundamental performance limitations: Java's memory overhead limits graph size, Swing provides an outdated UI, and the single-threaded layout algorithms are too slow for large CNF instances. The GPU-accelerated layout (OpenCL via JOCL) is also incomplete. A C++ rewrite with Qt 6, igraph, and native OpenCL will deliver order-of-magnitude performance improvements, a modern GUI, and a clean external solver integration model via named pipe IPC — enabling visualization of industrial-scale SAT instances.

## What Changes

**BREAKING**: Complete rewrite from Java to C++. No backward compatibility with the Java codebase.

- **New core engine in C++17/20**: DIMACS CNF parser, lightweight graph data structure (CSR adjacency), VIG (Variable Interaction Graph) and LIG (Literal Interaction Graph) construction
- **Community detection via igraph**: Replace custom Louvain/CNM/OL implementations with igraph's battle-tested algorithms (igraph_community_louvain, igraph_community_fastgreedy)
- **GPU-accelerated layout via OpenCL**: Port existing Fruchterman-Reingold OpenCL kernels to native C OpenCL API; add multi-threaded ForceAtlas2 with Barnes-Hut optimization; support AMD iGPU via OpenCL (no CUDA dependency)
- **External SAT solver**: User provides solver binary via `-s` flag (cadical, kissat, minisat, or any compatible solver), events captured via named pipe (FIFO)
- **Modern Qt 6 + QML GUI**: Replace Swing with QGraphicsScene for large-graph GPU rendering, QML for modern control panels, interactive zoom/pan/selection
- **Evolution tracking**: Port the solver event stream parser (variable assignments, clause learning, conflicts) with forward/backward playback and observer pattern
- **Headless export mode**: JPEG/PNG static export and animated GIF evolution export without GUI window
- **All original modes preserved**: `com` (community graph), `imp` (implication graph), `evo` (evolution viewer), `exp` (headless export), plus data generation modes

## Capabilities

### New Capabilities

- `dimacs-parser`: Parse DIMACS CNF files (p-line, c-line comments, clause lines) into internal graph representation with named variable support
- `graph-model`: Core graph data structures — nodes, edges, clauses, CSR adjacency storage, Union-Find connectivity, VIG and LIG graph types
- `community-detection`: Community detection algorithms (Louvain, CNM, Online) via igraph integration with community statistics (Q modularity, size distributions, edge ratios)
- `layout-engine`: Force-directed layout algorithms — Fruchterman-Reingold (CPU + OpenCL GPU), ForceAtlas2 (multi-threaded + Barnes-Hut), Kamada-Kawai, Grid, Circular community layouts
- `evolution-engine`: SAT solver event stream processing — named pipe IPC, forward/backward state replay, variable assignment tracking, conflict detection, decision variable highlighting
- `rendering`: Qt QGraphicsScene-based graph rendering with GPU acceleration — nodes as circles, edges as lines, community coloring, highlight layers, zoom/pan
- `gui`: Qt 6 + QML application shell — main window, control panels (layout options, community filters, evolution playback), file open/save, modern look-and-feel
- `export`: Headless and interactive image export — JPEG/PNG for static graphs, animated GIF for evolution sequences
- `solver-integration`: External SAT solver binary launch with named pipe (FIFO) event capture — works with cadical, kissat, minisat, or any solver supporting the pipe protocol

### Modified Capabilities

(none — this is a greenfield rewrite)

## Impact

- **Language change**: Java → C++17/20. Entire codebase rewritten.
- **New dependencies**: igraph (graph algorithms), Qt 6 (GUI), OpenCL C API (GPU), Eigen (linear algebra), nlohmann/json (JSON), CLI11 (CLI parsing)
- **SAT solver**: External binary only — user provides solver path via `-s` (cadical, kissat, minisat, or any compatible solver)
- **Build system**: Gradle → CMake with system packages (apt) for dependency management
- **Platform**: Linux primary, cross-platform possible via Qt and OpenCL
- **API surface**: CLI interface preserved (same command structure: com/imp/evo/exp), but internal APIs are entirely new C++ interfaces
- **Testing**: JUnit → Catch2 or Google Test
- **satlib.jar eliminated**: All functionality from the 66-class Java library (graph model, community detection, DIMACS parsing, evolution tracking) reimplemented in C++ or replaced by igraph
