# Export

Image export capabilities for rendering graphs to static images (JPEG, PNG) and animated GIFs (for evolution mode), supporting both headless offscreen rendering and interactive export from the GUI.

## ADDED Requirements

### Requirement: Static Image Export
The system SHALL render the current graph view to a QImage and save it as JPEG or PNG with configurable quality.

#### Scenario: PNG export from GUI
- **WHEN** the user selects Export from the File menu and chooses PNG format with quality 95
- **THEN** the current graph view is rendered to a QImage and saved as a PNG file with the specified quality

#### Scenario: JPEG export with quality setting
- **WHEN** the user exports as JPEG with quality 80
- **THEN** the image is saved as a JPEG file with quality parameter 80 (out of 100)

#### Scenario: Export includes all visible layers
- **WHEN** the user exports while the graph is displayed with nodes, edges, and highlights
- **THEN** the exported image contains all currently visible layers (edges, nodes, highlights) exactly as shown on screen

### Requirement: Animated GIF Export
In evolution mode, the system SHALL render each evolution step as a frame and write the sequence as an animated GIF.

#### Scenario: Animated GIF of evolution
- **WHEN** the user triggers animated export for an evolution with 200 conflicts
- **THEN** each conflict state is rendered as a frame, and the sequence is written as an animated GIF file

#### Scenario: Frame rate control
- **WHEN** the user configures GIF export at 10 frames per second
- **THEN** the animated GIF plays at 10 fps with appropriate inter-frame delay (100ms per frame)

#### Scenario: GIF sequence writer
- **WHEN** frames are being written to the GIF
- **THEN** a GIF sequence writer appends frames incrementally without holding all frames in memory simultaneously

### Requirement: Headless Offscreen Export
The system SHALL support rendering to an offscreen QImage without creating a visible window, defaulting to 1024x1024 resolution.

#### Scenario: Default headless export
- **WHEN** headless export is triggered without specifying dimensions
- **THEN** the graph is rendered to a 1024x1024 offscreen QImage and saved to the specified file

#### Scenario: Custom resolution headless export
- **WHEN** headless export is triggered with dimensions 2048x2048
- **THEN** the graph is rendered to a 2048x2048 offscreen QImage and saved to the specified file

#### Scenario: Headless export does not create window
- **WHEN** headless export runs on a machine without a display server
- **THEN** the export completes successfully using Qt's offscreen platform plugin, and no window is created

### Requirement: Interactive Export from GUI
The system SHALL allow exporting the current view from the GUI, capturing exactly what the user sees.

#### Scenario: Export current zoom level
- **WHEN** the user has zoomed into a region of the graph and selects Export Current View
- **THEN** the exported image shows only the visible region at the current zoom level

#### Scenario: Export entire graph
- **WHEN** the user selects Export Entire Graph
- **THEN** the exported image shows the full graph regardless of the current viewport, fitted to the output dimensions

### Requirement: Export Progress Reporting
The system SHALL report progress during export operations, especially for animated GIF export which may take significant time.

#### Scenario: Static export progress
- **WHEN** a static export of a large graph begins
- **THEN** progress is reported as rendering completes (0.0 when starting, 1.0 when saved to disk)

#### Scenario: Animated GIF export progress
- **WHEN** an animated GIF export of 200 frames is underway
- **THEN** progress is reported as the fraction of frames completed (e.g., 0.5 after 100 frames)

#### Scenario: Progress display in GUI
- **WHEN** an export is in progress
- **THEN** a progress bar or status indicator is shown in the GUI, and the user is notified on completion
