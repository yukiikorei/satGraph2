## Context

satGraf is a Java SAT solver visualization tool (~100 Java source files + satlib.jar with 66 classes). The system parses DIMACS CNF files, builds variable interaction graphs, detects communities, computes force-directed layouts, and renders interactively. The Java codebase has been fully analyzed and decompiled:

- **satlib.jar**: Core graph model (Graph/Node/Edge/Clause), DIMACS parsing (DimacsGraphFactory), community detection (LouvianNetwork ~877 lines, CNM, OL), evolution tracking (Evolution ~728 lines)
- **satgraf**: Layout algorithms (FruchPlacer, ForceAtlas2, FruchGPUPlacer with OpenCL kernels), Swing UI (GraphCanvas with EdgeLayer/NodeLayer/HighlightLayer), export (JPEG/GIF)
- **Dependencies**: 32 JAR files including JUNG, Gephi layout, JOCL (OpenCL), Trove (primitive collections), JFreeChart, commons-cli

The rewrite targets C++17/20 with igraph replacing custom community detection, Qt 6 replacing Swing, native OpenCL replacing JOCL, and external solver binary integration replacing the original minisat process spawning.

## Goals / Non-Goals

**Goals:**
- Order-of-magnitude performance improvement for layout computation on large graphs (100K+ variables)
- GPU-accelerated layout via OpenCL on AMD iGPU (no CUDA dependency)
- Modern, visually appealing Qt 6 + QML GUI
- Feature parity with original satGraf (all modes: com/imp/evo/exp)
- Clean, modular C++ architecture with clear separation of computation and rendering
- External SAT solver integration via named pipe IPC for evolution tracking
- Headless CLI export mode for batch processing

**Non-Goals:**
- Java interoperability or JNI bridge
- Backward compatibility with satlib.jar data formats (except DIMACS CNF)
- Windows/macOS support in initial release (Linux primary, cross-platform later)
- Web-based visualization (future consideration)
- Real-time collaborative editing
- Bundling or embedding any specific SAT solver (user supplies their own binary)

## Decisions

### Decision 1: igraph for community detection (not custom implementation)

**Choice**: Use igraph C library for Louvain and CNM community detection.

**Rationale**: The original satlib.jar contains ~1200 lines of custom Louvain (LouvianNetwork.java) and CNM implementations. igraph provides these as battle-tested, optimized C functions (`igraph_community_louvain`, `igraph_community_fastgreedy`). This eliminates the largest algorithmic porting effort and provides better performance.

**Alternative considered**: Port LouvianNetwork.java directly to C++. Rejected because igraph's implementation is faster (SIMD-optimized), handles edge cases better (disjoint graphs), and is actively maintained.

**Trade-off**: Adds igraph as a dependency (~2MB). The Online (OL) community detection algorithm is not in igraph and must be custom-implemented (~300 lines).

### Decision 2: Qt 6 + QML for GUI (not Dear ImGui or GTK)

**Choice**: Qt 6 with QGraphicsScene for canvas rendering and QML for control panels.

**Rationale**: QGraphicsScene/QGraphicsView is purpose-built for rendering thousands of items with GPU acceleration, spatial indexing for hit testing, and built-in zoom/pan. QML provides modern declarative UI for control panels. No other framework matches this combination for graph visualization.

**Alternative considered**: Dear ImGui (too tool-like, not "modern beautiful"), GTK (less mature GPU rendering), egui (Rust-only).

**Trade-off**: Qt is a large dependency. CMake integration is well-established. Licensing is LGPL/Commercial.

### Decision 3: OpenCL for GPU compute (not CUDA, not Vulkan compute, not wgpu)

**Choice**: Native OpenCL C API for GPU-accelerated Fruchterman-Reingold layout.

**Rationale**: The user has AMD integrated graphics. CUDA is NVIDIA-only. The existing FruchGPUPlacer.java already contains working OpenCL kernels (repel, attract, adjust, aggregate) that can be ported directly to C OpenCL calls. Vulkan compute and wgpu would require rewriting all kernels.

**Alternative considered**: Vulkan compute (more modern but massive rewrite of kernels), wgpu (Rust-first, C++ bindings immature).

**Trade-off**: OpenCL is older but proven. AMD's OpenCL driver support is solid. The existing kernels are OpenCL — direct port is lowest risk.

### Decision 4: External solver binary via named pipe (not embedded library)

**Choice**: The user provides a solver binary path via `-s <path>`. The application launches it as an external process and communicates via a named pipe (FIFO) to capture solver events.

**Rationale**: Original satGraf already uses this exact pattern (external minisat + named pipe). It is proven and simple. Supporting arbitrary solver binaries gives users maximum flexibility — they can use cadical, kissat, minisat, glucose, or any SAT competition solver without rebuilding the application. No solver source code needs to be bundled or linked.

**Alternative considered**: Embedding cadical as a C++ library (tighter integration but pins the project to one solver, adds build complexity). Rejected to keep solver choice in the user's hands.

**Trade-off**: Requires the solver binary to support the pipe output protocol (writing 'v', 'c', '!' lines). Users need a suitably instrumented solver build. The original satGraf project already has a `Pipe.cc` module for cadical/minisat that can be reused.

### Decision 5: CMake + system packages for build system

**Choice**: CMake for build configuration, system package manager (apt) for dependencies.

**Rationale**: All required libraries (igraph, Qt 6, Eigen3, OpenCL, nlohmann-json, CLI11, Catch2) are available in Ubuntu 24.04 apt repositories. No additional package manager needed. CMake's `find_package()` handles discovery.

**Alternative considered**: vcpkg or Conan for dependency management. Rejected because all dependencies are already available via apt, adding a package manager would be unnecessary complexity.

### Decision 6: Modular architecture with clear computation/rendering boundary

**Choice**: Separate core engine (no Qt dependency) from GUI layer. Core provides C interfaces: `Graph`, `Layout`, `CommunityDetector`, `EvolutionEngine`. GUI consumes these via Qt wrappers.

**Rationale**: Enables headless CLI mode without Qt linkage. Enables future alternative frontends (WebAssembly, Python bindings). Mirrors the original's clean Placer interface boundary.

```
┌─────────────────────────────────────┐
│  Qt GUI Layer                       │
│  ├── MainWindow (QML)               │
│  ├── GraphCanvas (QGraphicsScene)   │
│  └── ControlPanels (QML)            │
├─────────────────────────────────────┤
│  Application Layer (links both)     │
│  ├── AppController                  │
│  └── CLI Mode                       │
├─────────────────────────────────────┤
│  Core Engine (no Qt dependency)     │
│  ├── dimacs::Parser                 │
│  ├── graph::Graph / CommunityGraph  │
│  ├── community::Detector (igraph)   │
│  ├── layout::FruchtermanReingold    │
│  ├── layout::ForceAtlas2            │
│  ├── layout::OpenCLPlacer           │
│  ├── evolution::Engine              │
│  └── export::ImageExporter          │
├─────────────────────────────────────┤
│  External Libraries                 │
│  igraph | OpenCL | Eigen            │
│  External Solver (user binary, -s)  │
└─────────────────────────────────────┘
```

## Risks / Trade-offs

**[Risk] OpenCL driver compatibility on AMD iGPU** → Mitigation: Test early with a minimal OpenCL compute test. Fall back to CPU multi-threaded layout if GPU fails. The CPU ForceAtlas2 with Barnes-Hut already handles 100K nodes well.

**[Risk] igraph API learning curve** → Mitigation: igraph has excellent C documentation. The API surface needed is small (3-4 functions for community detection, graph construction). Prototype the integration early.

**[Risk] Qt QGraphicsScene performance at extreme scale (1M+ nodes)** → Mitigation: Use LOD (Level of Detail) rendering — at low zoom, render only community bounding boxes. Use spatial indexing for frustum culling. Benchmark early with synthetic large graphs.

**[Risk] External solver pipe protocol compatibility** → Mitigation: The pipe output format ('v', 'c', '!' lines) is defined by the original satGraf project's Pipe.cc. Document the protocol clearly. Provide Pipe.cc as a standalone file that users can compile into any supported solver. Gracefully handle malformed pipe output.

**[Risk] C++ memory management in complex graph algorithms** → Mitigation: Use smart pointers (unique_ptr, shared_ptr) throughout. RAII for OpenCL resources. The graph data structures use contiguous containers (vectors) not pointer-heavy linked structures.

**[Risk] Build complexity with many dependencies** → Mitigation: All dependencies available via apt on Ubuntu 24.04. CMakeLists.txt uses standard find_package(). No custom package manager required.
