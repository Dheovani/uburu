#!/usr/bin/env bash
set -euo pipefail

preset="${UBURU_MACOS_PRESET:-macos-release}"
buildDirectory="${UBURU_MACOS_BUILD_DIRECTORY:-build/macos-release}"
outputDirectory="${UBURU_MACOS_OUTPUT_DIRECTORY:-dist/macos}"
appName="${UBURU_MACOS_APP_NAME:-Uburu}"
packageName="${UBURU_MACOS_PACKAGE_NAME:-uburu-macos}"
skipBuild="${UBURU_SKIP_BUILD:-0}"
signIdentity="${UBURU_MACOS_CODESIGN_IDENTITY:-}"
notaryProfile="${UBURU_MACOS_NOTARY_PROFILE:-}"

scriptDirectory="$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
root="$(CDPATH= cd -- "$scriptDirectory/.." && pwd)"
buildApp="$root/$buildDirectory/apps/desktop/uburu_desktop.app"
outputPath="$root/$outputDirectory"
appPath="$outputPath/$appName.app"
dmgPath="$outputPath/$packageName.dmg"
checksumPath="$dmgPath.sha256"

if [[ "$skipBuild" != "1" ]]; then
  cmake --preset "$preset"
  cmake --build --preset "$preset" --target uburu_desktop
fi

if [[ ! -d "$buildApp" ]]; then
  echo "macOS app bundle not found: $buildApp" >&2
  exit 1
fi

macdeployqt="${MACDEPLOYQT:-}"
if [[ -z "$macdeployqt" ]]; then
  macdeployqt="$(command -v macdeployqt || true)"
fi

if [[ -z "$macdeployqt" && -n "${QT_ROOT:-}" && -x "$QT_ROOT/bin/macdeployqt" ]]; then
  macdeployqt="$QT_ROOT/bin/macdeployqt"
fi

if [[ -z "$macdeployqt" ]]; then
  echo "macdeployqt was not found. Set MACDEPLOYQT, QT_ROOT, or add macdeployqt to PATH." >&2
  exit 1
fi

rm -rf "$appPath" "$dmgPath" "$checksumPath"
mkdir -p "$outputPath"
cp -R "$buildApp" "$appPath"

"$macdeployqt" "$appPath" -qmldir="$root/apps/desktop/qml" -verbose=1

if [[ -n "$signIdentity" ]]; then
  codesign --force --deep --options runtime --timestamp --sign "$signIdentity" "$appPath"
else
  echo "Skipping codesign because UBURU_MACOS_CODESIGN_IDENTITY is not set."
fi

hdiutil create -volname "$appName" -srcfolder "$appPath" -ov -format UDZO "$dmgPath"

if [[ -n "$signIdentity" ]]; then
  codesign --force --timestamp --sign "$signIdentity" "$dmgPath"
fi

if [[ -n "$notaryProfile" ]]; then
  xcrun notarytool submit "$dmgPath" --keychain-profile "$notaryProfile" --wait
  xcrun stapler staple "$dmgPath"
else
  echo "Skipping notarization because UBURU_MACOS_NOTARY_PROFILE is not set."
fi

shasum -a 256 "$dmgPath" | sed "s#  .*/#  #" > "$checksumPath"

echo "App bundle: $appPath"
echo "DMG: $dmgPath"
echo "SHA-256: $checksumPath"
