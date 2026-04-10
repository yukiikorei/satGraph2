## Context

The project already uses CMake and CPack to produce platform packages: macOS uses the `DragNDrop` generator for `.dmg`, and non-Apple builds use the `DEB` generator for `.deb`. A root-level `build_macos.sh` script installs missing Homebrew dependencies, configures a release build, runs tests, packages with CPack, validates the generated DMG, and smoke-checks the application binary.

There is currently no `.github/workflows` automation and no equivalent root-level Ubuntu build script. The release workflow should therefore add the missing Ubuntu script while reusing the existing macOS script instead of duplicating macOS build logic in YAML.

## Goals / Non-Goals

**Goals:**
- Trigger release packaging when a pushed Git tag matches `pac.*`.
- Build packages on GitHub-hosted Ubuntu and macOS runners.
- Keep local and CI Ubuntu packaging behavior in a reusable `build_ubuntu.sh` script.
- Upload `.deb` and `.dmg` package outputs as workflow artifacts and GitHub Release assets.
- Preserve the existing local macOS build workflow.

**Non-Goals:**
- Changing CPack package metadata, package naming, or application versioning.
- Adding Windows packaging.
- Publishing packages to apt repositories, Homebrew taps, package registries, or external storage.
- Replacing the existing `build_macos.sh` script.

## Decisions

1. Use `pac.*` as the GitHub Actions tag glob.

   The user described the release tag pattern as `pac.xxx`. GitHub Actions tag filters use glob syntax, so `pac.*` represents that family of tags while avoiding accidental triggers for unrelated tags. Alternative considered: trigger on all tags and check the name inside a shell step. The workflow-level tag filter is simpler and avoids creating unnecessary runs.

2. Add `build_ubuntu.sh` as the authoritative Ubuntu build entrypoint.

   The script should mirror the existing macOS toggles where useful: `BUILD_DIR`, `BUILD_TYPE`, `RUN_TESTS`, `RUN_PACKAGE`, and `BUILD_JOBS`. It should install missing apt dependencies when running on Ubuntu, configure and build with CMake, run tests with `QT_QPA_PLATFORM=offscreen`, package with CPack, verify that a `.deb` exists, and smoke-check `satgraf --help`. Alternative considered: put the full Ubuntu flow directly in the workflow. A script is better because it is reusable locally and easier to test outside GitHub Actions.

3. Keep macOS packaging delegated to `build_macos.sh`.

   The workflow should install or rely on Homebrew dependencies via the existing script, then collect the generated `.dmg`. Alternative considered: create a parallel macOS YAML implementation. Reusing the script keeps macOS behavior in one place and avoids divergence.

4. Use separate platform jobs plus a release upload job.

   Each platform job uploads its package with `actions/upload-artifact`, and a final release job downloads those artifacts and attaches them to the GitHub Release. This makes build failures platform-specific and avoids coupling release upload to a single runner. Alternative considered: upload release assets directly from each platform job. A final upload job is easier to reason about and centralizes the required `contents: write` permission.

5. Use `softprops/action-gh-release` for GitHub Release asset upload.

   This action handles creating or updating the release for the pushed tag and uploading multiple files. Alternative considered: use the GitHub CLI. The action keeps the workflow compact and avoids relying on a preinstalled `gh` behavior.

## Risks / Trade-offs

- Ubuntu package dependency names can differ across runner versions -> Pin the workflow to an Ubuntu runner version that matches the documented dependency set, or keep the apt package list explicit and update it when the runner image changes.
- GUI tests and smoke checks can fail without display support -> Run tests with `QT_QPA_PLATFORM=offscreen` and install Qt/OpenGL/OpenCL runtime packages needed by the executable.
- CPack currently derives the package version from `project(satgraf VERSION 0.1.0)` rather than the tag -> Treat version synchronization as out of scope for this change and document the current behavior.
- Release upload depends on third-party actions -> Pin major versions and keep permissions limited to `contents: write` only for the release upload job.
