## Context

The current render pipeline in `main_window.hpp` uses `std::thread(...).detach()` (line ~209) to run community detection and layout in the background. The Pause button sets `render_cancel_` but does not join the thread. Post-layout computation (bridge nodes, quotient graph, FA2/FA3D layouts) runs on the GUI thread in `on_render_complete()`, causing freezes on large graphs.

During evolution mode, `solver_timer_` calls `engine_->process_line()` every 100ms, modifying `graph_` edges on the GUI thread. If a render thread is simultaneously reading `graph_`, this creates a data race.

The existing progress pattern uses `QMetaObject::invokeMethod` with `Qt::QueuedConnection` to log progress from background threads — this works well and will be extended.

## Goals / Non-Goals

**Goals:**
- Eliminate race conditions between render threads and evolution engine
- Move all post-layout computation (bridge nodes, quotient data, FA3D layouts) off the GUI thread
- Report progress for each post-layout stage via the existing log mechanism
- Ensure old render results never overwrite new ones

**Non-Goals:**
- Changing core library APIs (`evolution.hpp`, `layout.hpp`, `solver.hpp`)
- Adding progress bar UI widgets — progress shown via log only
- Making layout algorithms themselves cancellable mid-computation (already handled by `render_cancel_` check between stages)
- Thread-safe `graph_` for concurrent reads — we ensure exclusive access instead

## Decisions

### D1: Managed thread member instead of detach

**Decision:** Store render thread as `std::optional<std::thread> render_thread_` member. Before starting a new render, join the old thread if running.

**Rationale:** `detach()` makes it impossible to guarantee the old thread has finished before starting a new one. A managed thread gives us join-before-start semantics.

**Alternative considered:** Thread pool / `std::async` — overkill for a single-threaded pipeline, adds complexity with no benefit.

### D2: Generation counter for stale result detection

**Decision:** Add `std::atomic<uint64_t> render_generation_{0}`. Increment on each new render. The background thread captures the generation at start and passes it with results. `on_render_complete` / `on_post_layout_complete` discard results if generation doesn't match current.

**Rationale:** Even with managed threads, `QMetaObject::invokeMethod` is asynchronous — by the time the GUI thread processes the callback, the user may have started another render. The generation counter is lightweight and eliminates stale-overwrite bugs.

### D3: Two-stage background pipeline

**Decision:** Split the render pipeline into two stages, both running on the same background thread:

1. **Stage 1 (existing):** Community detection → Layout computation (already backgrounded)
2. **Stage 2 (new):** Bridge nodes → Quotient graph → Community FA2 centers → Full-graph FA3D → Quotient FA3D

Both stages run in the worker thread. Between stages and during iterative layouts, check `render_cancel_` and report progress via `QMetaObject::invokeMethod` + log.

**Rationale:** All heavy computation stays off the GUI thread. The final `on_render_complete` callback only does lightweight GUI updates (setting labels, calling `perform_render()` which is fast since data is pre-computed).

### D4: Stop solver before rendering

**Decision:** In `render_graph()`, if `evolution_active_` is true, stop the solver and join the engine before starting the render thread.

**Rationale:** The solver modifies `graph_` edges. During rendering we need a stable graph. Stopping the solver is clean — the user explicitly chose to render, which implies they want to re-analyze.

### D5: No mutex on graph_ — exclusive access pattern

**Decision:** Don't add mutexes to `graph_`. Instead, ensure only one thread accesses it at a time by joining before starting new work and stopping the solver.

**Rationale:** `graph_` is accessed by: (1) render thread for detection/layout, (2) GUI thread for evolution events, (3) GUI thread for rendering to scene. With managed threads + solver stop, these never overlap. Adding a mutex would require changes to core library headers.

## Risks / Trade-offs

- **[Solver stop during render]** → User loses solver progress. **Mitigation:** Log a message explaining the solver was stopped. The solver can be restarted after render.
- **[Thread join blocking GUI]** → Joining a thread on the GUI thread could freeze. **Mitigation:** The old thread will be near completion since it checks `render_cancel_` frequently. In practice the join is <100ms.
- **[Memory usage]** → Pre-computing all layout variants (2D quotient, 3D full, 3D quotient) even if user only views one mode. **Mitigation:** This is the existing behavior — just moved to background thread. No change in memory.
