## Why

Release packaging is currently documented for local use, but there is no repository workflow that turns a tagged version into downloadable macOS and Ubuntu packages. Automating this on `pac.xxx` tags makes release creation repeatable and keeps platform packages attached to the matching source revision.

## What Changes

- Add a GitHub Actions workflow that triggers when a tag matching `pac.*` is pushed.
- Build and package on GitHub-hosted macOS and Ubuntu runners.
- Add a `build_ubuntu.sh` script that mirrors the existing `build_macos.sh` controls for dependency setup, build, test, package, and smoke checks.
- Upload the generated `.dmg` and `.deb` packages as workflow artifacts and attach them to the tag's GitHub Release.
- Document the expected release tag pattern and local Ubuntu build script usage.

## Capabilities

### New Capabilities
- `release-packaging`: Automated release packaging for `pac.*` tags across macOS and Ubuntu.

### Modified Capabilities

## Impact

- Adds `.github/workflows/` release automation.
- Adds a new root-level `build_ubuntu.sh` script.
- Updates release/build documentation in `README.md`.
- Uses existing CMake/CPack packaging metadata and the existing `build_macos.sh` flow.
