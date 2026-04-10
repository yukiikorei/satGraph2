#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-build-ubuntu}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
RUN_TESTS="${RUN_TESTS:-1}"
RUN_PACKAGE="${RUN_PACKAGE:-1}"
BUILD_JOBS="${BUILD_JOBS:-}"
SKIP_APT_INSTALL="${SKIP_APT_INSTALL:-0}"

log() {
  printf '[build_ubuntu] %s\n' "$*"
}

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    log "Missing required command: $1"
    return 1
  fi
}

detect_jobs() {
  if [[ -n "${BUILD_JOBS}" ]]; then
    printf '%s' "${BUILD_JOBS}"
    return
  fi

  local jobs
  jobs="$(nproc 2>/dev/null || true)"
  if [[ -z "${jobs}" ]]; then
    jobs="$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)"
  fi
  if [[ -z "${jobs}" ]]; then
    jobs=4
  fi
  printf '%s' "${jobs}"
}

apt_ensure_packages() {
  if [[ "${SKIP_APT_INSTALL}" == "1" ]]; then
    log "Skipping apt dependency installation (SKIP_APT_INSTALL=1)."
    return
  fi

  if ! command -v apt-get >/dev/null 2>&1; then
    log "apt-get not found; skipping Ubuntu dependency installation."
    return
  fi

  local -a packages=(
    build-essential
    pkg-config
    cmake
    ninja-build
    ccache
    dpkg-dev
    libigraph-dev
    libeigen3-dev
    nlohmann-json3-dev
    libcli11-dev
    catch2
    qt6-base-dev
    qt6-declarative-dev
    libqt6opengl6-dev
    libgl1-mesa-dev
    mesa-common-dev
    opencl-headers
    ocl-icd-opencl-dev
    ocl-icd-libopencl1
  )

  local -a missing=()
  local pkg
  for pkg in "${packages[@]}"; do
    if ! dpkg-query -W -f='${Status}' "${pkg}" 2>/dev/null | grep -q "install ok installed"; then
      missing+=("${pkg}")
    fi
  done

  if [[ "${#missing[@]}" -eq 0 ]]; then
    log "All required apt packages are already installed."
    return
  fi

  log "Installing missing apt dependencies: ${missing[*]}"
  if [[ "${EUID}" -eq 0 ]]; then
    apt-get update
    apt-get install -y --no-install-recommends "${missing[@]}"
  elif command -v sudo >/dev/null 2>&1; then
    sudo apt-get update
    sudo apt-get install -y --no-install-recommends "${missing[@]}"
  else
    log "sudo is not available and current user is not root; cannot install: ${missing[*]}"
    exit 1
  fi
}

main() {
  apt_ensure_packages

  require_cmd cmake
  require_cmd ctest
  require_cmd cpack

  local jobs
  jobs="$(detect_jobs)"

  log "Configuring project in ${BUILD_DIR}..."
  cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

  if [[ "${RUN_TESTS}" == "1" ]]; then
    log "Building all targets (including tests)..."
    cmake --build "${ROOT_DIR}/${BUILD_DIR}" -j"${jobs}"
  else
    log "Building satgraf target only..."
    cmake --build "${ROOT_DIR}/${BUILD_DIR}" --target satgraf -j"${jobs}"
  fi

  if [[ "${RUN_TESTS}" == "1" ]]; then
    log "Running test suite (offscreen)..."
    QT_QPA_PLATFORM=offscreen ctest --test-dir "${ROOT_DIR}/${BUILD_DIR}" --output-on-failure
  fi

  if [[ "${RUN_PACKAGE}" == "1" ]]; then
    log "Packaging with CPack..."
    cpack --config "${ROOT_DIR}/${BUILD_DIR}/CPackConfig.cmake" --verbose

    local deb_file
    deb_file="$(ls -t "${ROOT_DIR}"/*.deb "${ROOT_DIR}/${BUILD_DIR}"/*.deb 2>/dev/null | head -n1 || true)"
    if [[ -z "${deb_file}" ]]; then
      log "No .deb package generated in ${ROOT_DIR} or ${BUILD_DIR}"
      exit 1
    fi
    log "Generated package: ${deb_file}"
  fi

  local app_bin="${ROOT_DIR}/${BUILD_DIR}/src/app/satgraf"
  if [[ ! -x "${app_bin}" ]]; then
    log "Expected executable missing: ${app_bin}"
    exit 1
  fi

  log "Smoke check: satgraf --help"
  "${app_bin}" --help >/dev/null

  log "Done. Build directory: ${ROOT_DIR}/${BUILD_DIR}"
}

main "$@"
