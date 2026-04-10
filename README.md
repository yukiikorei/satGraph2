# satGraf2

A C++17 rewrite of [SATGraf](https://github.com/ekuiter/SATGraf) — a visualization tool for SAT solver instances and their runtime evolution. This project is based on the research from [MapleSAT publications](https://maplesat.github.io/publications). It parses DIMACS CNF files, detects variable communities, computes force-directed graph layouts (CPU and GPU via OpenCL), and renders interactive visualizations with a Qt 6 GUI or headless image export.

## Features

### What's New Over Original SATGraf

- **Four rendering modes** — Detailed 2D (per-node), Simple 2D (quotient graph), full 3D (OpenGL with arcball rotation), and Simple 3D (quotient graph spheres)
- **ForceAtlas3D layout** — extends ForceAtlas2 forces (repulsion, attraction, gravity) into 3D space for spatial graph visualization
- **Interactive 3D community highlighting** — select a community to highlight its nodes/edges with brightness and size scaling; linked nodes in neighboring communities are highlighted individually (not the entire community)
- **Enriched info panel** — graph-level internal/external edge counts, per-community detail (bridge nodes, linked communities), per-node detail (internal/external neighbors, linked communities), all in consistent QFormLayout grids
- **Node text search** — type a node ID or name to select and highlight it, as an alternative to click selection
- **Configurable edge size** — edge width slider affects all four rendering modes
- **DEB packaging** — one-command `cpack` produces a `.deb` with desktop entry, hicolor icons (PNG + SVG), and dependency declarations

### Core Library (header-only)

- **DIMACS CNF parser** — reads standard DIMACS format into a variable interaction graph (VIG) or literal interaction graph (LIG)
- **Graph model** — typed graph with nodes, edges, community IDs, and variable assignment tracking
- **Community detection** — Louvain and CNM algorithms (via igraph) with factory-based registration
- **Layout engine (CPU)** — Fruchterman-Reingold, Kamada-Kawai, ForceAtlas2, ForceAtlas3D, and community-aware variants (circular, grid, grid-KK)
- **Layout engine (GPU)** — OpenCL-accelerated Fruchterman-Reingold with automatic CPU fallback
- **SAT solver integration** — spawns external solvers via named FIFOs (POSIX), parses the `v`/`c`/`!` event protocol, with RAII process management and crash detection
- **Evolution engine** — forward/backward replay of solver events, conflict scanning, decision variable tracking, observer pattern for real-time UI updates, file buffering with background threads

### GUI (Qt 6)

- **Split pane window** — 70/30 canvas/control panel with resizable divider
- **Four rendering modes** — switch between Detailed 2D, Simple 2D (quotient graph), 3D (full graph, OpenGL), and Simple 3D (quotient spheres) from a dropdown
- **Menu bar** — File (Open, Export), View (Zoom In/Out/Fit), Help (About)
- **Community mode controls** — community algorithm dropdown, layout algorithm dropdown, iteration slider (1–500), node size slider (1–25), edge size slider (1–10)
- **Evolution mode controls** — solver binary selector with path persistence (QSettings), step forward/backward buttons, conflict counter, timeline slider
- **Enriched info panel** — graph statistics (nodes, edges, communities, modularity, internal/external edges), community detail (nodes, bridge nodes, internal/external edges, linked communities), node detail (ID, community, internal/external neighbors, linked communities)
- **Community selection** — dropdown or click to select a community; highlight toggle checkbox; z-order promotion in 2D, brightness/size highlighting in 3D
- **Node search** — text input for node selection by ID or name, synced with click selection
- **Interactive rendering** — zoom/pan via mouse wheel and drag, node click detection, scale-aware label visibility, arcball rotation + scroll zoom in 3D modes

### Rendering

- **Four rendering modes:**
  - *Detailed 2D* — per-node rendering with layered z-order (edges z=0, community regions z=0.5, nodes z=1, labels z=2, decision highlight z=3)
  - *Simple 2D* — quotient graph: one circle per community, sized by node count, edges weighted by inter-community edge count
  - *3D* — full graph rendered with OpenGL (QOpenGLWidget), ForceAtlas3D layout, per-node spheres with community coloring, arcball rotation, scroll zoom
  - *Simple 3D* — quotient graph in 3D: community spheres sized by node count, weighted inter-community edges, perspective projection
- **Community coloring** — 20-color palette for intra-community edges/nodes, gray for inter-community, red dashed for conflict edges
- **3D community highlighting** — selected community nodes rendered at full brightness with 1.3× radius; linked neighbor-community nodes highlighted individually; all other nodes dimmed to 0.1 alpha
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

| Library       | Version                        | Purpose                                 |
| ------------- | ------------------------------ | --------------------------------------- |
| CMake         | ≥ 3.21                         | Build system                            |
| C++ compiler  | C++17                          | GCC 11+, Clang 14+, MSVC 2022+          |
| igraph        | ≥ 0.10                         | Community detection (Louvain, CNM)      |
| Qt 6          | Core, Gui, Widgets, OpenGL, OpenGLWidgets, Qml, Quick | GUI, 3D rendering, and image rendering |
| OpenCL        | ≥ 1.2                          | GPU-accelerated layout                  |
| Eigen3        | ≥ 3.4                          | Matrix operations for layout algorithms |
| nlohmann_json | ≥ 3.0                          | JSON serialization                      |
| CLI11         | ≥ 2.3                          | Command-line argument parsing           |
| Catch2        | ≥ 3.0                          | Unit testing framework                  |

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

### macOS one-shot build/dependency setup

On macOS, run:

```bash
./build_macos.sh
```

This script:
- installs missing Homebrew dependencies (`igraph`, `qt`, `eigen`, `nlohmann-json`, `cli11`, `catch2`, etc.)
- configures and builds `satgraf` in `build-macos`
- runs tests
- runs CPack and checks that a `.dmg` package contains `satgraf.app`
- runs a smoke check via `satgraf.app/Contents/MacOS/satgraf --help`

Useful toggles:

```bash
RUN_TESTS=0 RUN_PACKAGE=0 ./build_macos.sh
BUILD_DIR=build-macos-debug BUILD_TYPE=Debug ./build_macos.sh
```

### Ubuntu one-shot build/dependency setup

On Ubuntu, run:

```bash
./build_ubuntu.sh
```

This script:
- installs missing apt dependencies needed for configure/build/test/package
- configures and builds `satgraf` in `build-ubuntu`
- runs tests with `QT_QPA_PLATFORM=offscreen`
- runs CPack and verifies that a `.deb` package is generated
- runs a smoke check via `satgraf --help`

Useful toggles:

```bash
RUN_TESTS=0 RUN_PACKAGE=0 ./build_ubuntu.sh
BUILD_DIR=build-ubuntu-debug BUILD_TYPE=Debug ./build_ubuntu.sh
BUILD_JOBS=8 ./build_ubuntu.sh
```

### Linux compatibility regression check

Use the standard Linux flow to verify non-macOS paths remain valid:

```bash
cmake -S . -B build-linux -DCMAKE_BUILD_TYPE=Release
cmake --build build-linux -j$(nproc)
ctest --test-dir build-linux --output-on-failure
./build-linux/src/app/satgraf --help
```

### Release automation

Pushing a Git tag that matches `pac.*` triggers GitHub Actions release packaging on both macOS and Ubuntu runners. The workflow uploads generated `.dmg` and `.deb` files as workflow artifacts and attaches them to the GitHub Release for that tag.

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

**130 tests** covering graph model, DIMACS parsing, community detection, layout algorithms (CPU and GPU), 3D layout, solver integration, evolution engine, rendering, export, and end-to-end pipelines.

## Project Structure

```
satGraf2/
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
│   │   ├── graph_view_3d.hpp     # OpenGL 3D widget (QOpenGLWidget)
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

## AI-Driven Development

This project is **entirely AI-generated** and developed using the [OpenSpec](https://github.com/openspec-dev/openspec) specification-driven workflow. All significant features are implemented through a structured process:

1. **Proposal** — describe the change intent
2. **Design** — document the technical approach
3. **Specs** — define requirements and acceptance scenarios per capability
4. **Tasks** — break down into atomic, verifiable implementation steps
5. **Implement** — execute tasks against the spec
6. **Archive** — sync delta specs back to main specs and archive the change

No feature is merged without corresponding spec updates. The `openspec/` directory in this repository contains the full change history, including proposals, designs, specs, and task tracking for every feature.

```
openspec/
├── specs/                  # Current capability specifications
│   ├── gui/spec.md
│   ├── rendering/spec.md
│   ├── rendering-modes/spec.md
│   ├── layout-engine/spec.md
│   ├── community-z-order/spec.md
│   ├── node-text-selection/spec.md
│   └── info-panel-enrichment/spec.md
└── changes/archive/        # Completed change histories
    └── 2026-04-09-enrich-info-panel/
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

This project is licensed under the [MIT License](LICENSE) by Jinghu LIANG.

It is a C++ rewrite of the original Java [SATGraf](https://github.com/ekuiter/SATGraf), based on research from [MapleSAT publications](https://maplesat.github.io/publications).
