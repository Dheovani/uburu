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

appimagetool="${APPIMAGETOOL:-}"
if [[ -z "$appimagetool" ]]; then
  appimagetool="$(command -v appimagetool || true)"
fi

qtRoot="${QT_ROOT:-}"
if [[ -z "$qtRoot" && -n "$(command -v qmake6 || true)" ]]; then
  qtRoot="$(qmake6 -query QT_INSTALL_PREFIX)"
fi
if [[ -z "$qtRoot" && -n "$(command -v qmake || true)" ]]; then
  qtRoot="$(qmake -query QT_INSTALL_PREFIX)"
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
cp "$desktopFile" "$appdir/uburu.desktop"
cp "$iconSource" "$appdir/uburu.png"
chmod +x "$appdir/AppRun" "$appdir/usr/bin/uburu"

copy_runtime_library() {
  local libraryPath="$1"
  local destinationDirectory="$appdir/usr/lib"

  if [[ ! -f "$libraryPath" ]]; then
    return
  fi

  mkdir -p "$destinationDirectory"
  cp -L "$libraryPath" "$destinationDirectory/"
}

bundle_qt_dependencies() {
  local scanRoot
  local libraryPath

  for _ in 1 2 3 4; do
    while IFS= read -r scanRoot; do
      while IFS= read -r libraryPath; do
        case "$libraryPath" in
          "$qtRoot"/*)
            copy_runtime_library "$libraryPath"
            ;;
        esac
      done < <(LD_LIBRARY_PATH="$appdir/usr/lib:$qtRoot/lib:${LD_LIBRARY_PATH:-}" ldd "$scanRoot" 2>/dev/null |
        awk '/=>/ { print $3 }')
    done < <(find "$appdir/usr/bin" "$appdir/usr/lib" "$appdir/usr/plugins" "$appdir/usr/qml" -type f)
  done
}

bundle_manual_appdir() {
  if [[ -z "$appimagetool" ]]; then
    echo "appimagetool was not found. Set APPIMAGETOOL or add appimagetool to PATH." >&2
    exit 1
  fi

  if [[ -z "$qtRoot" ]]; then
    echo "Qt prefix was not found. Set QT_ROOT or add qmake/qmake6 to PATH." >&2
    exit 1
  fi

  mkdir -p "$appdir/usr/lib" "$appdir/usr/plugins" "$appdir/usr/qml"

  while IFS= read -r libraryPath; do
    case "$libraryPath" in
      "$qtRoot"/*)
        copy_runtime_library "$libraryPath"
        ;;
    esac
  done < <(ldd "$appdir/usr/bin/uburu" | awk '/=>/ { print $3 }')

  if [[ -d "$qtRoot/plugins" ]]; then
    cp -a "$qtRoot/plugins/." "$appdir/usr/plugins/"
  fi

  if [[ -d "$qtRoot/qml" ]]; then
    cp -a "$qtRoot/qml/." "$appdir/usr/qml/"
  fi

  bundle_qt_dependencies

  ARCH=x86_64 "$appimagetool" "$appdir" "$appimagePath"
}

if [[ -n "$linuxdeployqt" ]]; then
  linuxdeployqtArguments=(
    "$appdir/usr/share/applications/uburu.desktop"
    "-appimage"
    "-bundle-non-qt-libs"
    "-qmldir=$root/apps/desktop/qml"
  )

  if [[ "$allowNewGlibc" == "1" ]]; then
    linuxdeployqtArguments+=("-unsupported-allow-new-glibc")
  fi

  if ! (
    cd "$root/$outputDirectory"
    "$linuxdeployqt" "${linuxdeployqtArguments[@]}"
  ); then
    echo "linuxdeployqt failed; falling back to manual Qt AppDir bundling." >&2
    bundle_manual_appdir
  fi
else
  bundle_manual_appdir
fi

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
