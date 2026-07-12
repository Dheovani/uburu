# ADR 0001: Keep the search core independent from Qt/QML

## Context

Uburu needs a fast desktop UI, a CLI, automated tests, benchmarks, and future extension points. Search, indexing, Git integration, document extraction, and storage must remain usable without a graphical process.

## Decision

The reusable engine lives in `src/core` and does not depend on Qt. Application orchestration lives in `src/app`. Desktop code adapts application DTOs and controller state to Qt/QML. QML presents state and emits user intent, but it does not scan directories, read files, query SQLite, run regex, or own search semantics.

## Consequences

The same core can be used by the desktop app, CLI, tests, and benchmarks. UI responsiveness can be handled with queued events and worker execution without polluting the engine with Qt types. The cost is extra DTO and controller code, but that boundary prevents accidental coupling and keeps advanced indexing possible.
