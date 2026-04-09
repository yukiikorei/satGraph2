## Why

Every Render click computes all 4 layout variants (2D node-level, FA2 community centers, FA3D full graph, FA3D quotient graph) regardless of which mode the user views. A user in Detailed2D waits for two FA3D computations they don't see. Additionally, Simple modes reuse node-level layout algorithms on the quotient graph, ignoring that communities-as-nodes have meaningful edge weights (inter-community edge counts) that should drive the layout.

## What Changes

- **Lazy per-mode layout** — Render only computes the layout for the currently selected mode; switching modes triggers computation only if not yet cached
- **Mode-specific layout dropdown** — layout algorithm dropdown filters to show only valid algorithms for the current render mode
- **New CommunityWeightedLayout** — a force-directed layout designed for quotient graphs where: each node = community, node mass = community size, attraction scaled by inter-community edge weight, repulsion scaled by community size
- **New CommunityWeighted3DLayout** — 3D variant of the same community-weighted algorithm for Simple3D mode

## Capabilities

### New Capabilities
- `community-layout`: Force-directed layout algorithm optimized for quotient/community graphs with weighted edges and community-size-based node mass

### Modified Capabilities
- `rendering-modes`: Layout computation becomes lazy (per-mode, cached); layout dropdown filters by mode; Simple modes use community-weighted layout
- `layout-engine`: New community-weighted layout algorithms registered in LayoutFactory with mode tags

## Impact

- **`src/core/include/satgraf/layout.hpp`** — new `CommunityWeightedLayout` and `CommunityWeighted3DLayout` classes, LayoutFactory gains mode-tag awareness
- **`src/gui/include/satgraf_gui/main_window.hpp`** — render mode/layout dropdown coupling, lazy computation, cached per-mode results
- **No other files affected**
