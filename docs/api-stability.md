# API and ABI stability

Uburu does not expose a stable plugin ABI yet. All C++ extension points created before version 1.0 are internal contracts, even when they are deliberately shaped as replaceable interfaces. This includes index backend factories, file watcher factories, symbol parsers, and document extractors.

## Current policy

- Source-level API compatibility is best effort inside the repository only.
- Binary ABI compatibility is not promised for external plugins or separately compiled modules.
- Public plugin loading must not be enabled until contracts have explicit versions, ownership rules, threading rules, and compatibility checks.
- Internal factories may evolve between minor releases while their `ContractVersion` remains below major version `1`.
- A contract marked `internal` or `experimental` must not be advertised as stable to third-party developers.

## Version metadata

`core/contracts` defines the first contract metadata used by replaceable boundaries:

- `uburu.index.backend`
- `uburu.filesystem.file-watcher.backend`
- `uburu.symbols.parser`

Each contract has a semantic API version, an ABI revision, and a stability flag. The current version is `0.1.0` with ABI revision `0`, meaning the shape is useful for internal architecture but not yet a compatibility promise.

## Requirements before external plugins

Before Uburu loads external plugins, each public contract must define:

- stable ownership of objects passed across the boundary;
- allocator and destruction rules;
- threading and cancellation rules;
- error-reporting rules;
- version negotiation;
- behavior for unsupported or newer contract versions;
- supported operating systems and compiler/runtime expectations;
- tests that prove older compatible implementations still load.

Until then, plugin-like backends should be linked into the application or built as internal adapters.
