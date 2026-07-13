# Release packaging

This document records the release packaging path for Uburu. Packaging is intentionally incremental: a reproducible folder/ZIP bundle comes first, then installer, signing, update channel, and platform-specific distribution formats.

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

## Release notes and asset manifest

Release notes are versioned under `docs/releases/`. For a manual GitHub release, paste the matching note into the release description and attach the generated binaries/checksums as release assets.

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

## Upgrade, uninstall, and rollback policy

Installers must write application binaries only under the selected installation directory. User state belongs under the platform application-data location documented in `docs/storage.md`, currently `%LOCALAPPDATA%/uburu/uburu.db` on Windows. This keeps install/uninstall separate from settings, history, saved searches, and index data.

The first Windows installer preserves user data during uninstall. A future installer may offer an explicit "remove local data" option, but it must never silently delete the user's index, settings, history, or saved searches. Upgrades should install over the previous application directory while leaving the database in place for the storage migration/recovery layer to validate.

Rollback must be conservative before `1.0.0`: if a user installs an older Uburu over a newer database, the older build may refuse to open unsupported storage or require a safe rebuild of the index catalog. Release notes must call out any storage schema, indexed-document format, CLI contract, settings format, or extractor behavior change that affects rollback.

Before publishing a release, validate:

- whether the new version can open the previous version's database;
- whether interrupted indexing still recovers without corrupting preferences;
- whether uninstall removes the application directory while preserving the local data directory;
- whether reinstalling the same version reuses preserved settings and index state;
- whether rollback behavior is documented when schema or format versions change.

## Update channels

Uburu starts with manual release channels instead of an automatic updater:

- `preview`: pre-`1.0.0` builds intended for validation, early users, and controlled feedback;
- `stable`: builds that meet the V1 gates and have no known critical correctness, data-loss, installation, or startup issues.

The `v0.x.y` series belongs to the preview channel by default. GitHub releases for this series should be marked as pre-releases unless a specific build is promoted for broader use. Stable releases begin at `v1.0.0` or later after Windows packaging, supported-platform documentation, release notes, license review, and clean-machine validation are complete.

Patch releases must stay compatible with the same channel and must not introduce database, settings, CLI, or index-format changes unless the release notes explicitly say why. Minor preview releases may change these contracts, but must describe migration and rollback impact.

An automatic updater is intentionally out of scope for the first Windows preview. When added, it must verify signatures or trusted checksums before replacing binaries, must not run heavy migrations without user-visible progress, and must preserve the same storage and rollback rules described above.

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

The remaining packaging work includes:

- code signing;
- macOS bundle, signing, and notarization;
- Linux AppImage validation and future Flatpak manifest;
- automatic update transport.
