# Integration tests

Integration tests cover filesystem, SQLite, Git, worktrees, and incremental reconciliation in deterministic temporary directories.

Keep scenarios small and behavior-oriented. Prefer disposable repositories and databases built during the test over checked-in binary fixtures.
