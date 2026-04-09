## ADDED Requirements

### Requirement: Managed Render Thread
The render pipeline SHALL use a managed background thread (stored as a member variable) instead of a detached thread. Before starting a new render, the system SHALL join any existing render thread to ensure the old computation has finished.

#### Scenario: Rapid re-render joins old thread
- **WHEN** a render is in progress and the user clicks Render again
- **THEN** the old render thread is joined before the new one starts, and the old thread's results are discarded

#### Scenario: Pause then render
- **WHEN** the user clicks Pause then immediately clicks Render with a different algorithm
- **THEN** the old render thread completes or is cancelled and joined before the new render starts, with no concurrent access to graph data

### Requirement: Generation Counter for Stale Result Detection
The render pipeline SHALL maintain a monotonically increasing generation counter (`render_generation_`). Each render increments the counter and captures the current value. GUI-thread callbacks SHALL discard results whose generation does not match the current counter.

#### Scenario: Stale results discarded
- **WHEN** render A starts (generation 1), then render B starts (generation 2), and render A's callback arrives after render B's
- **THEN** render A's results are discarded because generation 1 ≠ 2

### Requirement: Solver Stop Before Render
When evolution mode is active and the user clicks Render, the system SHALL stop the solver and wait for the engine to quiesce before starting the render thread.

#### Scenario: Render during evolution
- **WHEN** the solver is running and the user clicks Render
- **THEN** the solver is stopped, the evolution engine is quiesced, and the render proceeds with a stable graph

### Requirement: Post-Layout Progress Logging
All post-layout computation stages SHALL report progress via the existing log mechanism. Progress messages SHALL be emitted from the background thread using `QMetaObject::invokeMethod` with `Qt::QueuedConnection`.

#### Scenario: Progress messages during render
- **WHEN** a render is in progress
- **THEN** the log displays messages like "Detecting communities...", "Layout: 50%", "Computing bridge nodes...", "Computing quotient layout...", "Computing 3D layout...", "Render complete"
