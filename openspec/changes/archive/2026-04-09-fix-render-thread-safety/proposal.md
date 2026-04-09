## Why

The render pipeline has two critical issues: (1) `std::thread(...).detach()` with no join/wait creates race conditions when the user clicks Pause then Render — old and new threads access `graph_` concurrently, causing crashes. Evolution mode compounds this by modifying `graph_` edges every 100ms on the GUI thread. (2) After background layout completes, `on_render_complete()` runs multiple iterative FA2/FA3D layouts on the GUI thread (bridge nodes, quotient graph, 3D layouts), freezing the window for seconds on large instances.

## What Changes

- **Replace detached render thread with managed thread** — store `std::thread` member, join before starting new render
- **Move post-layout computation off GUI thread** — bridge nodes, quotient graph construction, community FA2 layout, and FA3D layouts all run in the background worker thread
- **Add progress logging for post-layout stages** — "Computing bridge nodes...", "Computing quotient layout...", "Computing 3D layout..." via existing log mechanism
- **Stop solver before rendering** — if evolution is active when Render is clicked, stop the solver and wait before starting the render thread
- **Guard `on_render_complete` against stale callbacks** — use a generation counter to discard results from cancelled/superseded renders

## Capabilities

### New Capabilities

- `render-pipeline`: Background render pipeline with managed threads, progress reporting, and thread-safe state management

### Modified Capabilities

- `gui`: Render button behavior changes from detach-and-forget to managed thread with join; evolution mode stops solver before render
- `rendering-modes`: Post-layout computations (quotient data, FA3D) move from GUI thread to background thread

## Impact

- **`src/gui/include/satgraf_gui/main_window.hpp`** — primary file: render thread management, `on_render_complete` refactored, new `on_post_layout_complete` callback
- **No core library changes** — `evolution.hpp`, `layout.hpp`, `solver.hpp` are untouched
- **No new dependencies** — uses existing `std::thread`, `std::atomic`, `QMetaObject::invokeMethod`
