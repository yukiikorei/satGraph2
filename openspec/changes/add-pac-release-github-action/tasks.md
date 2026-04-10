## 1. Ubuntu Build Script

- [x] 1.1 Add root-level `build_ubuntu.sh` with `set -euo pipefail`, repository-root detection, logging helpers, and environment toggles for `BUILD_DIR`, `BUILD_TYPE`, `RUN_TESTS`, `RUN_PACKAGE`, and `BUILD_JOBS`.
- [x] 1.2 Add Ubuntu dependency setup for CMake, compiler, Qt 6, igraph, Eigen, nlohmann_json, CLI11, Catch2, OpenCL headers/runtime, and packaging tools.
- [x] 1.3 Configure and build the project with CMake using the selected build directory, build type, and job count.
- [x] 1.4 Run tests with `QT_QPA_PLATFORM=offscreen` when `RUN_TESTS=1`.
- [x] 1.5 Run CPack when `RUN_PACKAGE=1`, verify that a `.deb` package was produced, and smoke-check the built `satgraf --help` command.

## 2. GitHub Actions Workflow

- [x] 2.1 Add `.github/workflows/release.yml` with an `on.push.tags` filter matching `pac.*`.
- [x] 2.2 Add an Ubuntu package job on a GitHub-hosted Ubuntu runner that runs `./build_ubuntu.sh` and uploads the generated `.deb` as an artifact.
- [x] 2.3 Add a macOS package job on a GitHub-hosted macOS runner that runs `./build_macos.sh` and uploads the generated `.dmg` as an artifact.
- [x] 2.4 Add a release upload job that waits for both platform jobs, downloads artifacts, grants only the required `contents: write` permission, and attaches the `.deb` and `.dmg` files to the GitHub Release for the triggering tag.

## 3. Documentation

- [x] 3.1 Update `README.md` with local Ubuntu packaging instructions for `./build_ubuntu.sh` and its environment toggles.
- [x] 3.2 Update `README.md` with the release tag pattern (`pac.*`) and a short note that pushing such a tag triggers macOS and Ubuntu package uploads.

## 4. Verification

- [x] 4.1 Run shell syntax checks for `build_ubuntu.sh` and the workflow YAML where local tooling is available.
- [ ] 4.2 Run `RUN_TESTS=0 RUN_PACKAGE=0 ./build_ubuntu.sh` locally or in CI to verify the Ubuntu build path without packaging.
- [x] 4.3 Validate the OpenSpec change status before applying implementation.
