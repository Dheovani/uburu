#!/usr/bin/env sh
set -eu

appDir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"

export PATH="$appDir/usr/bin:$PATH"
export LD_LIBRARY_PATH="$appDir/usr/lib:$appDir/usr/lib64:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="$appDir/usr/plugins:${QT_PLUGIN_PATH:-}"
export QML2_IMPORT_PATH="$appDir/usr/qml:${QML2_IMPORT_PATH:-}"

exec "$appDir/usr/bin/uburu" "$@"
