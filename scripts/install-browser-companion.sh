#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
host_path="$(command -v vivoguard-browser-host || true)"

if [[ -z "${host_path}" && -x "${project_dir}/build/vivoguard-browser-host" ]]; then
  host_path="${project_dir}/build/vivoguard-browser-host"
fi

if [[ -z "${host_path}" ]]; then
  printf 'Browser host not found. Run ./scripts/build.sh or ./scripts/install.sh first.\n' >&2
  exit 1
fi

install_manifest() {
  local template_path="$1"
  local destination_path="$2"
  mkdir -p "$(dirname -- "${destination_path}")"
  sed "s|@HOST_PATH@|${host_path}|g" \
    "${template_path}" > "${destination_path}"
}

install_manifest \
  "${project_dir}/browser-extension/native-host/chromium.json.in" \
  "${HOME}/.config/google-chrome/NativeMessagingHosts/com.vivoguard.browser.json"
install_manifest \
  "${project_dir}/browser-extension/native-host/chromium.json.in" \
  "${HOME}/.config/chromium/NativeMessagingHosts/com.vivoguard.browser.json"
install_manifest \
  "${project_dir}/browser-extension/native-host/firefox.json.in" \
  "${HOME}/.mozilla/native-messaging-hosts/com.vivoguard.browser.json"

printf '\nNative browser bridge installed.\n\n'
printf 'Chromium / Chrome:\n'
printf '  1. Open chrome://extensions\n'
printf '  2. Enable Developer mode\n'
printf '  3. Load unpacked: %s/browser-extension/chromium\n' "${project_dir}"
printf '  4. Expected extension ID: hjikfphmmdebfbppaojecibekfgcllda\n\n'
printf 'Firefox:\n'
printf '  1. Open about:debugging#/runtime/this-firefox\n'
printf '  2. Load Temporary Add-on\n'
printf '  3. Select: %s/browser-extension/firefox/manifest.json\n' "${project_dir}"
