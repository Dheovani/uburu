# ADR 0003: Prefer bounded built-in document extractors for version 1.0

## Context

Version 1.0 needs content search for common rich formats such as PDF, DOCX, XLSX, PPTX, OpenDocument, RTF, HTML, and subtitles. These formats can contain malformed, hostile, compressed, protected, or very large data. Heavy third-party parser stacks can improve fidelity but may add packaging, binary size, security-update, and maintenance costs.

## Decision

Uburu ships bounded built-in extractors as the default 1.0 path. Archive-backed formats go through the shared ZIP safety layer. Extractors enforce byte, segment, nesting, compression, and cancellation limits, and they report explicit availability and failure states. Optional heavier parser backends are deferred until their cost is measured.

## Consequences

The first release gets useful content search while keeping security and packaging under control. Extraction fidelity is intentionally conservative, especially for complex PDFs and unsupported legacy formats. Future parser backends can be added behind the document-extraction interface without changing search, indexing, or preview consumers.
