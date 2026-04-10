## ADDED Requirements

### Requirement: Tag-triggered release workflow
The repository SHALL provide a GitHub Actions workflow that starts package builds when a pushed tag matches the `pac.*` release pattern.

#### Scenario: Matching pac tag is pushed
- **WHEN** a Git tag named `pac.1.2.3` is pushed to GitHub
- **THEN** GitHub Actions starts the release packaging workflow

#### Scenario: Non-release tag is pushed
- **WHEN** a Git tag named `v1.2.3` is pushed to GitHub
- **THEN** the release packaging workflow is not started by that tag

### Requirement: Cross-platform package builds
The release packaging workflow SHALL build packages on GitHub-hosted Ubuntu and macOS runners using the repository's platform build scripts.

#### Scenario: Release workflow runs platform builds
- **WHEN** the release packaging workflow is triggered by a `pac.*` tag
- **THEN** it runs an Ubuntu package build on a GitHub-hosted Ubuntu runner
- **AND** it runs a macOS package build on a GitHub-hosted macOS runner

#### Scenario: Platform build scripts produce package artifacts
- **WHEN** the platform build jobs complete successfully
- **THEN** the Ubuntu job provides a `.deb` package artifact
- **AND** the macOS job provides a `.dmg` package artifact

### Requirement: Ubuntu build script
The repository SHALL provide a root-level `build_ubuntu.sh` script for configuring, building, testing, packaging, and smoke-checking the project on Ubuntu.

#### Scenario: Default Ubuntu build
- **WHEN** a developer or CI runner executes `./build_ubuntu.sh` on Ubuntu
- **THEN** the script installs or verifies required apt dependencies
- **AND** configures a release CMake build
- **AND** builds the project
- **AND** runs the test suite with headless Qt settings
- **AND** produces a `.deb` package with CPack
- **AND** verifies that the `satgraf --help` smoke check succeeds

#### Scenario: Optional Ubuntu build stages
- **WHEN** `RUN_TESTS=0` or `RUN_PACKAGE=0` is supplied to `./build_ubuntu.sh`
- **THEN** the corresponding test or package stage is skipped without preventing the build stage from running

### Requirement: Release artifact upload
The release packaging workflow SHALL upload generated packages as workflow artifacts and attach them to the GitHub Release for the triggering tag.

#### Scenario: Workflow artifacts are retained
- **WHEN** platform package builds complete successfully
- **THEN** the workflow uploads the generated `.deb` and `.dmg` files as GitHub Actions artifacts

#### Scenario: GitHub Release receives package assets
- **WHEN** all platform package artifacts are available for a `pac.*` tag workflow run
- **THEN** the workflow creates or updates the GitHub Release for that tag
- **AND** attaches the generated `.deb` and `.dmg` files to the release
