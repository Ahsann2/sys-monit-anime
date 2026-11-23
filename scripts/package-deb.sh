#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ ! -d "${project_dir}/build" ]]; then
  "${project_dir}/scripts/build.sh"
fi

cmake --build "${project_dir}/build" --target package
printf '\nPackage created under %s/build\n' "${project_dir}"
