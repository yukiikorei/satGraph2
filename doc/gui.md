# GUI Layer

## Rendering Engine (`graph_renderer.hpp`)

Namespace: `satgraf::rendering`

### Color Palette

20-color community palette (indexed by `community_id % 20`):

| Idx | Color | Idx | Color | Idx | Color | Idx | Color |
|-----|-------|-----|-------|-----|-------|-----|-------|
| 0 | `#E6194B` | 5 | `#911EB4` | 10 | `#008080` | 15 | `#AAFFC3` |
| 1 | `#3CB44B` | 6 | `#46F0F0` | 11 | `#DCBEFF` | 16 | `#808000` |
| 2 | `#FFE119` | 7 | `#F032E6` | 12 | `#AA6E28` | 17 | `#FFD7B4` |
| 3 | `#0082C8` | 8 | `#D2F53C` | 13 | `#FFFAC8` | 18 | `#000080` |
| 4 | `#F58230` | 9 | `#FABED4` | 14 | `#800000` | 19 | `#808080` |

Special colors: inter-community edges = `#B4B4B4`, conflict edges = `#FF0000`.

### Scene Graph (Z-Order)

```
Z=0.0  Edges          EdgeGraphicsItem        Solid/Dashed lines
Z=0.5  Regions        CommunityRegionItem      Semi-transparent bounding boxes + ID labels
Z=1.0  Nodes          NodeGraphicsItem         Filled circles with community colors
Z=2.0  Node Labels    QGraphicsSimpleTextItem  Variable names (hidden by default, shown on zoom)
Z=3.0  Highlight      DecisionHighlightItem    Yellow ring on decision variable
```

### QGraphicsItem Subclasses

| Class | Base | Key Properties |
|-------|------|---------------|
| `NodeGraphicsItem` | `QGraphicsEllipseItem` | `node_id()`, `community_id()`, `diameter()`, `set_community_color()` |
| `EdgeGraphicsItem` | `QGraphicsLineItem` | `edge_id()`, `edge_type()` (Normal=Solid, Conflict=Dashed) |
| `DecisionHighlightItem` | `QGraphicsEllipseItem` | Yellow ring at 1.5× node diameter |
| `CommunityRegionItem` | `QGraphicsRectItem` | Alpha-30 fill, dashed outline, community ID label at top-left |

### GraphRenderer

Core rendering class. Owns a `QGraphicsScene` and manages all visual items.

| Method | Description |
|--------|-------------|
| `render(graph, coords, communities, node_size, edge_width)` | Full re-render: edges → nodes → regions |
| `set_decision_variable(var, coords, diameter)` | Show/hide yellow highlight ring |
| `apply_filters(filters, communities)` | Show/hide nodes and edges by community/assignment/edge type |
| `set_label_visibility(zoom, threshold)` | Show labels only when zoomed past threshold |
| `set_community_regions_visible(bool)` | Toggle community bounding boxes |
| `add_labels(graph)` | Attach variable-name text to each node |
| `node_at(QPointF) -> optional<NodeId>` | Hit-test at scene coordinates |

### GraphView

Interactive zoom/pan view.

| Feature | Detail |
|---------|--------|
| Drag mode | `ScrollHandDrag` (left-click pan) |
| Zoom | Mouse wheel (1.15× per step), anchored under cursor |
| Signals | `node_clicked(NodeId)`, `background_clicked()`, `zoom_changed(double)` |

### VisibilityFilters

| Filter | Default | Controls |
|--------|---------|----------|
| `visible_communities` | empty (show all) | Community membership |
| `show_unassigned/assigned_true/assigned_false` | all true | Assignment state |
| `show_normal_edges / show_conflict_edges` | both true | Edge type |

---

## Main Window (`main_window.hpp`)

### Window Architecture

```
QMainWindow (FramelessWindowHint + WA_TranslucentBackground, 10px rounded corners)
 └── Central Widget
      ├── TitleBar (32px, custom close/min/max buttons, draggable, double-click maximize)
      └── QSplitter (horizontal)
           ├── Left Panel (256px) — Controls
           ├── GraphView (stretch) — Canvas
           └── Right Panel (230px) — Info + Log
```

### Left Panel Controls

| Control | Widget | Purpose |
|---------|--------|---------|
| File label | QLabel | Loaded filename |
| Open File... | QPushButton | File dialog |
| Community | QComboBox | Detection algorithm selector |
| Layout | QComboBox | Layout algorithm selector |
| Background | QComboBox | Dark/White/Light Gray/Transparent |
| Iterations | QSlider (50–2000) | Layout iteration count |
| Node Size | QSlider (2–40) | Node diameter, live re-render |
| Show Community Regions | QCheckBox | Toggle region overlay |
| ▶ Render / ⏸ Pause | QPushButton | Threaded render / cancel |
| Solver path | QLineEdit + browse | Solver binary, persisted via QSettings |
| ▶ Start Solver / ■ Stop | QPushButton | Start/stop external solver |
| ◀ Step Back + Timeline | QPushButton + QSlider | Evolution history navigation |
| C: N | QLabel | Conflict counter |

### Right Panel

| Section | Content |
|---------|---------|
| Statistics | Nodes, Edges, Communities, Modularity |
| Community | Dropdown with color icons; detail (ID, color, node count) |
| Selected Node | Node ID, Community ID, color (on click) |
| Log | Single-line status at bottom |

### Threaded Rendering Pipeline

```
[UI Thread]                           [Worker Thread]
  Render clicked
    ├─ set_controls_enabled(false)
    ├─ rendering_active_ = true
    ├─ Button → "⏸ Pause"
    │                                  ├─ detect communities
    │                                  ├─ check cancel flag
    │                                  ├─ compute layout (progress via invokeMethod)
    │                                  ├─ check cancel flag
    │                                  └─ invokeMethod("on_render_complete")
  on_render_complete:
    ├─ store communities + coords
    ├─ renderer_->render(...)
    ├─ fit_all()
    ├─ rendering_active_ = false
    └─ Button → "▶ Render"

  Pause clicked:
    ├─ render_cancel_ = true
    ├─ rendering_active_ = false
    ├─ Button → "▶ Render"
    └─ set_controls_enabled(true)
```

Thread safety: `std::atomic<bool>` flags for `rendering_active_` and `render_cancel_`. Cross-thread UI updates via `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`.

### Solver Integration

1. Start Solver → fork+exec external process via `ExternalSolver`
2. 100ms QTimer polls FIFO → reads up to 50 lines per tick
3. Each line fed to `EvolutionEngine::process_line()`
4. UI updates: conflict count, timeline slider range, step-back button state
5. Step backward → `engine_->step_backward()` → re-render with decision variable highlight

---

## Export Module (`export.hpp`)

Namespace: `satgraf::export_`

| Function | Description |
|----------|-------------|
| `render_to_image(graph, coords, communities, QImage&, ...)` | Core offscreen renderer to existing QImage |
| `export_png(graph, coords, communities, path, w, h)` | Save as PNG (default 1024×1024) |
| `export_jpeg(graph, coords, communities, path, w, h, quality)` | Save as JPEG (quality 0–100, default 95) |
| `render_evolution_frames(graph, event_lines, mode, w, h)` | Capture conflict snapshots as QImage vector |
| `save_gif_frames(frames, path, delay_ms)` | Save numbered PNGs for animated sequence |
| `render_headless(graph, coords, communities, w, h)` | Convenience: create QImage + render + return |

---

## CLI (`main.cpp`)

### Flags

| Flag | Default | Description |
|------|---------|-------------|
| `-i, --input` | — | Input DIMACS CNF file |
| `-s, --solver` | — | SAT solver binary path |
| `-l, --layout` | `f` | Layout algorithm |
| `-m, --mode` | `com` | Mode: `com`/`imp`/`evo`/`exp` |
| `-o, --output` | — | Output file (PNG/JPEG) |
| `--headless` | false | Run without GUI |
| `--community` | `louvain` | Community detection algorithm |
| `--iterations` | 500 | Layout iterations |

### Modes

| Mode | Behavior |
|------|----------|
| GUI (default) | Full interactive window, `-i` optional |
| `--headless` | Parse → detect → layout → export, no window |
| `-m exp -o file.png` | Export-only mode |
| `-m evo -s solver` | GUI with solver integration |

### Validation

- Headless/export mode requires `-i`
- Evolution mode requires `-s`
- Solver path must exist
- Export mode requires `-o`
