# Licenses and redistribution

This document records Uburu's initial licensing policy. It does not replace legal review before public builds are distributed, but it prevents the project from treating licenses as a late surprise.

## Project license

Uburu's own code is distributed under the MIT license, as stated in the root `LICENSE` file.

## Main dependencies

| Dependency | Project use | Initial note |
| --- | --- | --- |
| Qt 6 | Qt Quick/QML desktop interface | Verify used modules and licensing mode before distribution. |
| Catch2 | Automated tests | Development dependency. |
| SQLite | Local persistence | May be provided by the system or vcpkg. |
| PCRE2 | Regex with JIT when available | Must remain optional in the core. |
| libgit2 | Git integration | Must remain behind an explicit adapter. |

## Qt

Qt is available under commercial and open-source options. The official Qt documentation states that:

- commercial licenses are appropriate when the project does not want to, or cannot, comply with LGPL/GPL terms;
- Qt under LGPLv3 can be appropriate when all LGPLv3 requirements are met;
- some open-source Qt modules are GPL-only and require special care;
- Qt includes third-party code with its own licenses;
- Qt 6.8+ publishes SBOMs for third-party components.

Uburu's initial policy:

1. Use only the Qt modules required by the desktop application.
2. Avoid GPL-only modules unless explicitly decided.
3. Prefer dynamic linking with Qt in redistributable builds.
4. Distribute required license notices and texts with artifacts.
5. Record the Qt version and packaged modules in each release.
6. Review LGPL/commercial obligations before publishing installers.

Official sources:

- https://doc.qt.io/qt-6/licensing.html
- https://www.qt.io/development/open-source-lgpl-obligations

## Release checklist

Before publishing a release:

- generate a list of packaged DLLs/libraries;
- record dependency versions and licenses;
- include license texts required by Qt and other dependencies;
- review whether any used Qt module is GPL-only;
- generate an SBOM when the release pipeline is available;
- validate whether the distribution mode can comply with LGPLv3 or requires a commercial license.
