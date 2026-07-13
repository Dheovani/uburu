#!/usr/bin/env bash
set -euo pipefail

preset="${UBURU_LINUX_PRESET:-linux-release}"
buildDirectory="${UBURU_LINUX_BUILD_DIRECTORY:-build/linux-release}"
outputDirectory="${UBURU_LINUX_OUTPUT_DIRECTORY:-dist/linux-appimage}"
appdirName="${UBURU_LINUX_APPDIR_NAME:-Uburu.AppDir}"
packageName="${UBURU_LINUX_PACKAGE_NAME:-uburu-linux-x86_64.AppImage}"
skipBuild="${UBURU_SKIP_BUILD:-0}"
allowNewGlibc="${UBURU_LINUXDEPLOYQT_ALLOW_NEW_GLIBC:-0}"

scriptDirectory="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(CDPATH= cd -- "$scriptDirectory/.." && pwd)"
appdir="$root/$outputDirectory/$appdirName"
desktopFile="$root/packaging/linux/uburu.desktop"
appRunTemplate="$root/packaging/linux/app-run.sh"
iconSource="$root/apps/desktop/assets/logo-uburu.png"
executable="$root/$buildDirectory/apps/desktop/uburu_desktop"
appimagePath="$root/$outputDirectory/$packageName"
checksumPath="$appimagePath.sha256"

if [[ "$skipBuild" != "1" ]]; then
  cmake --preset "$preset"
  cmake --build --preset "$preset" --target uburu_desktop
fi

if [[ ! -x "$executable" ]]; then
  echo "Desktop executable not found or not executable: $executable" >&2
  exit 1
fi

linuxdeployqt="${LINUXDEPLOYQT:-}"
if [[ -z "$linuxdeployqt" ]]; then
  linuxdeployqt="$(command -v linuxdeployqt || true)"
fi

if [[ -z "$linuxdeployqt" ]]; then
  echo "linuxdeployqt was not found. Set LINUXDEPLOYQT or add linuxdeployqt to PATH." >&2
  exit 1
fi

rm -rf "$appdir"
mkdir -p \
  "$appdir/usr/bin" \
  "$appdir/usr/share/applications" \
  "$appdir/usr/share/icons/hicolor/256x256/apps"

cp "$executable" "$appdir/usr/bin/uburu"
cp "$desktopFile" "$appdir/usr/share/applications/uburu.desktop"
cp "$iconSource" "$appdir/usr/share/icons/hicolor/256x256/apps/uburu.png"
cp "$appRunTemplate" "$appdir/AppRun"
chmod +x "$appdir/AppRun" "$appdir/usr/bin/uburu"

linuxdeployqtArguments=(
  "$appdir/usr/share/applications/uburu.desktop"
  "-appimage"
  "-bundle-non-qt-libs"
  "-qmldir=$root/apps/desktop/qml"
)

if [[ "$allowNewGlibc" == "1" ]]; then
  linuxdeployqtArguments+=("-unsupported-allow-new-glibc")
fi

(
  cd "$root/$outputDirectory"
  "$linuxdeployqt" "${linuxdeployqtArguments[@]}"
)

generatedAppimage="$(find "$root/$outputDirectory" -maxdepth 1 -type f -name '*.AppImage' -print -quit)"
if [[ -z "$generatedAppimage" ]]; then
  echo "linuxdeployqt did not generate an AppImage in $root/$outputDirectory." >&2
  exit 1
fi

if [[ "$generatedAppimage" != "$appimagePath" ]]; then
  mv -f "$generatedAppimage" "$appimagePath"
fi

sha256sum "$appimagePath" | sed "s#  .*/#  #" > "$checksumPath"

echo "AppImage: $appimagePath"
echo "SHA-256: $checksumPath"
