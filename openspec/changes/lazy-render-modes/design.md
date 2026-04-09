## Context

The render pipeline currently computes all 4 layout variants on every Render click. The user selects a render mode but the layout algorithm dropdown is global — showing all algorithms regardless of mode. Simple 2D/3D modes reuse FA2/FA3D designed for node-level graphs, missing the opportunity to use inter-community edge weights as the primary layout driver.

## Goals / Non-Goals

**Goals:**
- Render only computes the layout for the currently selected mode
- Layout dropdown filters to show only valid algorithms per mode
- New community-weighted layout algorithm for Simple modes (2D + 3D)
- Cached per-mode layouts so switching back is instant
- Community detection shared across modes (computed once)

**Non-Goals:**
- Changing the 3D OpenGL rendering code (graph_view_3d.hpp)
- Changing existing layout algorithms (FR, KK, FA2, FA3D)
- Adding new render modes beyond the existing 4

## Decisions

### D1: LayoutFactory gains mode tags

Add an `enum class LayoutMode { Detailed2D, Simple2D, Mode3D, Simple3D }` and tag each registered algorithm with its compatible modes. `available_algorithms(LayoutMode)` returns only matching ones.

**Rationale:** Simplest approach — no new factory classes, just filtering.

**Alternative:** Separate factory per mode — over-engineered, duplicates registration logic.

### D2: Per-mode cached results

Store layout results in a `std::unordered_map<RenderMode, PostLayoutResult>` cache. When switching modes, check cache first. If cache miss, compute in background and display when ready.

**Rationale:** Clean separation — each mode owns its data. Cache invalidation on new Render is simple (clear all).

### D3: CommunityWeightedLayout algorithm

Force-directed layout operating on quotient graph:
- Node mass = community size (larger communities repel more)
- Attraction = proportional to edge weight (inter-community edge count)
- Repulsion = proportional to product of masses (standard FA2)
- No gravity (communities should spread naturally)

**Rationale:** This is a simplified FA2 adapted for the quotient graph's structure. The key difference from existing FA2 is that edge weights drive attraction directly rather than being an optional influence parameter.

### D4: Layout dropdown updates on mode change

When render_mode_combo_ changes, repopulate layout_combo_ with mode-filtered algorithms. If current selection is invalid for new mode, auto-select first valid algorithm.

## Risks / Trade-offs

- **[Mode switch latency]** → First switch to an uncomputed mode triggers background computation. **Mitigation:** Show "Computing layout..." in log, cached afterward.
- **[Cache memory]** → 4 copies of layout data. **Mitigation:** Layout coordinates are small (< 1MB for typical graphs).
