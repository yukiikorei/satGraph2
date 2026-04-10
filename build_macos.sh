#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-build-macos}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
RUN_TESTS="${RUN_TESTS:-1}"
RUN_PACKAGE="${RUN_PACKAGE:-1}"
UPDATE_BREW="${UPDATE_BREW:-0}"
BUILD_JOBS="${BUILD_JOBS:-}"
MACOS_DEPLOY_QT="${MACOS_DEPLOY_QT:-1}"
AUDIT_BUNDLE_DEPS="${AUDIT_BUNDLE_DEPS:-1}"

log() {
  printf '[build_macos] %s\n' "$*"
}

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    log "Missing required command: $1"
    return 1
  fi
}

ensure_brew_formula() {
  local formula="$1"
  if ! brew list --formula "$formula" >/dev/null 2>&1; then
    log "Installing missing dependency: $formula"
    brew install "$formula"
  else
    log "Dependency already present: $formula"
  fi
}

audit_bundle_dependencies() {
  local app_bundle="$1"
  require_cmd otool
  require_cmd file

  local -i leaked=0
  local bin
  while IFS= read -r bin; do
    if ! file "${bin}" | grep -q "Mach-O"; then
      continue
    fi

    local dep
    while IFS= read -r dep; do
      dep="${dep%% *}"
      [[ -z "${dep}" ]] && continue

      case "${dep}" in
        @rpath/*|@loader_path/*|@executable_path/*|/System/*|/usr/lib/*)
          continue
          ;;
      esac

      # Any absolute non-system path indicates the app still depends on host libraries.
      if [[ "${dep}" = /* ]]; then
        log "External dependency leak: ${bin} -> ${dep}"
        leaked=1
      fi
    done < <(otool -L "${bin}" | tail -n +2)
  done < <(find "${app_bundle}" -type f)

  if (( leaked != 0 )); then
    log "Bundle contains non-system absolute library references."
    exit 1
  fi
}

main() {
  require_cmd xcode-select
  require_cmd brew
  require_cmd cmake

  if ! xcode-select -p >/dev/null 2>&1; then
    log "Xcode Command Line Tools are not configured."
    log "Run: xcode-select --install"
    exit 1
  fi

  local developer_dir
  developer_dir="$(xcode-select -p)"
  if [[ "${developer_dir}" != "/Library/Developer/CommandLineTools" ]]; then
    log "xcode-select currently points to: ${developer_dir}"
    log "Homebrew bottles for this setup expect CommandLineTools at /Library/Developer/CommandLineTools."
    log "If dependency install fails, run xcode-select --install and switch to CommandLineTools."
  fi

  log "Installing Homebrew dependencies (missing only)..."
  if [[ "${UPDATE_BREW}" == "1" ]]; then
    brew update
  fi

  # Some environments have both pkg-config and pkgconf installed and linked to
  # the same path. Resolve that before any dependency installs.
  if brew list --formula pkg-config >/dev/null 2>&1 && brew list --formula pkgconf >/dev/null 2>&1; then
    brew unlink pkg-config >/dev/null 2>&1 || true
    brew link --overwrite pkgconf >/dev/null 2>&1 || true
  fi

  ensure_brew_formula igraph
  ensure_brew_formula eigen
  ensure_brew_formula nlohmann-json
  ensure_brew_formula cli11
  ensure_brew_formula catch2
  ensure_brew_formula qt

  local qt_prefix
  qt_prefix="$(brew --prefix qt)"
  export CMAKE_PREFIX_PATH="${qt_prefix}:${CMAKE_PREFIX_PATH:-}"

  local jobs
  if [[ -n "${BUILD_JOBS}" ]]; then
    jobs="${BUILD_JOBS}"
  else
    jobs="$(sysctl -n hw.ncpu 2>/dev/null || true)"
    if [[ -z "${jobs}" ]]; then
      jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)"
    fi
    if [[ -z "${jobs}" ]]; then
      jobs=4
    fi
  fi

  log "Configuring project in ${BUILD_DIR}..."
  cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH}" \
    -DSATGRAF_MACOS_RUN_MACDEPLOYQT="${MACOS_DEPLOY_QT}"

  if [[ "${RUN_TESTS}" == "1" ]]; then
    log "Building all targets (including tests)..."
    cmake --build "${ROOT_DIR}/${BUILD_DIR}" -j"${jobs}"
  else
    log "Building satgraf target only..."
    cmake --build "${ROOT_DIR}/${BUILD_DIR}" --target satgraf -j"${jobs}"
  fi

  if [[ "${RUN_TESTS}" == "1" ]]; then
    log "Running test suite..."
    ctest --test-dir "${ROOT_DIR}/${BUILD_DIR}" --output-on-failure
  fi

  if [[ "${RUN_PACKAGE}" == "1" ]]; then
    log "Packaging with CPack..."
    cpack --config "${ROOT_DIR}/${BUILD_DIR}/CPackConfig.cmake" --verbose

    local dmg_file
    dmg_file="$(ls -t "${ROOT_DIR}"/*.dmg "${ROOT_DIR}/${BUILD_DIR}"/*.dmg 2>/dev/null | head -n1 || true)"
    if [[ -z "${dmg_file}" ]]; then
      log "No .dmg package generated in ${ROOT_DIR} or ${BUILD_DIR}"
      exit 1
    fi
    log "Generated package: ${dmg_file}"

    local mount_dir
    mount_dir="$(mktemp -d /tmp/satgraf-dmg-XXXXXX)"
    if hdiutil attach "${dmg_file}" -mountpoint "${mount_dir}" -nobrowse >/dev/null; then
      if [[ ! -d "${mount_dir}/satgraf.app" ]]; then
        log "satgraf.app not found in mounted DMG: ${mount_dir}"
        hdiutil detach "${mount_dir}" >/dev/null || true
        rm -rf "${mount_dir}"
        exit 1
      fi
      hdiutil detach "${mount_dir}" >/dev/null || true
    else
      rm -rf "${mount_dir}"
      log "Failed to mount DMG for inspection"
      exit 1
    fi
    rm -rf "${mount_dir}"

    local app_bundle="${ROOT_DIR}/${BUILD_DIR}/src/app/satgraf.app"
    if [[ ! -d "${app_bundle}" ]]; then
      log "Expected app bundle missing: ${app_bundle}"
      exit 1
    fi

    if [[ "${AUDIT_BUNDLE_DEPS}" == "1" ]]; then
      log "Auditing bundle dependencies for host-library leaks..."
      audit_bundle_dependencies "${app_bundle}"
    fi

    log "Smoke check: app binary --help"
    "${app_bundle}/Contents/MacOS/satgraf" --help >/dev/null
  fi

  log "Done. Build directory: ${ROOT_DIR}/${BUILD_DIR}"
}

main "$@"
