# Git awareness

`RepositoryInfo` represents the logical repository, its common Git directory, and, when available, the discovered worktree root. `WorktreeInfo` represents a physical tree, its worktree-specific Git directory, its HEAD, and its optional branch. A missing branch with a valid HEAD represents detached HEAD.

`Libgit2GitService` is the initial `GitService` implementation. It discovers repositories through libgit2, resolves the common Git directory, worktree root, current branch, detached HEAD, `HEAD` OID, linked worktrees, file status, and tracked blob OIDs. Status already differentiates clean files, index additions, untracked files, ignored files, modified files, deleted files, and conflicts. Operations return `GitResult<T>` with a typed `GitErrorCode`, making it possible to distinguish missing repositories, read failures, and unavailable backends. The Git CLI fallback exists only as `GitCliGitService`, an explicit adapter that is not used implicitly by the core. Changes in `HEAD`, the index, and relevant refs trigger incremental reconciliation.

`GitService::changeState()` exposes a comparable snapshot of the Git state visible for a worktree: current branch, `HEAD`, detached HEAD, and signatures for `HEAD`, `index`, the branch ref, and `packed-refs`. This snapshot does not replace filesystem watchers; it defines the state that watchers compare to decide when to invalidate deltas and start incremental reconciliation.

`planReconciliation()` compares two `GitChangeState` snapshots and turns differences into an explicit plan. Branch changes, `HEAD` changes, entering or leaving detached HEAD, and relevant-ref changes require structural reconciliation, while still allowing document reuse by blob hash. An index-only change requires local overlay reconciliation without treating the versioned generation as structurally stale.

The searchable view is always:

```text
commit/index content + working tree overlay
```

Modified and untracked files replace indexed content, while deleted files are hidden. Submodules are explicit boundaries: they can be ignored or indexed as their own repositories, but must never be walked accidentally.

## Working tree overlay

`GitService::workingTreeOverlay()` materializes the difference between the versioned generation and the working tree visible to the user. Each `GitOverlayEntry` carries:

- `relativePath`, the current path that should appear in search;
- `previousRelativePath`, when libgit2 identifies a rename or move;
- `status`, the raw Git state normalized to Uburu types;
- `disposition`, the search/indexing decision for the versioned generation;
- `reusableBlob`, when versioned content can be reused by blob hash.

Initial dispositions are:

- `useIndexedContent`: the versioned document remains valid;
- `replaceWithWorkingTree`: the working tree replaces versioned content;
- `addWorkingTreeFile`: a new file should enter as local overlay;
- `hideIndexedContent`: a locally deleted file should not appear as a stale result;
- `conflict`: a conflicted file should be handled conservatively.

Renames and moves are not treated as deletion followed by addition when libgit2 provides the previous path. The overlay preserves `previousRelativePath` and tries to resolve `reusableBlob` on the old path, allowing the future index to reuse documents by blob hash instead of rereading known content.

## Repository boundaries

`GitService::repositoryBoundary()` differentiates three cases:

- `none`: the path belongs normally to the current worktree;
- `submodule`: the path is registered as a submodule of the current repository;
- `nestedRepository`: the path contains its own `.git`, but is not a registered submodule.

Submodules and nested repositories are explicit boundaries. The scanner and index may offer a policy to ignore them, traverse them deliberately, or index them as another scope, but they must not enter those repositories by accident as if they were regular directories.

## Unavailable worktrees

`WorktreeInfo` also represents locked or prunable linked worktrees. A locked worktree exposes `locked` and `lockReason`; a physically removed worktree that is still registered by Git exposes `prunable`. The index should treat these states as structural metadata: it should not immediately erase history or cache, but it also should not assume that the physical tree can be scanned.

## Git hash algorithm

Git object hashes are represented by `GitObjectId`, which carries the algorithm (`sha1`, `sha256`, or `unknown`) together with the textual value. libgit2 currently validates the SHA-1 path in tests, and the contract is already prepared for SHA-256 repositories when the environment and dependencies expose that mode.
