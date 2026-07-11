# Interface

The Qt Quick UI is a client of the `app` layer. It contains only presentation and interaction state. `SearchController` runs the service outside the graphics thread, receives progressive occurrences through queued connections, and exposes a model to QML.

Visible text uses `qsTr` or `tr`, with `pt-BR` and `en-US` catalogs. Evolution should add result batching, contextual preview with highlight, advanced filters, history, index status, and diagnostics without moving rules into QML.

## Milestone 8 visual direction

The main screen should communicate speed, agility, and efficiency without looking generic. The first Milestone 8 iteration adopts a technical command composition: a compact top area for search and filters, status cards with instrumentation language, a streaming result list, and a side preview.

The initial dark theme uses controlled contrast, blue as the action color, and green as a punctual confirmation accent to suggest telemetry, low latency, and modern technical tools without making the UI excessively flashy. Empty states should be clear and instructive, explaining the next step without blocking progressive search.

The layout switches to vertical orientation at smaller widths to keep the search area prioritized and preserve comfortable reading of the list and preview.

The main screen must remain a high-level composition. Reusable QML components live in `apps/desktop/qml/components/`, preferably as small files with a single responsibility so `main.qml` does not concentrate all Milestone 8 evolution.

## Visible filters

The header exposes only filters with real behavior connected to the core or controller. At this stage, visible filters cover regex, case sensitivity, whole word, respecting `.gitignore`, including hidden files, including binaries, including subdirectories, and document types by extension. Options such as search target and maximum size keep internal defaults until the future settings screen, avoiding overly advanced controls on the main screen.

## Multiple scopes

The visual scope selector allows accumulating multiple search roots. Selecting a folder through the dialog, favorites, or recent entries adds the root to the current set; chips under `Selected` show active roots and allow removing them without deleting favorites or history. `SearchController` builds a `SearchScope` with those roots before calling the search service, preserving `SearchQuery::root` only as compatibility for single-root calls.

Subdirectory inclusions and exclusions are associated with the most specific selected root containing the chosen folder. The UI shows inclusions under `Included` and exclusions under `Ignored`, always as removable chips. The controller converts inclusions to `SearchRoot::includedDirectories` and exclusions to `SearchRoot::excludedDirectories`.

When a root has explicit inclusions, search in that root is restricted to the included subtrees. Exclusions still remove specific subtrees from the final scope. This enables scenarios such as searching several repositories at once, including only relevant modules, and ignoring large folders like `node_modules`, `build`, or local caches without turning those filters into global rules.

## Search metrics on the main screen

The search header should expose lightweight operational metrics to reinforce the perception of speed without turning the screen into a diagnostics panel. Visible Milestone 8 metrics are:

- visible results;
- files read;
- time to first result;
- total search duration.

These values are computed by the controller from the summary returned by `SearchService`. QML only presents observable properties; it must not measure search, infer core progress, or access worker threads directly.

The footer has a fixed spot for indexing state. In the current desktop integration, search uses the direct engine and the status appears as inactive indexing; when `IndexingService` is connected to the window, the same spot should display real progress without blocking direct search.

## Result list

The result list uses `ListView` with delegate reuse and limited visual cache. This avoids instantiating QML components for every result when there are many items, keeping rendering proportional to the visible area and a small navigation margin.

The C++ model still retains results published by search. Future memory optimizations should evolve the model contract, not replace visual virtualization with manual QML logic.

During progressive result publication, the list preserves the current selection when new items are appended. If search has no selection yet and the first result arrives, the UI selects the first occurrence automatically to load preview without requiring an extra click. Linear navigation between visible occurrences uses `F4` to move forward and `Shift+F4` to move backward.

Consecutive results from the same file are visually grouped. The C++ model exposes specific roles for group start and file label, keeping QML focused on rendering and avoiding manual grouping by inspecting neighboring items.

## File preview

The main-screen preview is loaded by `SearchController` in an asynchronous worker, using the core text reader. When another result is selected, the previous preview receives cooperative cancellation and late events are discarded by the active watcher.

The preview is limited by a line window around the occurrence and by a byte budget to keep the UI responsive. QML displays only observable state: selected file, location, preview text, and loading indicator.

The controller also delivers a safe HTML preview when there is a selected occurrence. This representation escapes file content, highlights all known occurrences on the active line, and keeps line numbers aligned in a monospace font. `PreviewPane` preserves a tab-width property for future exposure in preferences without rewriting the component.

## Interactions with found files

Results should allow direct operations on the found file without breaking the separation between UI and platform. The desired Milestone 8 behavior is an Uburu-owned menu with actions equivalent to common file manager operations, not a full reproduction of the Explorer/Finder/Linux desktop native menu.

The result-list context menu forwards intentions to `SearchController`, keeping QML free of platform logic. Initial actions are open file, open with when the platform offers an application picker, open file location, copy path, and copy occurrence. On Windows, `Open with...` uses the operating-system picker. On other platforms, it should evolve through Finder, portal, or desktop-environment adapters.

Visually, action menus use the same vocabulary as the main screen: elevated surface, subtle border, soft blue highlight on the active item, and shortcuts aligned to the right. The goal is to feel integrated with the product without fully imitating the platform's native menu. This pattern is shared by the result-list file menu and the preview text menu.

In the initial iteration, open file uses the default application configured in the operating system. The list also accepts double click or Enter to open the selected result, Ctrl+C to copy the absolute path, and Ctrl+Shift+C to copy the occurrence with location and snippet.

## Shortcuts and command palette

The main screen exposes an initial command palette through `Ctrl+K`, `Ctrl+Shift+P`, and a compact `Commands` button in the header. The palette is a QML coordination component: it lists available commands and emits the user's choice, while actions remain delegated to existing components and `SearchController`.

Essential shortcuts available at this stage:

- `Ctrl+F`: focus the search field;
- `Ctrl+O`: select directory or repository;
- `Ctrl+K` / `Ctrl+Shift+P`: open command palette;
- `Ctrl+D`: toggle favorite for the current directory;
- `F4`: select next visible occurrence;
- `Shift+F4`: select previous visible occurrence;
- `Esc`: cancel running search;
- `Enter`: run search or open selected result depending on focus;
- `Ctrl+C`: copy result path when the list is focused;
- `Ctrl+Shift+C`: copy result occurrence when the list is focused.

The palette includes diagnostics, history, saved searches, and occurrence navigation actions without moving domain rules to QML. The diagnostics action copies status, counters, and observable times from the current search to the clipboard, serving as a simple bridge until a dedicated diagnostics screen exists.

## History and saved searches

The main screen keeps local history of the last executed queries and a list of searches manually saved by the user. Both are persisted in QML `Settings` as experience state, not as core search data. The query is normalized by `trim`, moved to the top when reused, and limited to a small number of entries to avoid unbounded growth.

The header shows compact chips for the most important saved and recent searches. Selecting a chip loads the query and runs search when there is a valid scope. The current search can be saved or removed by a header button, `Ctrl+S`, or the command palette.

## Visual cancellation

Search cancellation must respond immediately to the user's action. When `Esc` or the cancel button is used, `SearchController` enters the `cancelling` state, updates status to `Cancelling...`, and disables additional cancellation attempts until the worker confirms shutdown.

While `cancelling` is active, `running` remains true to prevent a concurrent search on the same controller. Visual state should indicate that the request was accepted, but the already published result list remains available until search finishes or a new search clears the model.

## Themes

Milestone 8 introduces `system`, `dark`, and `light` theme infrastructure. The mode is persisted in QML `Settings` and applied by the `Theme` singleton, keeping components decoupled from local palettes and reducing visual inconsistencies.

`system` follows the operating system color preference when available. A dedicated theme control should live in the future settings area, not in the main search header or command palette.

The content preview area remains on a dark surface even in the light theme. This preserves contrast for the highlight HTML generated by the controller and avoids alternating code colors in two layers while preview rendering is still evolving.

## Settings menu

Milestone 11 connects the compact top-left menu to a real settings dialog. The settings entry point follows a desktop-style menu bar instead of adding more controls to the search header. The dialog currently exposes general preferences, language preference, privacy/diagnostics actions, theme selection, and layout reset. Theme changes apply immediately through the `Theme` singleton. Language preference is persisted in QML `Settings` and read during application startup, so changing it requires restarting Uburu before the translator is loaded with the selected locale.

## Window state persistence

The visual state of the main window is persisted in QML `Settings`, because it belongs to the presentation layer. At this stage, the application restores window geometry, preferred result-pane size, and visual search filters. The textual query itself is not restored automatically to avoid rerunning an old search when the application opens.

The last selected directory is restored by `SearchController`, together with directory history and favorites already persisted through `QSettings`. The controller restores a recent directory only when it still exists in the filesystem.

## Indexing status

The main-screen footer reserves a permanent area for indexing state. In Milestone 8, this area receives real progress from `IndexingService`, triggered by the command palette with `Reindex scope` (`Ctrl+Alt+I`) and cancellable with `Cancel indexing` (`Ctrl+Alt+Esc`).

Reindexing runs outside the graphics thread. `SearchController` creates the indexing service in a worker, discovers each selected root's Git worktree when one exists, runs `requestManualReindex()`, and forwards `IndexUpdateProgress` events through queued connection. The UI shows textual status, a compact progress bar, and a final summary with indexed, reused, removed, and failed documents.

Roots that are not Git repositories yet are indexed as regular directories, with synthetic identity based on the normalized path and no Git overlay. This preserves support for standalone directories without diluting Git-aware treatment for real repositories.

## Contextual help

Potentially ambiguous controls should expose short localized help through tooltips, without turning the main screen into long-form documentation. Milestone 8 uses `InfoIcon` for scope and document-type explanations, plus direct tooltips on filter chips such as regex, case-sensitive, whole word, respect `.gitignore`, and include subdirectories.

These messages are user-visible text and must continue to go through `qsTr`/translation catalogs.

The regex chip is enabled only when the build exposes PCRE2 support through `SearchController`. Even so, the core remains the final authority and validates `SearchQuery` to prevent regex in builds without a compatible backend.

## Initial accessibility

Reusable interactive components should expose accessible names consistent with visible text and documented shortcuts. Buttons, filter chips, menus, input fields, result list, command palette, and preview have localized `Accessible.name` or `Accessible.description` values to reduce dependence on visual inference.

Milestone 8 considers the main screen's accessibility base validated when:

- the search field is reachable through `Ctrl+F` and keeps predictable focus after search execution;
- folder selector, search/cancel buttons, filters, scope chips, favorites, recent entries, results, preview, menus, and command palette have accessible names or descriptions;
- keyboard navigation covers the main flows: open folder, search, cancel, open palette, navigate results, open result actions, and copy information;
- empty states, partial errors, cancellation, and inactive indexing are exposed through visible text, not only color;
- visual contrast on the main screen is sufficient for comfortable reading in the initial dark theme.

Full audits with real screen readers and automated contrast checks enter Milestone 9 as continuous quality tests. Milestone 8 leaves the structure ready and documents the manual checklist to prevent obvious regressions.

## High DPI, monitors, and responsiveness

The main screen must remain usable in reduced windows, high DPI, and fractional scales. Milestone 8 uses `Layout`, `Flow`, `SplitView`, elided text, virtualized lists, and scrollable sections to prevent compact controls from pushing results and preview out of the window.

Recommended manual checklist before releases:

- open the window at reduced size and confirm that header, scope, results, preview, and status remain accessible;
- validate 100%, 125%, 150%, and 200% scales on Windows;
- move the window between monitors with different scales and confirm that fonts, borders, menus, and tooltips remain proportional;
- confirm that long file and directory names show the relevant path end without breaking layout;
- confirm that history, favorites, inclusions, and exclusions use scrolling or menus when they do not fit horizontally;
- confirm that results and preview remain usable when the layout switches between horizontal and vertical orientation.

## Pluralization, shortcuts, and technical strings

Texts with variable counts should use Qt native pluralization with `%n` whenever the sentence depends on item count. Avoid building plurals in QML by concatenating suffixes such as `s`, because that breaks future languages and makes `pt-BR` artificial. Numeric placeholders that do not alter grammar may continue to use `%1`, such as compact metric cards.

Keyboard shortcuts should be treated as translatable strings only when they appear in the interface, but the actual sequence must remain centralized in the `Shortcut` or component that executes the action. When creating a new action, update together: command palette, tooltip/menu when present, UI docs, and `pt-BR`/`en-US` catalogs.

Technical strings widely recognized by advanced users, such as `Regex`, `.gitignore`, `Ctrl`, `Shift`, `PCRE2`, `HEAD`, and format names like `PDF` or `DOCX`, should remain stable. The surrounding sentence should be localized and explain the practical impact clearly.

## Partial search errors

Isolated read, permission, or file-removed failures during search must not interrupt delivery of valid results. When the core marks `SearchSummary::partialFailure`, the UI preserves the occurrence list and turns errors into a warning in the search status, showing the first error occurrence as short context.

Errors that prevent the whole search, such as query validation or unavailable backend, remain displayed as final errors. Explicit user cancellation has visual priority over partial warnings to avoid ambiguous feedback.

## Bottom progress indicators

The footer exposes activity without blocking the main panes. Search shows an indeterminate progress rail because direct traversal cannot always know the final file count while discovery is still running. Indexing shows a determinate rail whenever `IndexUpdateProgress::total` is known, and falls back to an indeterminate rail while preparing, scanning, or waiting for a total. Cancellation uses the warning color so the user can distinguish active work from shutdown.

## Document format content extraction

Current direct search treats plain-text files and supported document formats as searchable content. The current extractor layer covers DOCX, XLSX, PPTX, ODT, ODS, ODP, RTF, HTML, subtitles, and initial bounded PDF text extraction. Binary, legacy, encrypted, malformed, or unsupported packaged formats can still be found by file name when the target combines content and file name.

The UI should make remaining limitations clear only when the user filters by types without a current extractor, such as legacy Office, EPUB, or email containers. The filter selects files by extension, but searching inside unsupported content depends on future extractors below the UI, with memory limits, cooperative cancellation, and hostile-file handling.
