# ADR 0002: Use SQLite as the initial local storage backend

## Context

Uburu needs local persistence for repositories, worktrees, index generations, indexed documents, settings, history, saved searches, metrics, and recovery state. The database must be embedded, easy to deploy, and reliable on user machines.

## Decision

SQLite is the initial storage backend. It is accessed through `StorageService`, with migrations, WAL, foreign keys, busy timeout, integrity checks, generation publication, and recovery paths. Index and application services depend on storage interfaces rather than SQLite-specific construction.

## Consequences

Users do not need to install or maintain a database server. Releases only need to ship the SQLite dependency already handled through vcpkg. The design still allows future specialized index backends because SQLite is not exposed as the only search contract.
