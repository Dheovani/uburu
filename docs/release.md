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
- Linux AppImage and Flatpak evaluation;
- SBOM generation;
- update channel policy;
- rollback and database compatibility checklist.
