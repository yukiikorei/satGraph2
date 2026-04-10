## ADDED Requirements

### Requirement: macOS App Bundle
The build system SHALL produce a runnable `satgraf.app` bundle on macOS with bundle metadata, application icon, and the existing SatGraf executable and resources.

#### Scenario: Configure macOS bundle target
- **WHEN** the project is configured on macOS
- **THEN** the `satgraf` application target is configured as a macOS app bundle with a bundle identifier, display name, version metadata, and icon resource.

#### Scenario: Run app bundle from build output
- **WHEN** the macOS build completes
- **THEN** `satgraf.app` exists in the build output and can be launched as an application bundle in the current environment.

### Requirement: macOS Runtime Deployment
The packaging workflow SHALL deploy required Qt runtime frameworks and plugins into `satgraf.app` so the packaged app does not depend on the build tree for Qt runtime files.

#### Scenario: Deploy Qt runtime dependencies
- **WHEN** the macOS install or package step runs
- **THEN** Qt frameworks and required plugins used by SatGraf are copied into the app bundle using Qt-supported deployment tooling.

#### Scenario: Smoke launch deployed bundle
- **WHEN** the deployed `satgraf.app` is launched from the package staging area
- **THEN** the application starts without missing Qt framework or plugin errors.

### Requirement: macOS Package Artifact
The build system SHALL create a distributable macOS package artifact from the app bundle while preserving the existing Debian packaging configuration for non-macOS builds.

#### Scenario: Build macOS package
- **WHEN** `cpack` runs from a macOS build directory
- **THEN** it produces a macOS package artifact containing `satgraf.app`.

#### Scenario: Preserve Linux package behavior
- **WHEN** the project is configured on a non-Apple platform
- **THEN** the existing Debian CPack metadata and install rules remain available and are not replaced by macOS package settings.

### Requirement: Local macOS Packaging Verification
The project SHALL provide a repeatable local verification path that configures, builds, packages, inspects, and smoke-tests the macOS package in the current environment.

#### Scenario: Run packaging verification
- **WHEN** the documented macOS packaging verification commands are run
- **THEN** they configure the project, build the `satgraf` target, run CPack, confirm the package artifact exists, confirm `satgraf.app` exists, and launch or execute a noninteractive smoke check.

#### Scenario: Report package failures
- **WHEN** packaging or smoke launch fails
- **THEN** the verification path reports the failing command and leaves enough build/package output to diagnose missing dependencies or bundle metadata.
