## Implementation Tasks

### Phase 1: Thread Safety Foundation

- [x] 1.1 Add `std::optional<std::thread> render_thread_` member to MainWindow private section
- [x] 1.2 Add `std::atomic<uint64_t> render_generation_{0}` member to MainWindow private section
- [x] 1.3 Add `#include <optional>` and `#include <thread>` to main_window.hpp includes (if not already present)

### Phase 2: Replace detach() with Managed Thread

- [x] 2.1 In `render_graph()`: before starting new thread, join existing `render_thread_` if it has a value (call `render_thread_->join()` then reset)
- [x] 2.2 In `render_graph()`: if `evolution_active_` is true, stop solver (`solver_.cancel()`, stop timer, reset UI), set `evolution_active_ = false`, enable `render_mode_combo_`, log "Solver stopped for re-render"
- [x] 2.3 In `render_graph()`: increment `render_generation_`, capture `uint64_t gen = render_generation_.load()` for the closure
- [x] 2.4 Replace `std::thread(...).detach()` with `render_thread_.emplace(std::thread(...))` — the lambda now captures `gen`
- [x] 2.5 In the background thread lambda: pass `gen` to the `on_render_complete` invokeMethod call (add generation parameter)
- [x] 2.6 In `on_render_complete`: add `uint64_t gen` parameter, check `gen != render_generation_.load()` at the top — if stale, return immediately
- [x] 2.7 Update the `Q_ARG` types in `QMetaObject::invokeMethod` calls to include generation parameter

### Phase 3: Move Post-Layout Computation to Background Thread

- [x] 3.1 In the background thread lambda (after layout completes), add progress log: "Computing bridge nodes..."
- [x] 3.2 Move `compute_bridge_nodes()` call from `on_render_complete()` into the background thread, store result in a local variable
- [x] 3.3 Add progress log: "Computing quotient layout..."
- [x] 3.4 Move `compute_quotient_data()` call from `on_render_complete()` into the background thread — but `compute_quotient_data()` writes to member variables (quotient_graph_, community_centers_, etc.), so extract its return values into local variables in the thread, then pass them to a new `on_post_layout_complete` callback
- [x] 3.5 Create a struct `PostLayoutResult` holding: bridge_nodes set, quotient_graph, community_centers, community_centers_3d, community_sizes, community_ids, inter_community_edges, coords_3d
- [x] 3.6 Add progress log: "Computing 3D layout..." before FA3D compute3D calls
- [x] 3.7 Check `render_cancel_` between each post-layout stage — if cancelled, invoke `on_render_cancelled` and return
- [x] 3.8 At the end of the background thread, invoke `on_render_complete` with the PostLayoutResult struct plus communities and coords (all pre-computed)

### Phase 4: Simplify on_render_complete

- [x] 4.1 Rewrite `on_render_complete` to accept the pre-computed `PostLayoutResult` — just move the data into member variables and do lightweight GUI updates
- [x] 4.2 Remove heavy computation from `on_render_complete` (bridge nodes, compute_quotient_data, FA3D calls — all now in background thread)
- [x] 4.3 Ensure `perform_render()` only uses pre-computed data (no layout computation)

### Phase 5: Verification

- [x] 5.1 Build and verify compilation with `cmake --build build -j$(nproc)`
- [x] 5.2 Run full test suite `ctest --test-dir build --output-on-failure` — expect 130/130 pass
- [x] 5.3 Manually verify: render a graph, check log shows multi-stage progress
- [x] 5.4 Manually verify: click Pause then Render with different algorithm — no crash
- [x] 5.5 Manually verify: start solver, then click Render — solver stops, render proceeds
- [x] 5.6 Manually verify: GUI remains responsive during post-layout computation
