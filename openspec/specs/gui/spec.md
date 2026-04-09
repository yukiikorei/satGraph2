# GUI

Qt 6 + QML application shell providing the main window, control panels for community and evolution modes, menu bar, statistics display, and CLI headless mode.

## ADDED Requirements

### Requirement: Main Window Layout
The main window SHALL use a split pane layout with a graph canvas on the left and a control panel on the right. The split SHALL be resizable by dragging the divider.

#### Scenario: Default window layout
- **WHEN** the application starts with a CNF file loaded
- **THEN** the main window shows the graph canvas occupying roughly 70% of the width and the control panel occupying the remaining 30%

#### Scenario: Resizable split pane
- **WHEN** the user drags the divider between canvas and control panel to the left
- **THEN** the canvas shrinks and the control panel expands accordingly

#### Scenario: Window resize
- **WHEN** the user resizes the main window
- **THEN** both the canvas and control panel adjust proportionally, maintaining the split ratio

### Requirement: Menu Bar
The main window SHALL provide a menu bar with File, View, and Help menus.

#### Scenario: File menu
- **WHEN** the user opens the File menu
- **THEN** it contains Open (load CNF file), Save (save current graph), and Export (export image) actions

#### Scenario: View menu
- **WHEN** the user opens the View menu
- **THEN** it contains zoom in, zoom out, fit to view, and reset layout actions

#### Scenario: Help menu
- **WHEN** the user opens the Help menu
- **THEN** it contains an About action showing version and license information

### Requirement: Community Mode Controls
When in community mode, the right panel SHALL display: file selector, community algorithm dropdown, layout algorithm dropdown, node coloring options, edge coloring options, and community filter checkboxes.

#### Scenario: File selector
- **WHEN** the user clicks the file selector
- **THEN** a file dialog opens filtered for CNF files, and selecting a file triggers parsing and graph display

#### Scenario: Community algorithm dropdown
- **WHEN** the community algorithm dropdown is shown
- **THEN** it lists all registered community detection algorithms (at minimum: Louvain, CNM, Online), and selecting one triggers community detection

#### Scenario: Layout algorithm dropdown
- **WHEN** the layout algorithm dropdown is shown
- **THEN** it lists all registered layout algorithms (at minimum: FR, FA2, KK, Circle, Grid), and selecting one triggers relayout

#### Scenario: Node coloring options
- **WHEN** the user changes the node coloring scheme
- **THEN** all nodes are recolored immediately in the canvas

#### Scenario: Edge coloring options
- **WHEN** the user changes the edge coloring scheme (e.g., intra-community colored, inter-community white)
- **THEN** all edges are recolored immediately

#### Scenario: Community filter checkboxes
- **WHEN** community detection produces 5 communities
- **THEN** 5 checkboxes appear (one per community, labeled by community ID and size), and unchecking a community hides its nodes and edges

#### Scenario: Sortable community list
- **WHEN** the user clicks the sort control on the community filter list
- **THEN** communities are sorted by size (ascending or descending), node count, or community ID

### Requirement: Evolution Mode Controls
When in evolution mode, the right panel SHALL display all community controls plus playback controls, a conflict counter, a decision variable toggle, and temperature coloring options.

#### Scenario: Playback controls
- **WHEN** evolution mode is active
- **THEN** the panel shows play, pause, step forward, and step backward buttons, and a timeline slider spanning the conflict range

#### Scenario: Play/pause
- **WHEN** the user presses play
- **THEN** the engine begins processing events forward at a configurable speed, updating the graph display in real time

#### Scenario: Step forward
- **WHEN** the user presses step forward
- **THEN** the engine advances by one event (or one conflict, depending on granularity) and the display updates

#### Scenario: Step backward
- **WHEN** the user presses step backward
- **THEN** the engine reverts one event and the display updates to show the previous state

#### Scenario: Timeline slider
- **WHEN** the user drags the timeline slider to conflict 50 out of 200
- **THEN** the engine jumps to conflict 50 and the display reflects that solver state

#### Scenario: Conflict counter display
- **WHEN** the engine is at conflict 47
- **THEN** the panel displays "Conflict: 47 / 200" (or the total if known)

#### Scenario: Decision variable toggle
- **WHEN** the user enables the decision variable toggle
- **THEN** the current decision variable node is highlighted with a distinct visual indicator in the topmost rendering layer

#### Scenario: Temperature coloring
- **WHEN** temperature coloring is enabled
- **THEN** node colors shift based on activity level (e.g., cool blue for low activity, hot red for high activity), overriding community coloring

### Requirement: Implication Mode Controls
When in implication mode, the right panel SHALL display clause panels and support node interaction for setting values and watching implication propagation.

#### Scenario: Clause display panel
- **WHEN** implication mode is active and the user clicks on a clause
- **THEN** a panel displays all literals in the clause with their current assignment states (true, false, unassigned)

#### Scenario: Node value setting
- **WHEN** the user clicks on a node in the graph
- **THEN** a context menu or panel allows setting the node's value to true or false, and the UI propagates visible implications

#### Scenario: Implication propagation visualization
- **WHEN** a node value is set and implications propagate through clauses
- **THEN** affected nodes animate or highlight to show the chain of implications

### Requirement: Info Panel
The GUI SHALL display an info panel with graph statistics: node count, edge count, clause count, community statistics (when available), internal edge count (intra-community), and external edge count (inter-community). Community selection SHALL display internal nodes, bridge nodes, internal edges, external edges, and linked communities. Node selection SHALL display the node's community, internal neighbors, external neighbors, and linked communities. A text input SHALL allow node selection by ID or name.

#### Scenario: Basic statistics display
- **WHEN** a graph with 500 nodes, 1200 edges, and 400 clauses is loaded
- **THEN** the info panel shows "Nodes: 500", "Edges: 1200", "Clauses: 400"

#### Scenario: Community statistics
- **WHEN** community detection produces 4 communities with Q = 0.42, 800 intra-community edges and 400 inter-community edges
- **THEN** the info panel shows "Communities: 4", "Modularity: 0.420", "Internal Edges: 800", "External Edges: 400"

#### Scenario: Community detail on selection
- **WHEN** the user selects community 2 which has 120 nodes (15 of which are bridge nodes), 200 internal edges, 80 external edges connecting to 3 other communities
- **THEN** the community detail section shows: "Internal Nodes: 120", "Bridge Nodes: 15", "Internal Edges: 200", "External Edges: 80", "Linked Communities: 3"

#### Scenario: Node detail on selection
- **WHEN** the user selects node 42 in community 3 which has 5 neighbors in community 3, 2 neighbors in other communities spanning 2 distinct communities
- **THEN** the node detail section shows: "Node 42", "Community: 3", "Internal Neighbors: 5", "External Neighbors: 2", "Linked Communities: 2"

#### Scenario: Node text search
- **WHEN** the user types "42" in the node search field and presses Enter
- **THEN** node 42 is selected and highlighted, and the node detail section updates with enriched information

### Requirement: Modern QML Controls
The control panel SHALL use Qt 6 QML-styled controls with a clean, modern appearance consistent across all platforms.

#### Scenario: Consistent styling
- **WHEN** the application runs on different platforms (Linux, Windows, macOS)
- **THEN** the control panel has a consistent visual style defined by QML styling

#### Scenario: Responsive control layout
- **WHEN** the control panel is resized narrower
- **THEN** controls reflow vertically to remain usable without horizontal scrolling

### Requirement: CLI Headless Mode
The application SHALL support a CLI mode that runs without opening a GUI window, suitable for headless export and batch processing.

#### Scenario: Headless image export
- **WHEN** the application is launched with `--headless --export output.png --input instance.cnf`
- **THEN** the graph is parsed, laid out, rendered to an offscreen image, and saved to output.png without ever opening a window

#### Scenario: CLI help
- **WHEN** the application is launched with `--help`
- **THEN** a usage message listing all CLI options is printed to stdout and the application exits
