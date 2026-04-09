# Community Z-Order

## ADDED Requirements

### Requirement: Community Z-Order Promotion
When a community is selected from the community dropdown, all nodes belonging to that community SHALL be rendered at a higher z-value than unselected community nodes, visually bringing them to the foreground.

#### Scenario: Selecting a community promotes its nodes
- **WHEN** the user selects "Community 5" from the community dropdown
- **THEN** all nodes belonging to community 5 are set to z-value 2.0, while all other nodes remain at z-value 1.0, making community 5 nodes render on top

#### Scenario: Deselecting community restores default z-order
- **WHEN** the user selects "— none —" from the community dropdown
- **THEN** all nodes return to their default z-value of 1.0, restoring the original rendering order

#### Scenario: Community selection also highlights edges
- **WHEN** community 5 is selected
- **THEN** intra-community edges of community 5 are set to a higher z-value (1.5) so they render above other edges, while inter-community edges remain at z-value 0.0

#### Scenario: Selection persists across zoom and pan
- **WHEN** community 5 is selected and the user zooms or pans the view
- **THEN** community 5 nodes remain at the higher z-value and continue to render on top of other nodes
