# satGraf

A C++17 rewrite of [satGraf](https://github.com/Vignesh-Desmond/satGraf) — a visualization tool for SAT solver instances and their runtime evolution. It parses DIMACS CNF files, detects variable communities, computes force-directed graph layouts (CPU and GPU via OpenCL), and renders interactive visualizations with a Qt 6 GUI or headless image export.

## Features

### Core Library (header-only)

- **DIMACS CNF parser** — reads standard DIMACS format into a variable interaction graph (VIG) or literal interaction graph (LIG)
- **Graph model** — typed graph with nodes, edges, community IDs, and variable assignment tracking
- **Community detection** — Louvain and CNM algorithms (via igraph) with factory-based registration
- **Layout engine (CPU)** — Fruchterman-Reingold, Kamada-Kawai, ForceAtlas2, and community-aware variants (circular, grid, grid-KK)
- **Layout engine (GPU)** — OpenCL-accelerated Fruchterman-Reingold with automatic CPU fallback
- **SAT solver integration** — spawns external solvers via named FIFOs (POSIX), parses the `v`/`c`/`!` event protocol, with RAII process management and crash detection
- **Evolution engine** — forward/backward replay of solver events, conflict scanning, decision variable tracking, observer pattern for real-time UI updates, file buffering with background threads

### GUI (Qt 6)

- **Split pane window** — 70/30 canvas/control panel with resizable divider
- **Menu bar** — File (Open, Export), View (Zoom In/Out/Fit), Help (About)
- **Community mode controls** — community algorithm dropdown, layout algorithm dropdown, iteration slider
- **Evolution mode controls** — solver binary selector with path persistence (QSettings), step forward/backward buttons, conflict counter, timeline slider
- **Info panel** — node count, edge count, community count, modularity score
- **Interactive rendering** — zoom/pan via mouse wheel and drag, node click detection, scale-aware label visibility

### Rendering

- **Layered rendering** — edges (z=0), community regions (z=0.5), nodes (z=1), labels (z=2), decision highlight (z=3)
- **Community coloring** — 20-color palette for intra-community edges/nodes, gray for inter-community, red dashed for conflict edges
- **Visibility filters** — hide/show by community, assignment state, or edge type

### Export

- **Static image** — render to offscreen QImage, save as PNG or JPEG with configurable quality
- **Animated frames** — render evolution conflicts as numbered PNG frame sequence
- **Headless mode** — full pipeline (parse → community → layout → render → save) without opening a window

### CLI

```
satGraf — SAT solver visualization tool
Usage: satgraf [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  -i,--input TEXT:FILE        Input DIMACS CNF file
  -s,--solver TEXT            Path to SAT solver binary (required for evolution mode)
  -l,--layout TEXT [f]        Layout algorithm (f, fgpu, forceAtlas2, kk, c, grid, gkk)
  -m,--mode TEXT [com]        Visualization mode: com (community), imp (implication),
                               evo (evolution), exp (export)
  -o,--output TEXT            Output file for export mode (PNG/JPEG)
  --headless                  Run without GUI (for export/batch processing)
  --community TEXT [louvain]  Community detection method (louvain, cnm)
  --iterations INT:POSITIVE [500]
                              Number of layout iterations
```

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| CMake | ≥ 3.21 | Build system |
| C++ compiler | C++17 | GCC 11+, Clang 14+, MSVC 2022+ |
| igraph | ≥ 0.10 | Community detection (Louvain, CNM) |
| Qt 6 | Core, Gui, Widgets, Qml, Quick | GUI and image rendering |
| OpenCL | ≥ 1.2 | GPU-accelerated layout |
| Eigen3 | ≥ 3.4 | Matrix operations for layout algorithms |
| nlohmann_json | ≥ 3.0 | JSON serialization |
| CLI11 | ≥ 2.3 | Command-line argument parsing |
| Catch2 | ≥ 3.0 | Unit testing framework |

### Install dependencies (Ubuntu 24.04)

```bash
sudo apt install cmake g++ libigraph-dev libeigen3-dev \
    nlohmann-json3-dev libcli11-dev catch2 \
    qt6-base-dev qt6-declarative-dev \
    opencl-headers ocl-icd-opencl-dev
```

For systems without a GPU, the OpenCL dependency is still required at build time but the layout engine gracefully falls back to CPU at runtime.

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

The main executable is at `build/src/app/satgraf`.

## Usage

### Interactive GUI

```bash
# Open a CNF file in community visualization mode
./build/src/app/satgraf -i instance.cnf

# Use a specific layout algorithm
./build/src/app/satgraf -i instance.cnf -l forceAtlas2

# Evolution mode with a solver
./build/src/app/satgraf -i instance.cnf -m evo -s /path/to/solver
```

### Headless export

```bash
# Export a graph as PNG without opening a window
./build/src/app/satgraf -i instance.cnf --headless -o output.png

# Export as JPEG with custom settings
./build/src/app/satgraf -i instance.cnf --headless -m exp -o output.jpg \
    -l kk --community cnm --iterations 1000
```

## Testing

The project has two test executables:

- **satgraf_tests** — core library tests (no Qt required)
- **satgraf_gui_tests** — rendering, export, and integration tests (requires Qt, runs headless via `QT_QPA_PLATFORM=offscreen`)

```bash
# Build tests
cmake --build build -j$(nproc)

# Run all tests
ctest --test-dir build --output-on-failure

# Run only core tests
./build/tests/satgraf_tests

# Run only GUI tests (headless)
QT_QPA_PLATFORM=offscreen ./build/tests/satgraf_gui_tests
```

**126 tests** covering graph model, DIMACS parsing, community detection, layout algorithms (CPU and GPU), solver integration, evolution engine, rendering, export, and end-to-end pipelines.

## Project Structure

```
satGraf-rebuild/
├── CMakeLists.txt
├── src/
│   ├── core/include/satgraf/     # Header-only core library
│   │   ├── types.hpp             # Strong typedefs (NodeId, EdgeId, CommunityId)
│   │   ├── node.hpp              # Node with assignment, activity, community
│   │   ├── edge.hpp              # Edge with type (normal/conflict)
│   │   ├── graph.hpp             # Templated graph container
│   │   ├── dimacs_parser.hpp     # DIMACS CNF → graph
│   │   ├── community_detector.hpp # Louvain/CNM via igraph
│   │   ├── layout.hpp            # All layout algorithms + factory
│   │   ├── solver.hpp            # Named FIFO + external solver process
│   │   ├── evolution.hpp         # Forward/backward event replay engine
│   │   └── ...
│   ├── gui/include/satgraf_gui/  # Qt GUI module
│   │   ├── graph_renderer.hpp    # QGraphicsScene renderer + GraphView
│   │   ├── export.hpp            # QImage-based static/animated export
│   │   └── main_window.hpp       # Main window with control panels
│   └── app/
│       └── main.cpp              # CLI entry point (GUI/headless modes)
├── tests/
│   ├── test_graph_model.cpp
│   ├── test_dimacs_parser.cpp
│   ├── test_community_detection.cpp
│   ├── test_layout.cpp
│   ├── test_solver.cpp
│   ├── test_evolution.cpp
│   └── test_gui.cpp              # Rendering, export, E2E tests
├── SATGraf/                      # Original Java codebase (reference)
└── openspec/                     # OpenSpec change tracking
```

## Architecture

```
DIMACS CNF → Parser → Graph → Community Detector → Layout Engine → Renderer
                  ↓                                    ↑
            Solver (FIFO) → Event Parser → Evolution Engine
```

- **Core** is header-only with no Qt dependency — reusable as a library
- **GUI** links Qt for rendering and the interactive window
- **CLI** orchestrates both: GUI mode or headless export mode

## License

This project is a C++ rewrite of the original Java [satGraf](https://github.com/Vignesh-Desmond/satGraf) by Vignesh Desmond.
