# Release packaging

This document records the release packaging path for Uburu. Packaging is intentionally incremental: a reproducible folder/ZIP bundle comes first, then installer, optional signing, release channels, and platform-specific distribution formats that can actually be validated.

## Versioning

Uburu uses semantic versioning for public releases:

- `MAJOR`: incompatible database, CLI, configuration, or plugin-contract changes;
- `MINOR`: new features, supported formats, UI capabilities, or compatible index/storage upgrades;
- `PATCH`: bug fixes, extractor safety fixes, packaging fixes, and documentation corrections.

Before `1.0.0`, compatibility may still change, but release notes must describe storage, index, CLI, and settings impact explicitly.

## Windows MSVC portable bundle

The first Windows release artifact is a portable folder and ZIP package. It includes:

- the desktop executable;
- Qt runtime files collected by `windeployqt`;
- required vcpkg DLLs from the Release build tree;
- README files, license, license notes, a release manifest, and a SHA-256 checksum for the ZIP.

Run from PowerShell:

```powershell
.\scripts\package-windows-msvc-desktop.ps1
```

The default output is:

```txt
dist/windows-msvc-release/uburu-windows-msvc-x64/
dist/windows-msvc-release/uburu-windows-msvc-x64.zip
dist/windows-msvc-release/uburu-windows-msvc-x64.sha256
```

Use `-SkipBuild` only after a successful Release build. Use `-SkipArchive` when inspecting the deployed folder without producing a ZIP.

## Windows installer strategy

The first Windows installer path uses a traditional installer generated with Inno Setup 6. This is the preferred first-release option because it is simple to validate, works well for per-user installation, does not require Microsoft Store packaging, and can install the same `windeployqt` bundle already used by the portable artifact.

MSIX remains a future candidate, but it is not the first Uburu installer target. It has advantages for clean install/uninstall and update channels, but it adds packaging identity, signing, certificate, and distribution constraints that are premature before the first Windows release is validated on clean machines.

The traditional installer is configured as a per-user install under:

```txt
%LOCALAPPDATA%\Programs\Uburu
```

That keeps the first installer usable without elevation and avoids writing user data next to the executable. Settings, cache, logs, and indexes must remain in the platform storage location documented in `docs/storage.md`.

To build the installer, install Inno Setup 6 and run:

```powershell
.\scripts\build-windows-installer.ps1
```

The script builds the portable Release bundle first, then emits:

```txt
dist/windows-msvc-release/installer/uburu-setup-<version>-windows-x64.exe
dist/windows-msvc-release/installer/uburu-setup-<version>-windows-x64.exe.sha256
```

If `ISCC.exe` is not in `PATH`, the script also checks the default `Program Files` installation directories.

## Windows signing strategy

Unsigned preview builds are acceptable only for internal validation and early previews. Public Windows releases should sign at least the desktop executable and installer with an Authenticode code-signing certificate. After signing binaries inside the portable folder, regenerate the ZIP, installer, checksums, and release-assets manifest so the published artifacts contain the signed executable.

The helper script expects a local `.pfx` certificate and the Windows SDK `signtool.exe`:

```powershell
.\scripts\sign-windows-release.ps1 -CertificatePath path\to\certificate.pfx
```

If the certificate requires a password:

```powershell
.\scripts\sign-windows-release.ps1 -CertificatePath path\to\certificate.pfx -CertificatePassword "..."
```

The script is intentionally not run by default because signing material must not be committed, copied into release bundles, or stored in the repository.

## Release notes and asset manifest

Release notes are versioned under `docs/releases/`. For a manual GitHub release, paste the matching note into the release description and attach the generated binaries/checksums as release assets.

Validation records also live under `docs/releases/`. The `v0.1.0` preview uses `docs/releases/v0.1.0-validation.md` to separate what was actually tested from optional future work such as signing certificates, Flatpak packaging, or unsupported platforms.

To prepare the Windows release assets and a machine-readable manifest in one step, run:

```powershell
.\scripts\prepare-windows-release.ps1 -AppVersion v0.1.0
```

The script builds or refreshes the portable bundle, builds the installer, copies the release notes to the distribution directory, regenerates checksums, and writes:

```txt
dist/windows-msvc-release/release-assets.json
dist/windows-msvc-release/RELEASE-NOTES.md
dist/windows-msvc-release/uburu-windows-msvc-x64.spdx.json
dist/windows-msvc-release/THIRD-PARTY-NOTICES.md
```

The SBOM is an initial SPDX 2.3 JSON report generated from the release bundle and `vcpkg.json`. It is useful for release review and dependency tracking, but it does not replace legal review of Qt, LGPL/commercial obligations, or third-party notices before commercial distribution.

## Upgrade, uninstall, and compatibility policy

Installers must write application binaries only under the selected installation directory. User state belongs under the platform application-data location documented in `docs/storage.md`, currently `%LOCALAPPDATA%/uburu/uburu.db` on Windows. This keeps install/uninstall separate from settings, history, saved searches, and index data.

The first Windows installer preserves user data during uninstall. A future installer may offer an explicit "remove local data" option, but it must never silently delete the user's index, settings, history, or saved searches. Installing a newer build over an older one should leave the database in place for the storage migration/recovery layer to validate.

Before `1.0.0`, compatibility may change. If a release changes the storage schema, indexed-document format, CLI contract, settings format, or extractor behavior, the release notes must say what changed and whether older builds can safely open the resulting local data. The first preview only requires validation of the artifacts that were actually built and tested.

Before publishing a release, validate:

- whether interrupted indexing still recovers without corrupting preferences;
- whether uninstall removes the application directory while preserving the local data directory;
- whether reinstalling the same version reuses preserved settings and index state;
- whether compatibility impact is documented when schema or format versions change.

## Update channels

Uburu starts with manual release channels instead of an automatic updater:

- `preview`: pre-`1.0.0` builds intended for validation, early users, and controlled feedback;
- `stable`: builds that meet the V1 gates and have no known critical correctness, data-loss, installation, or startup issues.

The `v0.x.y` series belongs to the preview channel by default. GitHub releases for this series should be marked as pre-releases unless a specific build is promoted for broader use. Stable releases begin at `v1.0.0` or later after Windows packaging, supported-platform documentation, release notes, license review, and clean-machine validation are complete.

Patch releases must stay compatible with the same channel and must not introduce database, settings, CLI, or index-format changes unless the release notes explicitly say why. Minor preview releases may change these contracts, but must describe the compatibility impact.

An automatic updater is intentionally out of scope for the first Windows preview. If one is added later, it must verify signatures or trusted checksums before replacing binaries, must not run heavy migrations without user-visible progress, and must preserve the same storage safety rules described above.

## Linux packaging strategy

The first Linux packaging path is an AppImage built from a Qt desktop Release build. AppImage is the preferred first Linux artifact because it can be validated from a single file and does not require setting up a Flatpak runtime, portal permissions, repository hosting, or sandbox policy before the Linux desktop behavior itself is tested.

Flatpak remains a strong future target for distribution, sandboxing, desktop integration, and update channels, but it requires a separate manifest, runtime selection, filesystem portal review, and a clear policy for user-selected directory access. Those details should be handled after the AppImage smoke path is working.

To build the initial AppImage on Linux, install `linuxdeployqt`, configure `QT_ROOT` when Qt is not available from the system prefix, and run:

```sh
bash ./scripts/package-linux-appimage.sh
```

If `linuxdeployqt` is not on `PATH`, set:

```sh
export LINUXDEPLOYQT=/path/to/linuxdeployqt
```

If `linuxdeployqt` cannot complete the bundle on the current machine, set `APPIMAGETOOL` as well. The packaging
script then falls back to a manual Qt AppDir bundle, copies Qt plugin and QML dependencies from `QT_ROOT`, and uses
`appimagetool` to create the final AppImage.

On newer Linux distributions, `linuxdeployqt` may reject the host glibc as too new for broadly compatible AppImages. For local validation only, use:

```sh
UBURU_LINUXDEPLOYQT_ALLOW_NEW_GLIBC=1 bash ./scripts/package-linux-appimage.sh
```

The default output is:

```txt
dist/linux-appimage/uburu-linux-x86_64.AppImage
dist/linux-appimage/uburu-linux-x86_64.AppImage.sha256
```

Validate the checksum from the output directory:

```sh
cd dist/linux-appimage
sha256sum -c uburu-linux-x86_64.AppImage.sha256
```

If the validation machine does not have `libfuse.so.2`, smoke-test the AppImage through AppImage's extract-and-run
mode:

```sh
APPIMAGE_EXTRACT_AND_RUN=1 ./uburu-linux-x86_64.AppImage
```

Before publishing a Linux artifact, validate the AppImage on a clean distribution or VM older than the build host, confirm the file picker and file-opening actions work under the target desktop environment, and confirm user-selected paths are accessible without requiring the app to run from the repository tree.

## Optional future macOS packaging

macOS is not a supported release target for the first preview because there is no committed macOS validation environment. The existing script is only groundwork for a future contributor or maintainer with access to real macOS hardware.

If macOS becomes a supported release target later, the path starts with the CMake app bundle generated by `MACOSX_BUNDLE`, then uses `macdeployqt` to copy Qt frameworks and QML dependencies. The script also creates a compressed `.dmg` so the artifact can be tested manually before signing/notarization is wired into CI.

Run on macOS:

```sh
bash ./scripts/package-macos-bundle.sh
```

Optional signing and notarization are controlled by environment variables:

```sh
export UBURU_MACOS_CODESIGN_IDENTITY="Developer ID Application: ..."
export UBURU_MACOS_NOTARY_PROFILE="uburu-notary"
bash ./scripts/package-macos-bundle.sh
```

The default output is:

```txt
dist/macos/Uburu.app
dist/macos/uburu-macos.dmg
dist/macos/uburu-macos.dmg.sha256
```

Before publishing a macOS artifact in the future, validate launch from the `.app`, launch from the mounted `.dmg`, quarantine behavior after download, Gatekeeper behavior after signing/notarization, file picker access, opening result files in Finder, and preservation of local application data across reinstall/uninstall.

## Manual validation checklist

Validate the portable folder on a clean Windows machine or VM before calling the artifact releasable:

- start `uburu_desktop.exe` without modifying the system `PATH`;
- search a small plain-text folder;
- search a folder containing PDF, DOCX, XLSX, PPTX, ODT, RTF, HTML, and subtitle files;
- cancel a long search and confirm the UI remains responsive;
- index a small folder and confirm status/progress is visible;
- open a result preview and file action menu;
- confirm settings persist after restart;
- delete the folder and confirm no files are left next to the executable except user data stored in the configured platform location.

Validate the installer separately:

- install without administrator elevation;
- launch from the Start Menu shortcut;
- optionally create and launch from the desktop shortcut;
- search a small folder and a supported document-format folder;
- restart the app and confirm settings persist;
- uninstall through Windows settings;
- confirm the install directory is removed;
- confirm user data is preserved or removed according to the uninstall policy selected for the release.

## Pending release work

Optional future packaging work includes:

- real code-signing certificates and signed artifact validation;
- macOS bundle validation, signing credentials, and notarization if macOS becomes a supported target;
- future Flatpak manifest;
- automatic update transport if the project later needs one.
