#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${project_dir}/build-core"

mkdir -p "${build_dir}"
g++ \
  -std=c++20 \
  -O2 \
  -Wall \
  -Wextra \
  -Wpedantic \
  -I"${project_dir}/src" \
  "${project_dir}/src/core/AlertEngine.cpp" \
  "${project_dir}/src/core/Cleaner.cpp" \
  "${project_dir}/src/core/HardwareProfile.cpp" \
  "${project_dir}/src/core/NvmlMonitor.cpp" \
  "${project_dir}/src/core/ProcessController.cpp" \
  "${project_dir}/src/core/SystemSampler.cpp" \
  "${project_dir}/tests/core_smoke.cpp" \
  -ldl \
  -pthread \
  -o "${build_dir}/vivoguard-core-smoke"

"${build_dir}/vivoguard-core-smoke"
