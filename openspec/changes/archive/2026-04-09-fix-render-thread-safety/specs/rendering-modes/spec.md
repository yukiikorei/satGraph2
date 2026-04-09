## MODIFIED Requirements

### Requirement: Rendering Mode Selector
Post-layout computations (quotient graph construction, community FA2 centers, FA3D layouts) SHALL execute in the background render thread, not on the GUI thread. The `perform_render()` method SHALL only use pre-computed data to set up the view.

#### Scenario: Switching to 3D mode after render
- **WHEN** the user switches from Detailed 2D to 3D mode after a render completes
- **THEN** the 3D view displays immediately using pre-computed `coords_3d_` data, with no recomputation delay
