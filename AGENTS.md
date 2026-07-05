# AGENTS.md

## Project vision

This project is an advanced desktop file-search application inspired by Agent Ransack, ripgrep, and IDE search, with special focus on software repositories, Git versioning, incremental indexing, and high performance.

The goal is not only to build an MVP. The goal is to build, over time, the best possible local search system for codebases and complex directories.

The application should let users choose a directory/repository, search by text, regex, file names, extensions, symbols, and content, and receive results quickly with contextual preview, occurrence highlighting, and intelligent integration with the current Git state.

## Fundamental priorities

The absolute priority is efficiency.

Project priorities, in order:

1. Correct results.
2. Perceived speed for the user.
3. Low memory usage.
4. Clean and extensible architecture.
5. Refined user experience.
6. Portability.
7. Maintainability.

Do not sacrifice architecture to deliver quickly. Prefer a solid, modular, testable base.

## Main stack

Prefer:

- Language: C++23.
- Interface: Qt 6 with QML/Qt Quick.
- Build system: CMake.
- Tests: Catch2 or GoogleTest.
- Local database: SQLite.
- Regex: PCRE2 with JIT when available.
- Git integration: libgit2 when appropriate; Git CLI may be used only as fallback or in isolated points.
- File monitoring:
  - Windows: `ReadDirectoryChangesW`.
  - Linux: `inotify`.
  - macOS: `FSEvents`.
  - Qt `QFileSystemWatcher` may be used initially, but the design must allow more efficient native backends.
- Packaging:
  - Windows: MSIX/traditional installer.
  - Linux: AppImage/Flatpak.
  - macOS: `.app` bundle.

The interface must be separated from the core. The search engine must not depend on QML.

## Architectural direction

The application must be divided into clear layers:

```txt
UI/QML
  ↓
Application Controller
  ↓
Search Service
  ↓
Search Engine / Index Engine / Git Service / File System Service
  ↓
Platform Adapters
```

The search core must be usable without a graphical interface. It must be testable by command line or automated tests.

Suggested structure:

```txt
/
  AGENTS.md
  CMakeLists.txt
  README.md
  docs/
    architecture.md
    indexing.md
    git-awareness.md
    performance.md
    search-semantics.md
  apps/
    desktop/
      CMakeLists.txt
      src/
        main.cpp
        qml/
        ui/
  src/
    core/
      search/
      index/
      filesystem/
      git/
      text/
      platform/
      storage/
      diagnostics/
    app/
      controllers/
      services/
    shared/
      types/
      utils/
  tests/
    unit/
    integration/
    fixtures/
  benchmarks/
```

## Main modules

### `core/search`

Responsible for direct non-indexed search, textual matching, regex, file-name search, and filter composition.

Must support:

- literal search;
- case-sensitive and case-insensitive search;
- regex search;
- whole-word search;
- extension filtering;
- glob filtering;
- included/excluded directory filtering;
- hidden-file search when enabled;
- search respecting `.gitignore` when enabled;
- cancellation;
- progressive results;
- configurable result limit;
- file-size limit;
- binary-file detection.

### `core/index`

Responsible for persistent indexing.

Indexing must be incremental, Git-aware, and content-addressed whenever possible.

Do not treat the index only as a list of paths. The same path may have different content in different branches.

The index must consider:

- `repositoryId`;
- `worktreeId`;
- absolute worktree path;
- relative file path;
- size;
- mtime;
- content hash;
- Git blob hash when available;
- current branch;
- current HEAD;
- file status;
- deleted files;
- ignored files;
- untracked files;
- locally modified files.

Conceptual model:

```txt
Repository
  └── Worktree
        ├── Current HEAD
        ├── Current branch
        ├── Indexed file catalog
        ├── Content-addressed documents
        └── Working tree overlay
```

Search must represent the real visible working tree state, not only the last commit.

### `core/git`

Responsible for detecting and interpreting Git state.

Must support:

- detecting whether a folder is a Git repository;
- detecting the `.git` directory;
- detecting worktrees;
- detecting current branch;
- detecting current HEAD;
- detecting branch changes;
- detecting changes in `.git/HEAD`;
- detecting changes in `.git/index`;
- identifying tracked files;
- identifying untracked files;
- identifying ignored files;
- identifying modified files;
- obtaining blob hashes for tracked files when possible;
- correctly handling detached HEAD;
- correctly handling submodules;
- correctly handling multiple worktrees.

Branch switch should be treated as an important structural change, but it should not necessarily rebuild the whole index. Whenever possible, reuse already indexed documents by content hash or blob hash.

Search must reflect the current worktree state, including uncommitted local changes.

### `core/filesystem`

Responsible for scanning and monitoring the filesystem.

Must support:

- efficient recursive scan;
- directory filters;
- extension filters;
- binary detection;
- encoding detection;
- chunked reading;
- parallel processing;
- prioritization of smaller or more likely files;
- backpressure to avoid overloading the UI;
- cooperative cancellation;
- native watchers per platform;
- path normalization;
- symlink handling;
- directory-cycle protection;
- configuration to follow or ignore symlinks.

### `core/text`

Responsible for text reading, normalization, and matching.

Must consider:

- UTF-8;
- UTF-16 LE/BE;
- Latin-1 when needed;
- files with BOM;
- `LF` and `CRLF` line endings;
- optional Unicode normalization;
- robust case-insensitive search;
- line extraction;
- before/after context extraction;
- occurrence highlighting;
- searching large files without loading everything into memory when possible.

### `core/storage`

Responsible for local persistence.

Use SQLite for:

- repository catalog;
- worktree catalog;
- file metadata;
- index state;
- search history;
- user preferences;
- per-repository settings;
- result cache when appropriate;
- performance diagnostics;
- indexing statistics.

Avoid depending exclusively on SQLite FTS5 as the definitive search-engine solution. It may be used as a component, but the architecture must allow backend replacement or evolution.

### `core/diagnostics`

Responsible for logging, metrics, and internal profiling.

Must measure:

- scan time;
- indexing time;
- time to first result;
- total search time;
- files processed per second;
- bytes processed per second;
- matching time;
- result rendering or UI-delivery time;
- approximate memory usage;
- number of ignored files;
- number of skipped binary files;
- number of reindexed files;
- number of documents reused by hash.

The application should have a diagnostics screen or mode to help development.

## Search model

The application must support two main modes.

### Direct search

Searches files directly in the current tree.

Use this mode when:

- the index does not exist yet;
- the user wants immediate results;
- the directory is small;
- the search is specific;
- the index is stale.

### Indexed search

Queries the persistent index.

Use this mode when:

- the repository has already been indexed;
- the user performs repeated searches;
- the file base is large;
- the user wants fast search by content, name, extension, symbols, or metadata.

The interface may combine both modes:

```txt
1. Show fast results from the index.
2. Continue validating/refining against the direct current state.
3. Update or remove stale results.
```

## Git and versioning

The index must be version-control aware.

Important concepts:

```txt
Repository:
  Logical Git repository.

Worktree:
  Currently opened physical directory.

Commit:
  Versioned state.

Branch:
  Pointer to a commit.

Blob:
  Versioned file content.

Working tree overlay:
  Uncommitted local changes, new files, modified files, and deleted files.
```

Never assume that `path` uniquely identifies a document's content.

Use a richer conceptual key:

```txt
repositoryId
worktreeId
relativePath
contentHash
gitBlobHash
```

When switching branches:

- detect HEAD change;
- detect changes in `.git/index`;
- perform incremental rescan;
- remove or hide files that no longer exist;
- reuse content already indexed by hash;
- reindex only new or modified files;
- preserve search history and preferences.

Search must reflect the current worktree state, including uncommitted local changes.

## User interface

The interface must be fast, clean, and productive.

The main screen should contain:

- main search field;
- directory/repository selector;
- search options:
  - literal text;
  - regex;
  - case-sensitive;
  - whole word;
  - include ignored;
  - include hidden;
  - include binaries;
  - respect `.gitignore`;
- extension filter;
- directory filter;
- size filter;
- result list;
- file preview;
- occurrence highlight;
- result count;
- time to first result;
- indexing status;
- cancel button;
- search history;
- favorites or saved searches.

The UI must receive results progressively. Do not wait for search to finish before showing results.

The application must never freeze the interface during search, indexing, or file reading.

## Performance

Mandatory rules:

- Never run heavy search work on the UI thread.
- Use cooperative cancellation.
- Use thread-safe result queues.
- Avoid loading huge files entirely into memory.
- Avoid regex when literal search is sufficient.
- Use PCRE2 JIT for regex when available.
- Batch results sent to the UI.
- Debounce the search field.
- Use cache with explicit invalidation.
- Avoid unnecessary full reindexing.
- Prefer incremental indexing.
- Measure before optimizing aggressively.

Time to first result is a first-class metric.

## Concurrency

The system must have a concurrency-safe design.

Use:

- worker threads;
- thread pool;
- task queue;
- cancellation tokens;
- result streaming;
- memory limits;
- depth limits;
- task priority.

Avoid:

- data races;
- long locks;
- direct UI updates from worker threads;
- excessive use of global mutexes;
- unnecessary shared state.

## Settings

Global settings:

- light/dark/system theme;
- language;
- maximum file size;
- default ignored directories;
- ignored extensions;
- include hidden files;
- respect `.gitignore`;
- number of threads;
- index database location;
- search result limit.

Per-repository settings:

- friendly name;
- path;
- ignore preferences;
- relevant extensions;
- saved searches;
- indexing state;
- last opened branch;
- last indexed HEAD.

## Internationalization

The interface must be prepared for i18n from the beginning.

Required initial language:

- `pt-BR`

Also prepare structure for:

- `en-US`

Do not write hardcoded visible user text directly in QML or C++. Centralize translatable messages.

Portuguese must use correct accents and formal standard usage.

## Code quality

Use modern C++.

Prefer:

- RAII;
- strong types;
- `std::filesystem`;
- `std::optional`;
- `std::variant` when appropriate;
- `std::string_view` carefully;
- `std::chrono`;
- `std::jthread` when available;
- clear separation between interfaces and implementations;
- explicit errors;
- automated tests.

Avoid:

- mutable global state;
- raw pointers without need;
- code coupled to UI;
- giant functions;
- god classes;
- obscure optimizations without benchmarks;
- silencing important errors;
- mixing search logic with rendering;
- magic numbers in domain logic, parsing, protocols, binary formats, limits, or algorithms;
- redundant snippets, unnecessary branches, or ceremonial code that can be expressed more directly without losing clarity.

When a numeric value has semantic meaning, declare a named constant before using it. In C++, prefer `constexpr` or `inline constexpr` with the smallest suitable scope, usually in the anonymous namespace of the `.cpp` when the value is internal to the translation unit. Use macros only when required by preprocessor, platform, or external-library integration.

Preferred examples:

```cpp
constexpr unsigned char utf8ContinuationTagMask = 0b1100'0000U;
constexpr char32_t maximumUnicodeScalar = 0x10FFFFU;
constexpr std::size_t defaultPreviewContextLines = 3;
```

The constant name must explain the value's meaning, not just repeat its representation.

Prefer direct expressions when they preserve semantics and are more readable than a sequence of branches. When simplifying booleans, verify logical equivalence, especially when two or more conditions can be active at the same time. Do not switch `&&` to `||`, or apply De Morgan mechanically, without validating behavior with existing or new tests.

Preferred example:

```cpp
return (!options.wholeWord || hasWordBoundary(match)) &&
       (!options.wholeIdentifier || hasIdentifierBoundary(match));
```

For human readability, visually separate declarations and control blocks:

- After a variable declaration without assignment, insert a blank line before the next instruction, except when the next line is another related declaration.
- After control blocks such as `if`, `else`, `for`, `while`, `switch`, and auxiliary scoped blocks, insert a blank line before the next logical instruction.
- After a `return`, insert a blank line only when there is following code in the same scope. Do not add an artificial blank line between a final `return` and the block/function `}`.
- Avoid multiline ternaries with compound conditions inside initializers or aggregate `return` values when alignment becomes confusing. Prefer extracting the condition to a semantic function/variable. When a multiline ternary is kept, do not align `?` and `:` with many spaces up to the condition; advance only one visual level relative to the main expression.
- When repeatedly comparing the same variable, enum, status, pointer, or similar conceptual value, put each comparison on its own line to ease reading, review, and future diffs.

Example:

```cpp
std::string message;

if (enabled) {
  message = "ready";
}

return message;
```

## Tests

Create tests for:

- literal search;
- case-insensitive search;
- regex search;
- whole-word search;
- UTF-8 files;
- CRLF files;
- binary skipping;
- search respecting `.gitignore`;
- recursive scanner;
- symlinks;
- cancellation;
- incremental indexing;
- branch switch;
- deleted files;
- modified files;
- untracked files;
- worktrees;
- hash reuse;
- result ordering;
- occurrence highlighting;
- context extraction.

Use small, deterministic fixtures.

## Benchmarks

Create benchmarks for:

- scanning many small files;
- scanning few large files;
- literal search;
- regex search;
- time to first result;
- initial indexing;
- incremental reindexing;
- branch switch;
- document reuse by hash;
- UI result loading.

Benchmarks should be easy to run and compare.

## Documentation

Keep documentation in `docs/`.

Expected files:

```txt
docs/architecture.md
docs/indexing.md
docs/git-awareness.md
docs/search-semantics.md
docs/performance.md
docs/storage.md
docs/ui.md
docs/build.md
docs/licenses.md
```

Whenever an important architectural decision is made, record it in documentation.

## Development style

### C++ formatting

All new or modified C++ code in `apps/`, `src/`, and `tests/` must fully respect the root `.clang-format`, configured for C++23. Before finishing a change:

- Use 2-space indentation, never tabs.
- Use 2-space continuation indentation; lists, initializers, broken calls, and multiline expressions must not gain a visual 4-space indentation.
- Keep the 120-column limit.
- Use `Type* name` and `Type& name` for pointers and references.
- Preserve case-sensitive include ordering.
- Format with `cmake --build <build-directory> --target format` or run `clang-format -i` only on project-owned C++ files you changed.
- Do not format vendored dependencies or automatically generated files.

For function signatures, calls, and declarations with parameters:

- Do not break lines if all parameters still fit comfortably within the 120-column limit.
- When a break is necessary because the line exceeds 120 columns, put each parameter on its own line, aligned as a block. Do not keep the first parameter on the function line merely to use more horizontal space.
- Avoid breaks caused only by excessive automatic alignment when the line is still readable.

The `format` target is auxiliary and must never be required to build the project.

### C++ identifier names

Every new or modified project-owned C++ identifier must use `camelCase`, aligned with Qt convention. This includes functions, methods, local variables, struct/class members, internal constants, and project-owned enumerators.

Preferred examples:

```cpp
constexpr std::size_t maximumLineLength = 1024U * 1024U;
auto searchRoot = query.root;
query.options.caseSensitive = true;
```

Classes, structs, enums, and type aliases remain in `PascalCase`, such as `SearchQuery`, `SearchResult`, `GitService`, and `IndexDocument`.

Files remain in `kebab-case`; do not use `camelCase`, `PascalCase`, or `snake_case` in the filesystem. External APIs preserve their original convention, such as `std::string_view`, `std::stop_token`, `std::filesystem::directory_options::follow_directory_symlink`, system macros, Qt, PCRE2, libgit2, and names required by libraries.

### File names

Every new or renamed project-owned file must use `kebab-case`, with only lowercase letters, numbers, and hyphens between words. Valid examples: `search-engine.hpp`, `direct-search-engine-tests.cpp`, and `git-awareness.md`.

The only exceptions are canonical names required or widely conventioned by tools: `CMakeLists.txt`, `README.md`, `AGENTS.md`, `TODO.md`, `.clang-format`, `.gitignore`, and manifests such as `vcpkg.json`. Generated files and vendored dependencies also keep their original names.

When renaming a file, update in the same work all includes, CMake targets, resources, documentation, and tests that reference it. Do not introduce new `snake_case`, `PascalCase`, or `camelCase` file names.

When implementing something:

1. Understand the existing architecture.
2. Do not break the separation between core and UI.
3. Prefer small interfaces.
4. Write tests for central behavior.
5. Document relevant decisions.
6. Measure when the change involves performance.
7. Preserve cross-platform compatibility.
8. Avoid shortcuts that make advanced indexing harder in the future.

## Definition of quality

A feature is good only when:

- it works correctly;
- it has error handling;
- it does not block the UI;
- it is testable;
- it is extensible;
- it does not worsen the architecture;
- it does not assume Windows only;
- it does not ignore Git/worktree when relevant;
- it does not depend on accidental behavior.

## Do not do

Do not create an Electron application.

Do not create the search core in JavaScript.

Do not couple search to QML.

Do not depend exclusively on external commands such as `grep`, `find`, or `rg`.

Do not index only by path.

Do not ignore branch, HEAD, worktree, and local Git state.

Do not rebuild the whole index unnecessarily.

Do not block the UI during long operations.

Do not add visible user text without going through i18n.

Do not accept code without tests for critical core parts.

Do not treat performance as a secondary detail.

## Product philosophy

This project should be treated as a serious tool for developers and advanced users.

The system should be fast enough for daily use, reliable enough to replace manual searches, and intelligent enough to understand versioned repositories.

The long-term goal is an extremely efficient local search application with excellent user experience, capable of handling large codebases, multiple branches, worktrees, modified files, and incremental indexing without losing precision.
