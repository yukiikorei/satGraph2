## MODIFIED Requirements

### Requirement: Info Panel
The render button SHALL use a managed thread instead of detached thread. The `on_render_complete` callback SHALL only perform lightweight GUI updates (setting labels, triggering `perform_render()`) since all heavy computation runs in the background thread.

#### Scenario: Render button starts managed thread
- **WHEN** the user clicks Render
- **THEN** any existing render thread is joined, a new managed thread is started, and controls are disabled until completion

#### Scenario: GUI responsive during render
- **WHEN** a render is in progress
- **THEN** the GUI remains responsive (window can be moved, resized) and the log shows progress updates
