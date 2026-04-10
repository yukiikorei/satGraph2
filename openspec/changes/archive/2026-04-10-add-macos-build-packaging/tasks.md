## 1. macOS Bundle Configuration

- [x] 1.1 Update `src/app/CMakeLists.txt` so the existing `satgraf` target becomes a `MACOSX_BUNDLE` target only on Apple platforms.
- [x] 1.2 Add macOS bundle metadata including bundle identifier, display name, executable name, short version, bundle version, and copyright/license fields.
- [x] 1.3 Add a maintained `.icns` bundle icon asset generated from the existing project icon source.
- [x] 1.4 Wire the `.icns` asset into the `satgraf` target with `MACOSX_BUNDLE_ICON_FILE` and resource source properties.

## 2. macOS Packaging and Dependency Deployment

- [x] 2.1 Refactor root CPack configuration so Debian settings remain active on non-Apple platforms and macOS settings are active only inside `if(APPLE)`.
- [x] 2.2 Add macOS install rules that install `satgraf.app` into the package root without installing Linux `.desktop` metadata.
- [x] 2.3 Add Qt runtime deployment for macOS using Qt CMake deployment helpers when available and `macdeployqt` as the fallback.
- [x] 2.4 Configure macOS CPack to produce a DragNDrop package artifact containing `satgraf.app`.

## 3. macOS GUI Visual Integration

- [x] 3.1 Add macOS-only application attributes and style setup in `src/app/main.cpp` while preserving existing non-macOS startup behavior.
- [x] 3.2 Update `MainWindow` initialization to prefer native macOS window/menu behavior on Apple platforms instead of Linux-oriented custom chrome where practical.
- [x] 3.3 Add macOS-specific styling for main window surfaces, the right control panel, separators, buttons, combo boxes, sliders, checkboxes, and status/log text.
- [x] 3.4 Guard all macOS-only GUI code with `Q_OS_MACOS` or equivalent Qt platform checks so Linux builds do not require macOS APIs or assets.
- [ ] 3.5 Verify the macOS visual treatment keeps labels and controls readable over loaded graph content and does not alter non-macOS styling branches.

## 4. Packaging Verification Workflow

- [x] 4.1 Document or script the local macOS configure command for a clean build directory.
- [x] 4.2 Document or script the local macOS build command for the `satgraf` target.
- [x] 4.3 Document or script the local macOS package command using CPack.
- [x] 4.4 Add package inspection checks that confirm the macOS package artifact exists and contains `satgraf.app`.
- [x] 4.5 Add a smoke check that launches the packaged/deployed app or runs a noninteractive bundle executable check in the current macOS environment.

## 5. Regression Checks

- [x] 5.1 Configure and build the project on the current macOS environment.
- [x] 5.2 Run the existing automated test suite after the packaging changes.
- [x] 5.3 Run CPack on the current macOS environment and confirm the package artifact is produced.
- [x] 5.4 Smoke-test the packaged app from the generated package or staging directory.
- [x] 5.5 Configure and build the project on Linux, or document the exact Linux-compatible configure/build check when Linux is not available in the current environment.
- [x] 5.6 Review CMake configuration on non-Apple paths to confirm Debian packaging metadata remains guarded and unchanged in behavior.
- [x] 5.7 Smoke-test the Linux GUI startup path, or document why it could not be run in the current environment.

## Notes

- `./build_macos.sh` has been verified on the current macOS environment for configure/build/test/package/smoke-check.
- DMG output path is `${PROJECT_ROOT}/satgraf-0.1.0-macos.dmg`.
- macOS Qt deploy (`macdeployqt`) is now optional via `SATGRAF_MACOS_RUN_MACDEPLOYQT` and defaults to `OFF` due Homebrew Qt plugin dependency/signing issues in this environment.
