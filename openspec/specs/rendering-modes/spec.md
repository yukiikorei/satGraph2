# Rendering Modes

## ADDED Requirements

### Requirement: Rendering Mode Selector
The GUI SHALL provide a rendering mode dropdown in the left panel with three options: "2D" (default), "Simple 2D", and "3D". The selected mode determines how the graph is visually rendered after community detection.

#### Scenario: Default rendering mode
- **WHEN** the application starts
- **THEN** the rendering mode dropdown shows "2D" selected and the graph renders with individual nodes and edges (current behavior)

#### Scenario: Switch to Simple 2D
- **WHEN** the user selects "Simple 2D" from the rendering mode dropdown after community detection
- **THEN** the canvas re-renders showing one circle per community, sized proportionally to node count, with edges between communities weighted by inter-community edge count

#### Scenario: Switch to 3D
- **WHEN** the user selects "3D" from the rendering mode dropdown after community detection
- **THEN** the canvas switches to a 3D view showing community spheres sized by node count with weighted inter-community edges, rendered with perspective projection

#### Scenario: Mode switch before rendering
- **WHEN** the user selects a rendering mode before clicking "Render"
- **THEN** the mode is stored and applied when rendering completes

### Requirement: Simple 2D Rendering
In Simple 2D mode, each community SHALL be rendered as a single filled circle. Circle diameter SHALL be proportional to the number of nodes in the community. Edges between community circles SHALL have width proportional to the number of inter-community edges. Edge weight SHALL be displayed as a label on the edge.

#### Scenario: Community circle sizing
- **WHEN** community A has 50 nodes and community B has 10 nodes
- **THEN** community A's circle is approximately 5x the diameter of community B's circle (proportional to sqrt of node count for area-proportional sizing)

#### Scenario: Edge width proportional to weight
- **WHEN** there are 20 inter-community edges between community A and community B, and 5 between A and C
- **THEN** the edge line between A and B is 4x wider than between A and C

#### Scenario: Community labels visible
- **WHEN** Simple 2D mode is active
- **THEN** each community circle displays its community ID and node count as a label

#### Scenario: Click interaction in Simple 2D
- **WHEN** the user clicks on a community circle
- **THEN** the community dropdown updates to that community and the enriched community detail is displayed

### Requirement: 3D Rendering with Rotation
In 3D mode, communities SHALL be rendered as 3D spheres sized by node count, with weighted inter-community edges as 3D lines. The user SHALL be able to rotate the view by dragging the mouse and zoom with the scroll wheel.

#### Scenario: Mouse drag rotation
- **WHEN** the user clicks and drags the mouse in the 3D view
- **THEN** the camera rotates around the center of the graph (arcball rotation), allowing the user to view from any angle

#### Scenario: Scroll wheel zoom in 3D
- **WHEN** the user scrolls the mouse wheel in 3D mode
- **THEN** the camera zooms in or out along its current view direction

#### Scenario: 3D community sphere sizing
- **WHEN** community A has 100 nodes and community B has 25 nodes
- **THEN** community A's sphere has approximately 2x the radius of community B's sphere (proportional to cube root for volume-proportional sizing)

#### Scenario: Perspective depth cues
- **WHEN** 3D mode is active and communities are at different depths
- **THEN** farther communities appear smaller (perspective projection) providing natural depth perception

#### Scenario: Edge labels in 3D
- **WHEN** 3D mode is active
- **THEN** inter-community edges are rendered as lines between sphere centers with weight indicated by line thickness

### Requirement: Rendering Mode Interaction with Evolution
When evolution mode is active, the rendering mode SHALL be forced to "2D" because evolution requires individual node highlighting.

#### Scenario: Evolution forces 2D
- **WHEN** the user starts the solver in evolution mode while Simple 2D or 3D is selected
- **THEN** the rendering mode switches to "2D" and Simple 2D/3D options are disabled until the solver stops

### Requirement: Background Post-Layout Computation
Post-layout computations (quotient graph construction, community FA2 centers, FA3D layouts) SHALL execute in the background render thread, not on the GUI thread. The `perform_render()` method SHALL only use pre-computed data to set up the view.

#### Scenario: Switching to 3D mode after render
- **WHEN** the user switches from Detailed 2D to 3D mode after a render completes
- **THEN** the 3D view displays immediately using pre-computed `coords_3d_` data, with no recomputation delay
