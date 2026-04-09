# satGraf Architecture

## Overview

satGraf is a C++17 desktop application for visualizing SAT solver behavior through graph-based community analysis. It parses DIMACS CNF files into variable interaction graphs, detects community structure, computes force-directed layouts, and renders interactive visualizations with real-time solver event tracking.

## Project Structure

```
satGraf-rebuild/
├── src/
│   ├── core/           # Algorithm library (no Qt dependency)
│   │   └── include/satgraf/
│   │       ├── types.hpp              # Strong typedefs (NodeId, EdgeId, CommunityId)
│   │       ├── node.hpp               # Graph node with SAT assignment state
│   │       ├── edge.hpp               # Graph edge with visibility/type
│   │       ├── clause.hpp             # CNF clause as (NodeId -> polarity) map
│   │       ├── graph.hpp              # Templated graph container
│   │       ├── csr.hpp                # Compressed Sparse Row adapter
│   │       ├── union_find.hpp         # Union-Find with path compression
│   │       ├── dimacs_parser.hpp      # DIMACS CNF file parser (VIG/LIG)
│   │       ├── dimacs_writer.hpp      # DIMACS CNF file writer
│   │       ├── community_detector.hpp # Community detection (Louvain, CNM, Online)
│   │       ├── community_graph.hpp    # Community-aware graph with modularity stats
│   │       ├── community_node.hpp     # Extended node with community membership
│   │       ├── layout.hpp             # 8 layout algorithms + factories
│   │       ├── evolution.hpp          # Forward/backward solver event replay
│   │       └── solver.hpp             # External solver process + FIFO IPC
│   ├── gui/            # Qt 6 visualization layer
│   │   └── include/satgraf_gui/
│   │       ├── graph_renderer.hpp     # QGraphicsScene rendering pipeline
│   │       ├── main_window.hpp        # Main window + custom title bar
│   │       └── export.hpp             # PNG/JPEG/frame export
│   └── app/
│       └── main.cpp                   # CLI entry (GUI / headless / export modes)
├── tests/              # Catch2 test suite (126 tests)
├── doc/                # Documentation
└── CMakeLists.txt      # Build system + CPack DEB packaging
```

## Data Flow

```
DIMACS CNF file
     │
     ▼
 ┌─────────────┐     ┌──────────────────────┐
 │  Parser     │────▶│  Graph<Node, Edge>    │
 │  (VIG/LIG)  │     │  + Clauses            │
 └─────────────┘     └──────────┬───────────┘
                                │
                    ┌───────────┼───────────┐
                    ▼           ▼           ▼
             ┌───────────┐ ┌────────┐ ┌──────────┐
             │ Community │ │ Layout │ │ Evolution│
             │ Detector  │ │ Engine │ │ Engine   │
             └─────┬─────┘ └───┬────┘ └────┬─────┘
                   │           │           │
                   ▼           ▼           ▼
             CommunityResult  CoordinateMap  History
                   │           │           │
                   └───────────┼───────────┘
                               ▼
                     ┌──────────────────┐
                     │  GraphRenderer   │
                     │  (QGraphicsScene)│
                     └────────┬─────────┘
                              │
                    ┌─────────┼─────────┐
                    ▼         ▼         ▼
               GraphView   Export    Evolution
               (interactive PNG/JPEG  replay
                zoom/pan)  frames)   controls)
```

## Key Design Decisions

| Decision | Rationale |
|----------|-----------|
| Header-only core library | Compile-time optimization, easy test inclusion |
| Strong typedefs for IDs | Prevents accidental mixing of NodeId/EdgeId/CommunityId at compile time |
| Factory pattern (Layout, Detector) | Runtime algorithm selection without recompilation |
| `std::thread` for rendering | UI responsiveness during O(n^2) layout computation |
| POSIX named FIFOs | Standard IPC for external solver integration |
| Snapshot-based undo | O(1) backward step in evolution engine |
| Qt Graphics Framework | Hardware-accelerated 2D rendering with built-in interaction |

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| Qt 6 | 6.4+ | GUI framework (Core, Gui, Widgets) |
| igraph | 0.10+ | Community detection algorithms (Louvain, CNM) |
| OpenCL | 1.2 | GPU-accelerated Fruchterman-Reingold layout |
| Eigen3 | 3.4+ | Linear algebra for layout algorithms |
| nlohmann_json | 3.x | JSON configuration |
| CLI11 | 2.x | Command-line argument parsing |
| Catch2 | 3.x | Unit testing framework |

## Build & Package

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build

cd build && cpack -G DEB    # → satgraf-0.1.0-Linux.deb
```

See individual module docs for algorithm details:

- [Core Modules](core-modules.md) — Graph, Parser, Types, CSR, Union-Find
- [Algorithms](algorithms.md) — Layout algorithms, Community detection
- [Evolution Engine](evolution.md) — Solver event replay, FIFO protocol
- [GUI Layer](gui.md) — Renderer, Main Window, Export, CLI
