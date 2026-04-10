## ADDED Requirements

### Requirement: macOS 26 Visual Integration
When running on macOS, the GUI SHALL apply macOS-specific visual integration that follows macOS 26-style conventions through translucent Liquid Glass-inspired surfaces, platform-native spacing, native menu/title behavior where practical, and readable controls, while preserving existing behavior on non-macOS platforms.

#### Scenario: macOS visual style is applied
- **WHEN** the GUI starts on macOS
- **THEN** the main window, control panel, menus, and primary controls use the macOS-specific style branch and present translucent, readable, platform-integrated surfaces.

#### Scenario: Non-macOS style is preserved
- **WHEN** the GUI starts on Linux or another non-Apple platform
- **THEN** the existing cross-platform visual styling and window behavior remain active without requiring macOS-only APIs or assets.

#### Scenario: Linux GUI remains buildable and runnable
- **WHEN** the project is configured, built, and launched on Linux
- **THEN** the existing Qt GUI builds and runs without requiring macOS frameworks, macOS icons, or macOS-specific window APIs.

#### Scenario: Controls remain readable over graph content
- **WHEN** a graph is loaded and the control panel overlays or sits near high-contrast graph content
- **THEN** labels, buttons, selectors, sliders, and checkboxes remain readable and usable under the macOS visual treatment.

#### Scenario: Native macOS window behavior
- **WHEN** the GUI starts on macOS
- **THEN** window chrome, traffic-light controls, menu behavior, and application identity use native Qt/macOS integration where practical instead of Linux-oriented custom chrome.
