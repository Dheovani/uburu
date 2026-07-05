# Fixtures

Fixtures must remain small, deterministic, and explicitly cover UTF-8, UTF-16, CRLF, binary files, symlinks, and Git repositories with worktrees.

Use `test-fixtures.hpp` for small data shared by unit and integration tests. The intent is to name recurring cases semantically without hiding the file structure:

- precomposed and decomposed Unicode text;
- UTF-8 with BOM, UTF-16 LE/BE, Latin-1, and binary bytes;
- minimal `.gitignore` layouts;
- basic files for a disposable Git worktree.

Avoid large fixtures or fixtures that depend on the local environment. When a test needs to create real files, combine these fixtures with the RAII helpers in `tests/helpers`.
