# User guide

[Português do Brasil](user-guide.pt-BR.md)

Uburu is a local file-search application for people who need to find information quickly in folders, repositories, notes, documents, and mixed file collections. It searches on your machine, shows results progressively, and keeps the interface responsive while work is running.

## Main screen

The main screen has four central areas:

- the search header, where you type the query and choose search options;
- the scope field, where you choose the folder or repository to search;
- the results list, where matching files and occurrences appear;
- the preview panel, where Uburu shows the selected occurrence in context.

The footer shows the current status, search/indexing progress, partial errors, and indexing information.

## Choosing where to search

Use the scope field to choose the folder or repository that Uburu should search. You can type a path manually, press Enter, or use the folder icon to select a directory through the system picker.

The scope dropdown shows favorite and recent paths. Selecting a path changes the current scope. The star button toggles whether that path is a favorite.

Uburu can also include or ignore subdirectories inside the current scope. When you add included or ignored folders, the scope field summarizes them with `+` and `-`, for example:

```text
C:\Users\you\Documents\Project (+C:\Users\you\Documents\Project\important,-C:\Users\you\Documents\Project\node_modules)
```

An included folder narrows or extends the searched area according to the selected scope rules. An ignored folder is skipped even when it is inside the selected scope.

## Running a search

Type the text you want to find in the search field and press Enter or click Search. Results appear progressively; you do not need to wait for the entire folder to finish scanning.

Uburu currently searches both file names and file contents by default. This means a file may appear because its name matches the query, because its content contains the query, or both.

Use Cancel or Esc to stop a running search. Already published results remain visible while the cancellation is processed.

## Search options

Regex enables regular-expression search. Use it for patterns rather than plain phrases. If the pattern is invalid or too expensive, Uburu reports the error instead of freezing the application.

Case-sensitive makes uppercase and lowercase different.

Whole word returns only isolated word occurrences instead of matching fragments inside larger words.

Respect `.gitignore` skips files and directories ignored by Git rules.

Include hidden allows hidden files and folders to be searched.

Include binaries allows binary files to be considered. Content search still depends on whether the file can safely be read or extracted as text.

Include subdirectories controls whether Uburu searches below the selected folder.

## Type filter

The Types field limits the search to specific extensions. You can type values such as:

```text
pdf, docx, txt
```

Leave it empty to search all supported file types. The filter applies to file extensions, not to content classification.

## Results and preview

The results list shows matching occurrences. Selecting a result opens a bounded preview around the match. Uburu scrolls the preview near the selected occurrence and highlights matching text when possible.

You can use the file action menu to open the file, open its location, copy its path, or copy the selected occurrence. Double click or Enter opens the selected result with the system's default application.

## Indexing

Uburu can index a selected scope so repeated searches become faster. Indexing runs outside the UI thread and reports progress in the footer. If an index is stale, missing, or being updated, the status area communicates that state.

For Git repositories, Uburu tracks repository/worktree identity, branch and HEAD state, local modifications, deleted files, ignored files, and reusable document content. The goal is to reflect the files you can actually see in the working tree, not only a previous commit.

## Supported content formats

Plain text files are searched directly. Uburu also extracts searchable text from several rich formats with safety limits:

- PDF;
- DOCX, XLSX, and PPTX;
- ODT, ODS, and ODP;
- RTF;
- HTML and XHTML;
- SRT and VTT subtitle files.

Unsupported, protected, unsafe, binary, or too-large files may still be searchable by file name even when their content cannot be searched. Uburu distinguishes these cases in indexing/search status whenever possible.

## Privacy

Uburu is designed as a local tool. Searches, indexes, history, and settings stay on your machine unless you explicitly export diagnostics or files. Telemetry is not enabled by default.

Paths and content can be sensitive. Diagnostic exports should be reviewed before sharing.

## Shortcuts

- `Ctrl+F`: focus the search field;
- `Ctrl+O`: choose a folder;
- `Enter`: run search or open the selected result depending on focus;
- `Esc`: cancel a running search or close transient UI;
- `F4`: go to the next occurrence;
- `Shift+F4`: go to the previous occurrence;
- `Ctrl+C`: copy the selected result path when the result list is focused;
- `Ctrl+Shift+C`: copy the selected occurrence;
- `Ctrl+K` or `Ctrl+Shift+P`: open the command palette.

## Common issues

If a search returns fewer results than expected, check the scope, the Types filter, `.gitignore` handling, hidden-file handling, and whether subdirectories are enabled.

If a rich document appears by name but not by content, the file may be protected, malformed, unsupported, too large, or blocked by safety limits.

If results include files outside the expected folder, clear the scope and select it again. The active scope shown in the scope field is the source of truth for the next search.

If the app appears busy, watch the footer. Search and indexing can run for a while on large directories, but the interface should remain responsive and cancellable.
