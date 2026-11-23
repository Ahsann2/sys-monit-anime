#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${VIVOGUARD_BUILD_DIR:-${project_dir}/build}"
bin_dir="${VIVOGUARD_BIN_DIR:-}"
if [[ -z "$bin_dir" ]]; then
  if [[ ! -x "$build_dir/sys-monit-anime" ]]; then
    "$project_dir/scripts/build.sh"
  fi
  cmake --install "$build_dir" --prefix "$build_dir/staged"
  bin_dir="$build_dir/staged/bin"
fi

for binary in sys-monit-anime sys-monit-anime-probe sys-monit-anime-browser-host; do
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
launcher="$data_target/applications/sys-monit-anime.desktop"
autostart="$config_target/autostart/sys-monit-anime.desktop"
stage="$(mktemp -d)"
cleanup_stage() {
  local file
  for file in "$stage/sys-monit-anime.desktop" "$stage/autostart.desktop"; do
    [[ ! -e "$file" ]] || unlink "$file"
  done
  rmdir "$stage"
}
trap cleanup_stage EXIT
generate_launcher() {
  local template="$1" output="$2" line
  while IFS= read -r line; do
    case "$line" in
      Exec=sys-monit-anime*) printf 'Exec="%s/sys-monit-anime"%s\n' "$bin_target" "${line#Exec=sys-monit-anime}" ;;
      *) printf '%s\n' "$line" ;;
    esac
  done < "$template" > "$output"
  if command -v desktop-file-validate >/dev/null; then
    desktop-file-validate "$output"
  fi
}
generate_launcher "$project_dir/packaging/sys-monit-anime.desktop" "$stage/sys-monit-anime.desktop"

# Repair an existing login entry while retaining its enabled/disabled state.
if [[ -f "$autostart" ]]; then
  while IFS= read -r line; do
    case "$line" in
      Exec=*) printf 'Exec="%s/sys-monit-anime" --tray\n' "$bin_target" ;;
      Name=*) printf 'Name=Monitor Sistem\n' ;;
      Comment=*) printf 'Comment=Pantau perangkat keras setelah login\n' ;;
      TryExec=*) printf 'TryExec=%s/sys-monit-anime\n' "$bin_target" ;;
      *) printf '%s\n' "$line" ;;
    esac
  done < "$autostart" > "$stage/autostart.desktop"
  if command -v desktop-file-validate >/dev/null; then
    desktop-file-validate "$stage/autostart.desktop"
  fi
fi

for binary in sys-monit-anime-probe sys-monit-anime-browser-host sys-monit-anime; do
  put_file "$bin_dir/$binary" "$bin_target/$binary" 755
done
put_file "$project_dir/assets/vivoguard.png" \
  "$data_target/icons/hicolor/512x512/apps/sys-monit-anime.png" 644
put_file "$stage/sys-monit-anime.desktop" "$launcher" 644
if [[ -f "$stage/autostart.desktop" ]]; then
  put_file "$stage/autostart.desktop" "$autostart" 644
fi
if [[ -f "$HOME/Desktop/sys-monit-anime.desktop" ]]; then
  put_file "$stage/sys-monit-anime.desktop" "$HOME/Desktop/sys-monit-anime.desktop" 755
fi
printf 'Monitor Sistem terpasang di %s\n' "$bin_target/sys-monit-anime"
