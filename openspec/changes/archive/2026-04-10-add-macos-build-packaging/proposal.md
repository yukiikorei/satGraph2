## Why

The project currently has Debian-oriented CPack metadata but no macOS-specific build, bundle, or package path. Running and distributing SatGraf on macOS should be reproducible from the current development environment, and the macOS build should visually integrate with macOS 26 conventions instead of presenting as a generic cross-platform desktop window.

## What Changes

- Add a macOS build and packaging path that produces a runnable `.app` bundle and a distributable package artifact from the current environment.
- Add macOS install metadata for bundle identity, icon resources, runtime dependency deployment, and CPack generator selection without breaking the existing Debian package flow.
- Add a local packaging verification workflow that configures, builds, packages, and smoke-tests the resulting macOS app/package.
- Update the macOS GUI presentation to adopt macOS 26-style visual treatment: Liquid Glass-inspired translucent chrome, platform-native spacing, macOS control behavior where Qt supports it, and a fallback that preserves the existing layout on non-macOS platforms.
- Preserve the existing Linux build, runtime behavior, Qt interface, desktop metadata, and Debian package path; macOS-specific behavior must be guarded by platform checks.

## Capabilities

### New Capabilities
- `macos-packaging`: macOS build, bundle, dependency deployment, package generation, and local package smoke-test workflow.

### Modified Capabilities
- `gui`: macOS-specific visual integration and styling requirements for a macOS 26-style presentation while preserving existing cross-platform behavior.

## Impact

- **`CMakeLists.txt`** — platform-specific CPack configuration, macOS bundle install rules, icon handling, and dependency deployment hooks.
- **`src/app/CMakeLists.txt`** — macOS bundle properties and resource wiring for the `satgraf` application target.
- **`src/app/main.cpp`** — macOS application attributes and platform style setup where needed.
- **`src/gui/include/satgraf_gui/main_window.hpp`** — macOS-specific style application for the main window, toolbars, panels, and controls.
- **`icon.icns` or generated macOS icon asset** — bundle icon source for the `.app` package.
- **Documentation or packaging scripts** — reproducible local commands for configure/build/package/smoke-test on macOS, plus Linux build/run regression notes.
