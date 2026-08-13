#!/usr/bin/env bash
set -euo pipefail

display="${DISPLAY:-:1}"
runtimeDirectory="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"

env -i \
  DISPLAY="$display" \
  XDG_RUNTIME_DIR="$runtimeDirectory" \
  HOME="$HOME" \
  USER="${USER:-$(id -un)}" \
  LANG="${LANG:-C.UTF-8}" \
  PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" \
  peek "$@"
