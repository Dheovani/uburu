# Test architecture

Uburu's test suite is organized around behavior boundaries rather than implementation files. Unit tests verify pure rules and service contracts, integration tests exercise real temporary files, repositories, and databases, desktop tests validate the application/controller boundary, fuzzers stress hostile inputs, and benchmarks measure performance without replacing correctness tests.

## Test layers

Unit tests live in `tests/unit`. They should be small, deterministic, and focused on one behavior: matching semantics, path normalization, `.gitignore` rules, document extraction outcomes, storage migrations, indexing reconciliation, CLI parsing, DTO conversion, and service event contracts. A unit test may create files when the behavior is filesystem-specific, but it should still keep the scenario minimal.

Integration tests live in `tests/integration`. They are reserved for behavior that only makes sense with real operating-system resources, SQLite databases, Git repositories, worktrees, or watcher reconciliation. They must use disposable directories and avoid assumptions about the developer's machine.

Desktop tests cover the boundary between Qt-facing controllers/models and the application layer. They should prove observable state, cancellation, preview loading, result actions, selected scope handling, and model roles without moving search semantics into QML.

Fuzzers live in `tests/fuzzers`. They are smoke-oriented in CI and should have explicit size, time, and output limits. Longer fuzzing sessions are developer tasks, not normal build steps.

Benchmarks live in `benchmarks`. They must not be used as correctness checks. Their job is to produce comparable timing, throughput, memory, batching, and indexing counters for known scenarios.

## Fixtures and temporary resources

Shared tiny fixtures live in `tests/fixtures`. Prefer semantic fixture helpers over checked-in large files. When a test needs real files, use the RAII helpers in `tests/helpers` so temporary paths are unique, cleaned up, and safe for parallel CTest execution.

Tests that create Git repositories, SQLite databases, archives, or document packages should build them locally inside a temporary directory. This makes the test self-contained and avoids binary fixtures whose structure is hard to review.

## Naming and selection

Test names should describe the observable behavior in plain English. Prefer names such as `direct search reports files removed between scan and read as partial failures` over names tied to private helper functions.

Use CTest regular expressions for focused validation:

```powershell
ctest --test-dir build/windows-msvc-debug -C Debug --output-on-failure -R "zip archive reader"
```

When debugging CI, reproduce the smallest failing target first, then expand to the full preset once the local failure is fixed.

## Quality rules

Critical core behavior requires automated tests before it is marked complete in `TODO.md`. Performance-sensitive behavior requires a benchmark, metric, or reproducible measurement. UI behavior should be tested at the controller/model boundary whenever possible, keeping QML focused on rendering and interaction.

Tests must not depend on user-specific paths, installed applications, network access, system locale, or execution order. If a test must be platform-specific, it should make that requirement explicit and skip safely when the platform cannot support it.
