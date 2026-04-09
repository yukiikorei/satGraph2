## ADDED Requirements

### Requirement: Node Text Input Field
The right panel SHALL provide a text input field that allows users to type a node ID or node name to select a node, as an alternative to clicking nodes in the graph canvas.

#### Scenario: Select node by numeric ID
- **WHEN** the user types "42" into the node search field and presses Enter
- **THEN** the node with ID 42 is selected, the node detail section updates with enriched information, and the node is highlighted in the canvas (decision variable highlight)

#### Scenario: Select node by name
- **WHEN** the user types "v127" into the node search field and presses Enter
- **THEN** the node whose name matches "v127" is selected, the node detail section updates, and the node is highlighted in the canvas

#### Scenario: Partial name matching
- **WHEN** the user types "12" into the node search field
- **THEN** the input provides autocomplete suggestions or feedback for nodes whose ID or name contains "12"

#### Scenario: Node not found
- **WHEN** the user types an ID or name that does not match any node
- **THEN** the log area displays a message like "Node not found: <input>" and no selection change occurs

#### Scenario: Clear input resets selection
- **WHEN** the user clears the text input field
- **THEN** the node highlight is removed and the node detail section resets to "Click a node" or similar placeholder

#### Scenario: Text input coexists with click selection
- **WHEN** the user first selects a node via text input, then clicks a different node in the canvas
- **THEN** the text input field updates to reflect the clicked node's identifier, and the detail section shows the clicked node's information

### Requirement: Node Search is Always Available
The node text input SHALL be visible and functional whenever a graph is loaded, regardless of whether community detection has been run.

#### Scenario: Search before community detection
- **WHEN** a graph is loaded but not yet rendered with communities
- **THEN** the user can type a node ID/name to select it, with the detail section showing basic node info (ID, name) and community fields showing "—"

#### Scenario: Search disabled with no graph
- **WHEN** no graph is loaded
- **THEN** the node search field is disabled or shows a placeholder indicating a file must be loaded first
