# Symbols

Uburu treats symbol indexing as an optional layer over the document and file catalog. The file catalog remains authoritative for visibility, scope, Git state, and path identity; document extraction remains responsible for searchable text. Symbol parsers may add names, ranges, kinds, and language metadata, but they must not become a required path for ordinary text search.

## Tree-sitter evaluation

Tree-sitter is a good fit for source-code symbols because it is incremental, language-oriented, widely used by editors, and able to parse incomplete files better than compiler frontends. It should be evaluated as the first serious backend for symbols, not as a general text extractor and not as a hard dependency of `core/index`.

The required integration boundary is already `core/symbols`:

- `LanguageParser` detects language metadata from a path and optional content sample.
- `SymbolParser` extracts symbols for a language id.
- `SymbolParserRegistry` selects the parser without exposing backend details to indexing.
- `ExtensionLanguageParser` provides deterministic extension-based detection until content-aware detection is needed.

Tree-sitter must therefore enter as a replaceable adapter implementing `SymbolParser`. The adapter may own parser instances, language grammar handles, query objects, and parse-tree reuse, but callers should see only `SymbolParseRequest` and `SymbolParseResult`.

## Constraints for a future adapter

- Keep tree-sitter optional at build time.
- Do not require tree-sitter to run direct or indexed text search.
- Do not store backend-specific node ids or pointers in persisted index rows.
- Persist stable symbol data only: language id, symbol name, qualified name, kind, path, range, and selection range.
- Keep grammar selection explicit and versioned, because query behavior can change between grammar versions.
- Apply normal Uburu cancellation and size limits before and during parsing.
- Treat parse errors as partial symbol extraction, not as document extraction failure.
- Benchmark parser throughput before enabling symbols by default for large repositories.

## Initial decision

Tree-sitter is approved as the preferred future symbol backend candidate, but only behind the `SymbolParser` adapter. The current milestone should not add tree-sitter to `vcpkg.json` until at least one concrete language query and persistence path are ready. This avoids introducing a dependency that is not yet exercised by product behavior.
