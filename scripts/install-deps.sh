#!/usr/bin/env bash
set -euo pipefail

qt_packages=(qt6-base-dev qt6-base-dev-tools qt6-svg-dev)
if [[ -n "${VIVOGUARD_QT_ROOT:-}" ]]; then
  if [[ ! -f "$VIVOGUARD_QT_ROOT/lib/cmake/Qt6/Qt6Config.cmake" ]]; then
    printf 'Qt SDK desktop tidak ditemukan di %s\n' "$VIVOGUARD_QT_ROOT" >&2
    exit 1
  fi
  qt_packages=()
fi

if [[ "$(id -u)" -eq 0 ]]; then
  sudo_cmd=()
else
  sudo_cmd=(sudo)
fi

"${sudo_cmd[@]}" apt-get update
"${sudo_cmd[@]}" apt-get install -y \
  build-essential \
  cmake \
  ninja-build \
  pkg-config \
  "${qt_packages[@]}" \
  libgl1-mesa-dev \
  zip

printf '\nDependencies installed. Run: ./scripts/build.sh\n'
