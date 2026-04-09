## MODIFIED Requirements

### Requirement: Lazy Per-Mode Layout Computation
The render pipeline SHALL compute only the layout needed for the currently selected render mode. Layouts for other modes SHALL NOT be computed until the user switches to those modes. Computed layouts SHALL be cached per mode so switching back is instant.

#### Scenario: Detailed2D render only computes 2D layout
- **WHEN** the user is in Detailed2D mode and clicks Render
- **THEN** only the selected 2D layout algorithm runs; no FA3D or community layouts are computed

#### Scenario: Switch to uncomputed mode triggers layout
- **WHEN** the user renders in Detailed2D then switches to Mode3D
- **THEN** the FA3D layout computation starts in the background for the 3D mode

#### Scenario: Switch back to cached mode is instant
- **WHEN** the user has already viewed Mode3D and switches back to Detailed2D
- **THEN** the 2D view displays immediately using cached layout data

### Requirement: Mode-Specific Layout Dropdown
The layout algorithm dropdown SHALL show only algorithms valid for the current render mode. When the render mode changes, the dropdown SHALL repopulate. If the previously selected algorithm is invalid for the new mode, the first valid algorithm SHALL be auto-selected.

#### Scenario: Detailed2D shows all node-level algorithms
- **WHEN** render mode is Detailed2D
- **THEN** the layout dropdown shows: f, fgpu, forceAtlas2, kk, c, grid, gkk

#### Scenario: Simple2D shows community algorithms
- **WHEN** render mode is Simple2D
- **THEN** the layout dropdown shows only community-weighted layout algorithms

#### Scenario: Mode3D shows 3D algorithms
- **WHEN** render mode is Mode3D
- **THEN** the layout dropdown shows: fa3d and any other 3D algorithms

#### Scenario: Simple3D shows community 3D algorithms
- **WHEN** render mode is Simple3D
- **THEN** the layout dropdown shows only community-weighted 3D layout algorithms

### Requirement: New Render Clears Cache
When the user clicks Render, all cached per-mode layouts SHALL be cleared and only the current mode's layout recomputed.

#### Scenario: Re-render after parameter change
- **WHEN** the user changes the iteration slider and clicks Render in Detailed2D mode
- **THEN** all cached layouts are cleared, Detailed2D layout is recomputed, and other modes will need recomputation when visited
