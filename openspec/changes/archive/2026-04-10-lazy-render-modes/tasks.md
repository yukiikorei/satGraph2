## Implementation Tasks

### Phase 1: LayoutFactory Mode Tags (layout.hpp)

- [x] 1.1 Add `enum class LayoutMode { Detailed2D, Simple2D, Mode3D, Simple3D }` near the LayoutFactory class
- [x] 1.2 Change `creators_` from `map<string, Creator>` to `map<string, pair<Creator, std::set<LayoutMode>>>` to store mode tags alongside each creator
- [x] 1.3 Update `register_layout()` to accept optional mode set parameter
- [x] 1.4 Add `available_algorithms(LayoutMode mode)` overload that filters by mode
- [x] 1.5 Keep existing `available_algorithms()` (no args) for backward compatibility
- [x] 1.6 Tag existing registrations: f/fgpu/forceAtlas2/kk/c/grid/gkk → Detailed2D; fa3d → Mode3D

### Phase 2: CommunityWeightedLayout Algorithm (layout.hpp)

- [x] 2.1 Implement `CommunityWeightedLayout` class extending `Layout` — force-directed on quotient graph with node mass = community size, attraction proportional to edge weight
- [x] 2.2 Implement `CommunityWeighted3DLayout` class producing `Coordinate3DMap` — same force model in 3D
- [x] 2.3 Register both in LayoutFactory: `community-fa` tagged Simple2D, `community-fa3d` tagged Simple3D

### Phase 3: GUI Layout Dropdown Coupling (main_window.hpp)

- [x] 3.1 Add method `update_layout_combo_for_mode()` that clears and repopulates `layout_combo_` based on current `render_mode_` using `LayoutFactory::available_algorithms(mode)`
- [x] 3.2 Call `update_layout_combo_for_mode()` when render mode changes (in `on_render_mode_changed()`)
- [x] 3.3 On startup, call `update_layout_combo_for_mode()` to initialize correctly
- [x] 3.4 Store selected layout name per mode so switching modes restores previous selection

### Phase 4: Lazy Per-Mode Layout Computation (main_window.hpp)

- [x] 4.1 Replace single `coords_`/`coords_3d_`/`community_centers_`/`community_centers_3d_` with per-mode cache: `std::unordered_map<RenderMode, PostLayoutResult> mode_cache_`
- [x] 4.2 Refactor background thread in `render_graph()` to compute only the current mode's layout
- [x] 4.3 On mode switch: check `mode_cache_` — if cached, call `perform_render()` immediately; if not, trigger background computation for the new mode
- [x] 4.4 On new Render: clear `mode_cache_`, compute current mode only
- [x] 4.5 Ensure `perform_render()` reads from `mode_cache_[render_mode_]` instead of global variables

### Phase 5: Mode-Specific Background Computation

- [x] 5.1 Detailed2D: community detection → selected 2D layout → bridge nodes → populate stats
- [x] 5.2 Simple2D: community detection → build quotient graph → community-fa layout on quotient → no bridge nodes needed
- [x] 5.3 Mode3D: community detection → FA3D on full graph → bridge nodes → populate stats
- [x] 5.4 Simple3D: community detection → build quotient graph → community-fa3d on quotient → no bridge nodes needed
- [x] 5.5 Each mode's result stored in `mode_cache_[mode]`

### Phase 6: Verification

- [x] 6.1 Build and verify compilation
- [x] 6.2 Run full test suite — expect 130/130 pass
- [x] 6.3 Add unit test for CommunityWeightedLayout: small quotient graph, verify output coordinates
- [x] 6.4 Add unit test for CommunityWeighted3DLayout: same with 3D output
- [x] 6.5 Manually verify: layout dropdown filters correctly per mode
- [x] 6.6 Manually verify: switch modes — cached modes instant, uncached trigger background computation
- [x] 6.7 Manually verify: Render clears cache and recomputes only current mode
