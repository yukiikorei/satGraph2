## MODIFIED Requirements

### Requirement: Node Rendering
Nodes SHALL be rendered as filled circles with a configurable diameter (default 10 pixels). Node color SHALL be determined by community assignment. Node z-value SHALL be dynamically adjustable to support community selection highlighting.

#### Scenario: Default node appearance
- **WHEN** a node is rendered with default settings
- **THEN** it appears as a filled circle of 10px diameter, colored by its community ID using a community color palette, at z-value 1.0

#### Scenario: Custom node diameter
- **WHEN** the node diameter is set to 20px
- **THEN** all nodes render at 20px diameter

#### Scenario: Community-based coloring
- **WHEN** node 5 belongs to community 2 and community 2's color is blue
- **THEN** node 5 is rendered in blue

#### Scenario: Selected community z-order promotion
- **WHEN** community 3 is selected by the user
- **THEN** all nodes in community 3 have their z-value set to 2.0, rendering them above nodes at z-value 1.0

#### Scenario: Deselected community z-order restoration
- **WHEN** community 3 is deselected (user selects "— none —")
- **THEN** all nodes in community 3 have their z-value restored to 1.0

### Requirement: Edge Rendering
Edges SHALL be rendered as lines between connected nodes. Intra-community edges SHALL be colored by community; inter-community edges SHALL be rendered in white or gray. Edge z-value SHALL be dynamically adjustable for selected community highlighting.

#### Scenario: Intra-community edge
- **WHEN** an edge connects two nodes in community 3, and community 3's color is green
- **THEN** the edge line is rendered in green

#### Scenario: Inter-community edge
- **WHEN** an edge connects node in community 1 to a node in community 4
- **THEN** the edge line is rendered in white or gray, distinct from community colors

#### Scenario: Selected community edge promotion
- **WHEN** community 3 is selected
- **THEN** intra-community edges of community 3 are promoted to z-value 1.5, rendering them above edges at z-value 0.0
