#include "core/git/libgit2-git-service.hpp"
#include "helpers/libgit2-repository-fixture.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace
{

#if defined(UBURU_HAS_LIBGIT2)

  [[nodiscard]] uburu::WorktreeInfo mainWorktreeFor(const uburu::git::Libgit2GitService& service,
                                                    const std::filesystem::path& repositoryRoot)
  {
    const auto repositoryResult = service.discoverRepository(repositoryRoot);
    const auto& repository = std::get<uburu::RepositoryInfo>(repositoryResult);
    const auto worktreesResult = service.listWorktrees(repository);
    const auto& worktrees = std::get<std::vector<uburu::WorktreeInfo>>(worktreesResult);

    REQUIRE_FALSE(worktrees.empty());

    return worktrees.front();
  }

  [[nodiscard]] std::optional<uburu::GitOverlayEntry>
  findOverlayEntry(const std::vector<uburu::GitOverlayEntry>& overlay, const std::filesystem::path& relativePath)
  {
    const auto entry = std::find_if(
      overlay.begin(), overlay.end(), [&](const auto& candidate) { return candidate.relativePath == relativePath; });

    if (entry == overlay.end())
      return std::nullopt;

    return *entry;
  }

#endif

} // namespace

TEST_CASE("libgit2 integration tracks branch and HEAD changes in a disposable repository")
{
#if defined(UBURU_HAS_LIBGIT2)
  uburu::tests::Libgit2RepositoryDirectory directory("uburu-libgit2-integration-branch-change");
  uburu::tests::createBasicRepository(directory.path());

  const uburu::git::Libgit2GitService service;
  const auto initialWorktree = mainWorktreeFor(service, directory.path());
  const auto initialStateResult = service.changeState(initialWorktree);
  const auto& initialState = std::get<uburu::git::GitChangeState>(initialStateResult);

  uburu::tests::createAndCheckoutBranch(directory.path(), "feature");
  uburu::tests::writeFile(directory.path() / "feature.txt", "feature only\n");
  uburu::tests::commitAll(directory.path(), "feature commit");

  const auto changedWorktree = mainWorktreeFor(service, directory.path());
  const auto changedStateResult = service.changeState(changedWorktree);
  const auto& changedState = std::get<uburu::git::GitChangeState>(changedStateResult);

  REQUIRE(changedState.branch.has_value());
  CHECK(*changedState.branch == "feature");
  CHECK_FALSE(changedState.detachedHead);
  CHECK_FALSE(changedState.headOid.empty());
  CHECK(initialState.headOid != changedState.headOid);
#else
  SUCCEED("libgit2 is not available in this build");
#endif
}

TEST_CASE("libgit2 integration builds a working tree overlay from real file changes")
{
#if defined(UBURU_HAS_LIBGIT2)
  uburu::tests::Libgit2RepositoryDirectory directory("uburu-libgit2-integration-overlay");
  uburu::tests::createBasicRepository(directory.path());

  const uburu::git::Libgit2GitService service;
  const auto worktree = mainWorktreeFor(service, directory.path());

  uburu::tests::writeFile(directory.path() / "modify-me.txt", "modified\n");
  uburu::tests::writeFile(directory.path() / "new.txt", "new\n");
  std::filesystem::remove(directory.path() / "delete-me.txt");

  const auto overlayResult = service.workingTreeOverlay(worktree);
  const auto& overlay = std::get<std::vector<uburu::GitOverlayEntry>>(overlayResult);
  const auto modified = findOverlayEntry(overlay, "modify-me.txt");
  const auto added = findOverlayEntry(overlay, "new.txt");
  const auto deleted = findOverlayEntry(overlay, "delete-me.txt");

  REQUIRE(modified.has_value());
  CHECK(modified->status == uburu::GitFileStatus::modified);
  CHECK(modified->disposition == uburu::GitOverlayDisposition::replaceWithWorkingTree);

  REQUIRE(added.has_value());
  CHECK(added->status == uburu::GitFileStatus::untracked);
  CHECK(added->disposition == uburu::GitOverlayDisposition::addWorkingTreeFile);

  REQUIRE(deleted.has_value());
  CHECK(deleted->status == uburu::GitFileStatus::deleted);
  CHECK(deleted->disposition == uburu::GitOverlayDisposition::hideIndexedContent);
#else
  SUCCEED("libgit2 is not available in this build");
#endif
}
