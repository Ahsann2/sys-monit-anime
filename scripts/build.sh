#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${VIVOGUARD_BUILD_DIR:-${project_dir}/build}"
cmake_args=()
if [[ -n "${VIVOGUARD_QT_ROOT:-}" ]]; then
  cmake_args+=(
    "-DCMAKE_PREFIX_PATH=${VIVOGUARD_QT_ROOT}"
    "-DVIVOGUARD_QT_RUNTIME_DIR=${VIVOGUARD_QT_ROOT}/lib"
  )
fi

cmake \
  -S "${project_dir}" \
  -B "${build_dir}" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DVIVOGUARD_BUILD_UI=ON \
  -DVIVOGUARD_BUILD_TESTS=ON \
  -DVIVOGUARD_LOCAL_ARTWORK="${VIVOGUARD_LOCAL_ARTWORK:-OFF}" \
  "${cmake_args[@]}"

cmake --build "${build_dir}" --parallel "${VIVOGUARD_BUILD_JOBS:-2}"
ctest --test-dir "${build_dir}" --output-on-failure

printf '\nBuild complete:\n'
printf '  UI:    %s\n' "${build_dir}/vivoguard"
printf '  Probe: %s\n' "${build_dir}/vivoguard-probe"
