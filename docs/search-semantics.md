# Search semantics

This document defines the observable contract of Uburu's direct search. Indexed search must preserve the same semantics so results do not change when the source moves from direct reading to a persistent index.

## Result unit

A result represents one occurrence, not just a line containing the expression.

Each occurrence must contain:

- path relative to the searched directory;
- 1-based line;
- 1-based visual column;
- occurrence length in UTF-8 bytes;
- line text for preview;
- highlight spans for the line;
- previous/following context when configured.

If a line contains more than one occurrence, each occurrence is published as a separate `SearchResult`.

## Search scope

`SearchQuery` has a `SearchScope` with zero or more `SearchRoot` values. Each root represents a physical searchable root and may have its own included/excluded directories. This allows searching multiple repositories or standalone directories at the same time and ignoring specific subtrees, such as `node_modules`, per root.

`SearchQuery::root` remains as temporary compatibility for older single-root calls. Direct search resolves effective roots as follows:

1. if `SearchScope::roots` is not empty, use those roots;
2. otherwise, use `SearchQuery::root` with global `SearchOptions` filters.

`SearchResult::path` remains relative to the root that produced the result. To disambiguate results with the same relative path in different roots, `SearchResult::searchRoot` carries the physical source root.

## Query validation

Search validates `SearchQuery` before starting filesystem traversal. Invalid queries must not call the scanner or publish results.

Validation errors are returned as typed codes in `SearchSummary::errors`, so UI and services can translate messages without coupling user-visible text to the core.

Initial supported errors:

- `emptyRoot`;
- `rootNotFound`;
- `rootNotDirectory`;
- `rootUnavailable`;
- `emptyExpression`;
- `unsupportedSearchMode`;
- `regexCompileFailed`;
- `regexResourceLimitExceeded`;
- `regexTimeout`;
- `invalidRegexLimit`;
- `invalidResultLimit`;
- `invalidPerFileResultLimit`;
- `invalidMaximumFileSize`;
- `fileOpenFailed`;
- `fileReadFailed`.

`rootNotFound` means the configured root does not exist. `rootNotDirectory` means the path exists but is not a directory. `rootUnavailable` means the root exists or was requested, but the operating system returned an access or availability error while checking or opening it. This covers permission-denied directories and also gives removable media or unstable network locations a distinct recoverable error instead of misreporting them as missing roots.

Regex mode is reported as `unsupportedSearchMode` when the build does not include PCRE2. When PCRE2 is available, compilation errors return `regexCompileFailed` with the backend-provided message and offset.

## Literal search

Literal search interprets the expression as plain text. Characters with special meaning in regex have no special meaning in this mode.

By default, literal search is case-insensitive. When `caseSensitive` is enabled, UTF-8 code points in the line and expression must match exactly.

Case-insensitive comparison uses simple Unicode case folding, limited to one-code-point to one-code-point transformations. This covers ASCII and common precomposed Latin letters, such as `AÇÃO` against `ação` and `CAFÉ` against `café`.

The matcher works over UTF-8 text normalized by the text reader. Internal match offsets and lengths remain in UTF-8 bytes; `SearchResult::column` and `MatchSpan::column` are 1-based visual columns computed by UTF-8 code point.

## Overlapping occurrences

Overlapping occurrences are preserved in literal search.

Example:

```txt
text:       aaaa
expression: aa
columns:    1, 2, 3
```

This behavior avoids hiding real matches and keeps the contract predictable for future highlight, ranking, and index strategies.

## Whole word

`wholeWord` uses word boundaries for natural text. ASCII letters, ASCII digits, and precomposed Latin letters are considered part of a word. Punctuation and `_` are word boundaries in this mode.

Examples:

- `ação` matches `pré-ação`;
- `ação` does not match `préação`;
- `search` matches `search_engine`, because `_` is punctuation for natural text.

For source code, `wholeIdentifier` uses identifier boundaries. ASCII letters, ASCII digits, and `_` are considered part of the identifier.

Examples:

- `search` does not match `search_engine`;
- `search` does not match `searchEngine`;
- `search` does not match `search2`;
- `search` matches `call(search)`.

If `wholeWord` and `wholeIdentifier` are enabled at the same time, the occurrence must satisfy both boundary rules.

## Regex

When `SearchOptions::mode` is `SearchMode::regex`, the expression is compiled once per search with PCRE2 in UTF/UCP mode. The search reuses the compiled regex across all processed lines, avoiding recompilation inside the file loop.

Regex also respects `caseSensitive` by default: when disabled, the pattern is compiled with `PCRE2_CASELESS`.

The matcher tries to enable PCRE2 JIT with `PCRE2_JIT_COMPLETE`. When JIT is accepted, the search summary records `RegexExecutionMode::jit`. When PCRE2 is available but JIT is not accepted for the pattern or current build, the search continues with interpreted fallback and records `RegexExecutionMode::interpretedFallback`.

Regex has configurable limits in `SearchOptions`:

- `regexMatchLimit`;
- `regexDepthLimit`;
- `regexHeapLimitKib`;
- `regexTimeout`.

The first three are applied in the `pcre2_match_context`. Timeout uses PCRE2 automatic callouts and interrupts the match attempt when the time budget expires. When a limit is reached, search stops and returns a typed error, distinguishing `regexResourceLimitExceeded` from `regexTimeout`.

Regex preserves the same result unit as literal search: each occurrence becomes an individual result, with offset and length in UTF-8 bytes. `wholeWord` and `wholeIdentifier` are also applied to regex matches.

Compilation errors return `regexCompileFailed` with:

- `translationKey`, so the UI can translate the visible message;
- `context`, with the technical message provided by the backend;
- `offset`, when PCRE2 reports the error position in the pattern.

## Search target

`SearchOptions::target` defines where the expression is applied:

- `content`: search only file content;
- `fileName`: search only the relative file path;
- `contentAndFileName`: publish occurrences from both the relative path and content.

File-name results use `SearchResultKind::fileName`, line `0`, and a 1-based column inside the relative path. Content results use `SearchResultKind::content` and 1-based line.

File-name search does not open the file, allowing paths to be found even when content is not accessible at that moment. Regex, case sensitivity, and whole-word rules also apply to the relative path.

## Document extraction

Indexed content search uses the `core/document` extraction boundary before storing searchable text. Plain text files are indexed line by line through the text reader. HTML, HTM, and XHTML files are indexed as visible text: tags are stripped, common entities are decoded, block-level tags create text boundaries, and script, style, and comment contents are excluded by default. RTF files are indexed as extracted visible text with common escapes decoded, embedded image/object destinations skipped, and explicit safety limits for group nesting, control words, and binary payloads. This keeps searches from matching implementation details that are not visible document content.

Subtitle files are treated as structured text documents. SRT and WebVTT cue text is indexed without sequence numbers, timing arrows, WebVTT headers, or note blocks. Cue locations use `DocumentLocationKind::timestamp` with the start time in milliseconds, preparing search results and preview for future time-aware navigation.

When content extraction is unavailable, unsupported, unsafe, or temporarily limited, the file remains searchable by name if it was scanned. In that case the indexed document has no stored content text, so `content` search does not return matches from raw container bytes, compressed XML packages, scripts, binary payloads, or parser artifacts.

## File filters

The scanner applies filters before delivering `FileEntry` to the search engine:

- maximum size;
- allowed extensions;
- included directories;
- excluded directories;
- included globs;
- excluded globs;
- hidden files according to `includeHidden`.

Exclusions take precedence over inclusions. A file inside an included directory is still ignored if it also falls under an excluded directory or excluded glob.

Extensions are compared without the leading dot. On Windows, extension and glob comparison is case-insensitive; on other platforms, comparison preserves the system's case sensitivity.

Initial globs support `*` and `?` over the normalized relative path with `/`. They are a simple Milestone 1 filter semantics, not a complete `.gitignore` implementation.

## Result limits

`resultLimit` is a global search limit. A result may be published only while still within the limit.

When the limit is reached:

- no additional result should be emitted;
- `SearchSummary::limitReached` must be `true`;
- `SearchSummary::matches` must count only published results.

`perFileResultLimit` limits the number of results published for a single file. When the per-file limit is reached:

- the search stops publishing occurrences from that file;
- traversal continues to the next files;
- `SearchSummary::filesWithMatchLimitReached` is incremented;
- `SearchSummary::limitReached` remains reserved for the global limit.

## Deterministic order

Direct search publishes files in deterministic path order. The scanner sorts entries in each directory by normalized path before visiting files and subdirectories.

The initial relevance strategy is deliberately simple: in combined searches, occurrences in the name/relative path are published before content occurrences from the same file. More sophisticated ranking should be introduced later with metrics and without breaking final order stability.

## Progressive results

Direct search publishes results as soon as each occurrence is found. It does not wait for the whole traversal to finish before delivering the first result to the consumer. Deterministic ordering is done per directory, not by materializing the whole tree in advance.

When `contextAfterLines` is greater than zero, content results may be held for up to that number of lines to fill following context. This delay is local to the file and does not require loading the whole file.

## Encoding, binaries, and lines

The core text reader detects BOM and supports:

- UTF-8 with or without BOM;
- UTF-16 LE with BOM;
- UTF-16 BE with BOM;
- configurable fallback to Latin-1 or UTF-8 without BOM.

Invalid UTF-8 follows `SearchOptions::invalidUtf8Policy`: replace with U+FFFD, skip the invalid byte, or fail reading. The default policy replaces invalid sequences to preserve search in partially corrupted files without aborting the whole directory.

Binary files are detected by configurable sample before line-by-line reading. Detection considers NUL bytes and the ratio of control bytes, but does not classify UTF-16 with BOM as binary only because it contains alternating NUL bytes.

## Line endings

Line-by-line reading supports `LF`, `CRLF`, standalone `CR`, empty lines, and files without a final newline. Line-ending markers are not part of `SearchResult::lineText`, and the reader records the line ending type in `TextLine` for future preview, offsets, and diagnostics.

## Document extraction

Milestone 12 introduces `core/document` as the stable boundary for text extracted from files. Search, indexing, preview, and future CLI code should consume `DocumentExtractor` contracts instead of calling format-specific parser libraries directly. The first implementation is `PlainTextExtractor`, which adapts the existing UTF-aware text reader into extracted line segments with document locations.

The extraction API distinguishes completed extraction, cancellation, unsupported formats, binary skips, open/read failures, invalid encoding, safety-limit skips, parser failures, and encrypted/protected documents. Rich formats such as PDF, OOXML, OpenDocument, HTML, RTF, subtitles, and email containers must plug into this boundary with explicit limits and status mapping rather than pretending to be plain text files. The current structured extractors cover HTML/XHTML visible text, SRT/VTT cue text, RTF visible text, initial PDF page text from simple unencrypted content streams, initial DOCX body text from `word/document.xml`, initial XLSX sheet text from workbook, shared-string, and worksheet XML, initial PPTX slide/speaker-note text from presentation relationships and slide XML, and initial OpenDocument text from `content.xml` and `meta.xml`.

The content-availability policy intentionally separates content extraction from file-name search. `contentAvailable` means extracted text can participate in content search. `nameOnlyUnsupported`, `nameOnlyBinary`, `nameOnlySafetyLimited`, and `nameOnlyProtected` mean the file remains searchable by path/name but its content is not exposed to matching or indexing. `extractionFailed` means the extractor attempted to process the file and hit a real parser/open/read failure that should be reported as a failure instead of silently downgrading to a name-only result. `cancelled` means the run ended cooperatively and must not be reported as a document failure.

File-name search remains a separate search target. Extractor failures must not prevent the product from representing that a file exists. Indexed search stores name-only entries when content extraction is unsupported, skipped, or failed after the file identity can be established. Those entries have no indexed text and therefore do not satisfy content-only queries, but they do satisfy file-name queries.

## Cancellation and partial failures

Cancellation is cooperative. Search must stop as soon as possible after the cancellation token is signaled.

The summary explicitly distinguishes:

- normal completion;
- cancellation;
- limit reached;
- partial read failure;
- invalid query.

Failures opening or reading a file do not silently stop the whole search. The summary marks `partialFailure`, increments `filesWithReadErrors`, and adds a typed error with the relative path in context. Already published results remain valid.

Permission failures, files removed between scan and read, and stream failures are normalized as typed partial failures. The concrete error may vary by platform, but search must not silently discard it.

## Files changed during search

Direct search represents the state observed when each file is opened. If a file changes between scan and read, the content read after opening is the source of truth for that occurrence.

If the file is removed, becomes inaccessible, or fails during reading, search records a typed partial failure and continues with other files. Milestone 1 does not try to create consistent snapshots of the whole tree; that guarantee belongs to the future index, overlay, and Git integration design.

## Rich document safety

Archive-backed document formats must pass `RichFormatSafetyLimits` and the `core/archive` ZIP catalog reader before an extractor exposes their content to search or indexing. The initial safety contract caps total expanded bytes, single expanded entry size, entry count, nesting depth, and compression ratio; the ZIP layer also rejects unsafe entry names and unsupported ZIP64 metadata, and payload access is explicit per entry rather than whole-archive extraction. DOCX currently has an initial extractor that reads `word/document.xml` only and exposes visible body text for direct search, indexed search, and preview. XLSX currently has an initial extractor that reads workbook, shared-string, and worksheet XML, then exposes sheet-scoped cell text without indexing raw package XML. PPTX currently has an initial extractor that reads presentation relationships, slide XML, and speaker-note XML when linked from the slide relationship graph, then exposes slide-scoped text without indexing raw package XML. OpenDocument currently has an initial extractor that reads `content.xml` and `meta.xml` for ODT, ODS, and ODP packages, publishing document, sheet, or slide-scoped text without indexing raw package XML. Other archive-backed formats such as EPUB and similar packages remain searchable by name only until their own extractors exist and pass the same limits.

PDF extraction is intentionally conservative in the initial implementation. It supports bounded local parsing of simple unencrypted PDFs, page-scoped result locations, literal strings, hex strings, and FlateDecode page streams. It rejects encrypted/protected files as protected content and reports malformed files as parser failures. Broader real-world PDF coverage, including object streams, custom font encodings, ToUnicode maps, forms, annotations, and layout-aware ordering, requires a dedicated permissively licensed backend decision before becoming part of the default contract.

## Ownership and copies

`SearchResult` owns the published text to remain safe when consumers process results asynchronously. The engine avoids intermediate copies in the hot path whenever it does not need to transfer ownership, but materializes the line/path when building the published result.
