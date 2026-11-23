#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${VIVOGUARD_BUILD_DIR:-${project_dir}/build}"
bin_dir="${VIVOGUARD_BIN_DIR:-}"
if [[ -z "$bin_dir" ]]; then
  if [[ ! -x "$build_dir/vivoguard" ]]; then
    "$project_dir/scripts/build.sh"
  fi
  cmake --install "$build_dir" --prefix "$build_dir/staged"
  bin_dir="$build_dir/staged/bin"
fi

for binary in vivoguard vivoguard-probe vivoguard-browser-host; do
  if [[ ! -x "$bin_dir/$binary" ]]; then
    printf 'Binary belum tersedia: %s\n' "$bin_dir/$binary" >&2
    exit 1
  fi
done

# Stage replacements on the same filesystem and activate each with rename.
put_file() {
  local source="$1" destination="$2" mode="$3" temporary
  mkdir -p "$(dirname "$destination")"
  temporary="$(mktemp "${destination}.XXXXXX")"
  if ! install -m "$mode" -- "$source" "$temporary" ||
     ! mv -fT -- "$temporary" "$destination"; then
    [[ ! -e "$temporary" ]] || unlink "$temporary"
    return 1
  fi
}

bin_target="$HOME/.local/bin"
data_target="${XDG_DATA_HOME:-$HOME/.local/share}"
config_target="${XDG_CONFIG_HOME:-$HOME/.config}"
launcher="$data_target/applications/vivoguard.desktop"
autostart="$config_target/autostart/vivoguard.desktop"
stage="$(mktemp -d)"
cleanup_stage() {
  local file
  for file in "$stage/vivoguard.desktop" "$stage/autostart.desktop"; do
    [[ ! -e "$file" ]] || unlink "$file"
  done
  rmdir "$stage"
}
trap cleanup_stage EXIT
generate_launcher() {
  local template="$1" output="$2" line
  while IFS= read -r line; do
    case "$line" in
      Exec=vivoguard*) printf 'Exec="%s/vivoguard"%s\n' "$bin_target" "${line#Exec=vivoguard}" ;;
      *) printf '%s\n' "$line" ;;
    esac
  done < "$template" > "$output"
  if command -v desktop-file-validate >/dev/null; then
    desktop-file-validate "$output"
  fi
}
generate_launcher "$project_dir/packaging/vivoguard.desktop" "$stage/vivoguard.desktop"

# Repair an existing login entry while retaining its enabled/disabled state.
if [[ -f "$autostart" ]]; then
  while IFS= read -r line; do
    case "$line" in
      Exec=*) printf 'Exec="%s/vivoguard" --tray\n' "$bin_target" ;;
      Name=*) printf 'Name=Monitor Sistem\n' ;;
      Comment=*) printf 'Comment=Pantau perangkat keras setelah login\n' ;;
      TryExec=*) printf 'TryExec=%s/vivoguard\n' "$bin_target" ;;
      *) printf '%s\n' "$line" ;;
    esac
  done < "$autostart" > "$stage/autostart.desktop"
  if command -v desktop-file-validate >/dev/null; then
    desktop-file-validate "$stage/autostart.desktop"
  fi
fi

for binary in vivoguard-probe vivoguard-browser-host vivoguard; do
  put_file "$bin_dir/$binary" "$bin_target/$binary" 755
done
put_file "$project_dir/assets/vivoguard.png" \
  "$data_target/icons/hicolor/512x512/apps/vivoguard.png" 644
put_file "$stage/vivoguard.desktop" "$launcher" 644
if [[ -f "$stage/autostart.desktop" ]]; then
  put_file "$stage/autostart.desktop" "$autostart" 644
fi
if [[ -f "$HOME/Desktop/vivoguard.desktop" ]]; then
  put_file "$stage/vivoguard.desktop" "$HOME/Desktop/vivoguard.desktop" 755
fi
printf 'Monitor Sistem terpasang di %s\n' "$bin_target/vivoguard"
