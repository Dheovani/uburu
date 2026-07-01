#include "core/filesystem/path-normalization.hpp"
#include "core/git/git-cli-git-service.hpp"
#include "core/git/libgit2-git-service.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>

#if defined(UBURU_HAS_LIBGIT2)
#include <git2.h>
#endif

namespace
{

  class TemporaryDirectory
  {
  public:
    explicit TemporaryDirectory(std::string name)
      : pathValue(std::filesystem::temp_directory_path() / uniqueName(std::move(name)))
    {
      std::error_code error;

      std::filesystem::remove_all(pathValue, error);
      std::filesystem::create_directories(pathValue);
    }

    ~TemporaryDirectory()
    {
      std::error_code error;

      std::filesystem::remove_all(pathValue, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const
    {
      return pathValue;
    }

  private:
    [[nodiscard]] static std::string uniqueName(std::string name)
    {
      const auto now = std::chrono::steady_clock::now().time_since_epoch().count();

      return name + "-" + std::to_string(now);
    }

    std::filesystem::path pathValue;
  };

  void writeFile(const std::filesystem::path& path, std::string_view content)
  {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
  }

#if defined(UBURU_HAS_LIBGIT2)

  void createInitialCommit(const std::filesystem::path& repositoryRoot)
  {
    git_libgit2_init();

    git_repository* repository = nullptr;
    REQUIRE(git_repository_init(&repository, repositoryRoot.string().c_str(), false) == 0);

    writeFile(repositoryRoot / "tracked.txt", "tracked\n");
    writeFile(repositoryRoot / "modify-me.txt", "modify me\n");
    writeFile(repositoryRoot / "delete-me.txt", "delete me\n");
    writeFile(repositoryRoot / ".gitignore", "*.ignored\n");

    git_index* index = nullptr;
    REQUIRE(git_repository_index(&index, repository) == 0);
    REQUIRE(git_index_add_bypath(index, "tracked.txt") == 0);
    REQUIRE(git_index_add_bypath(index, "modify-me.txt") == 0);
    REQUIRE(git_index_add_bypath(index, "delete-me.txt") == 0);
    REQUIRE(git_index_add_bypath(index, ".gitignore") == 0);
    REQUIRE(git_index_write(index) == 0);

    git_oid treeOid{};
    REQUIRE(git_index_write_tree(&treeOid, index) == 0);

    git_tree* tree = nullptr;
    REQUIRE(git_tree_lookup(&tree, repository, &treeOid) == 0);

    git_signature* signature = nullptr;
    REQUIRE(git_signature_now(&signature, "Uburu Tests", "tests@uburu.local") == 0);

    git_oid commitOid{};
    REQUIRE(git_commit_create_v(
              &commitOid, repository, "refs/heads/main", signature, signature, nullptr, "initial commit", tree, 0) ==
            0);
    REQUIRE(git_repository_set_head(repository, "refs/heads/main") == 0);

    git_signature_free(signature);
    git_tree_free(tree);
    git_index_free(index);
    git_repository_free(repository);
    git_libgit2_shutdown();
  }

  void stageFile(const std::filesystem::path& repositoryRoot, std::string_view relativePath)
  {
    git_libgit2_init();

    git_repository* repository = nullptr;
    REQUIRE(git_repository_open(&repository, (repositoryRoot / ".git").string().c_str()) == 0);

    git_index* index = nullptr;
    REQUIRE(git_repository_index(&index, repository) == 0);
    REQUIRE(git_index_add_bypath(index, std::string(relativePath).c_str()) == 0);
    REQUIRE(git_index_write(index) == 0);

    git_index_free(index);
    git_repository_free(repository);
    git_libgit2_shutdown();
  }

  git_index_entry conflictEntry(const git_oid& oid, const char* path, std::uint16_t stage)
  {
    git_index_entry entry{};
    entry.mode = GIT_FILEMODE_BLOB;
    entry.id = oid;
    entry.path = path;
    entry.flags = static_cast<std::uint16_t>(stage << GIT_INDEX_ENTRY_STAGESHIFT);

    return entry;
  }

  void addIndexConflict(const std::filesystem::path& repositoryRoot, const char* relativePath)
  {
    git_libgit2_init();

    git_repository* repository = nullptr;
    REQUIRE(git_repository_open(&repository, (repositoryRoot / ".git").string().c_str()) == 0);

    git_oid ancestorOid{};
    git_oid oursOid{};
    git_oid theirsOid{};
    REQUIRE(git_blob_create_frombuffer(&ancestorOid, repository, "ancestor\n", 9) == 0);
    REQUIRE(git_blob_create_frombuffer(&oursOid, repository, "ours\n", 5) == 0);
    REQUIRE(git_blob_create_frombuffer(&theirsOid, repository, "theirs\n", 7) == 0);

    git_index* index = nullptr;
    REQUIRE(git_repository_index(&index, repository) == 0);

    const auto ancestor = conflictEntry(ancestorOid, relativePath, 1);
    const auto ours = conflictEntry(oursOid, relativePath, 2);
    const auto theirs = conflictEntry(theirsOid, relativePath, 3);

    REQUIRE(git_index_conflict_add(index, &ancestor, &ours, &theirs) == 0);
    REQUIRE(git_index_write(index) == 0);

    git_index_free(index);
    git_repository_free(repository);
    git_libgit2_shutdown();
  }

  void addLinkedWorktree(const std::filesystem::path& repositoryRoot,
                         const std::filesystem::path& worktreeRoot,
                         std::string_view name = "feature")
  {
    git_libgit2_init();

    git_repository* repository = nullptr;
    REQUIRE(git_repository_open(&repository, (repositoryRoot / ".git").string().c_str()) == 0);

    git_object* headCommit = nullptr;
    REQUIRE(git_revparse_single(&headCommit, repository, "HEAD^{commit}") == 0);

    git_reference* featureBranch = nullptr;
    REQUIRE(git_branch_create(&featureBranch,
                              repository,
                              std::string(name).c_str(),
                              reinterpret_cast<const git_commit*>(headCommit),
                              false) == 0);

    git_worktree_add_options options = GIT_WORKTREE_ADD_OPTIONS_INIT;
    options.ref = featureBranch;

    git_worktree* worktree = nullptr;
    REQUIRE(
      git_worktree_add(&worktree, repository, std::string(name).c_str(), worktreeRoot.string().c_str(), &options) == 0);

    git_worktree_free(worktree);
    git_reference_free(featureBranch);
    git_object_free(headCommit);
    git_repository_free(repository);
    git_libgit2_shutdown();
  }

  void lockLinkedWorktree(const std::filesystem::path& repositoryRoot, std::string_view name)
  {
    git_libgit2_init();

    git_repository* repository = nullptr;
    REQUIRE(git_repository_open(&repository, (repositoryRoot / ".git").string().c_str()) == 0);

    git_worktree* worktree = nullptr;
    REQUIRE(git_worktree_lookup(&worktree, repository, std::string(name).c_str()) == 0);
    REQUIRE(git_worktree_lock(worktree, "locked by test") == 0);

    git_worktree_free(worktree);
    git_repository_free(repository);
    git_libgit2_shutdown();
  }

  void addSubmodule(const std::filesystem::path& repositoryRoot,
                    const std::filesystem::path& submoduleRepositoryRoot,
                    std::string_view relativePath)
  {
    git_libgit2_init();

    git_repository* repository = nullptr;
    REQUIRE(git_repository_open(&repository, (repositoryRoot / ".git").string().c_str()) == 0);

    git_submodule* submodule = nullptr;
    REQUIRE(
      git_submodule_add_setup(
        &submodule, repository, submoduleRepositoryRoot.string().c_str(), std::string(relativePath).c_str(), true) ==
      0);

    git_submodule_free(submodule);
    git_repository_free(repository);
    git_libgit2_shutdown();
  }

  void detachHead(const std::filesystem::path& repositoryRoot)
  {
    git_libgit2_init();

    git_repository* repository = nullptr;
    REQUIRE(git_repository_open(&repository, (repositoryRoot / ".git").string().c_str()) == 0);

    git_oid headOid{};
    REQUIRE(git_reference_name_to_id(&headOid, repository, "HEAD") == 0);
    REQUIRE(git_repository_set_head_detached(repository, &headOid) == 0);

    git_repository_free(repository);
    git_libgit2_shutdown();
  }

#endif

} // namespace

TEST_CASE("libgit2 git service reports typed error outside repositories")
{
  TemporaryDirectory directory("uburu-libgit2-git-service-not-repo-test");
  const uburu::git::Libgit2GitService service;

  const auto result = service.discoverRepository(directory.path());
  const auto* error = std::get_if<uburu::git::GitError>(&result);

  REQUIRE(error != nullptr);
  CHECK(error->code == uburu::git::GitErrorCode::notRepository);
}

TEST_CASE("libgit2 git service discovers repository metadata")
{
#if defined(UBURU_HAS_LIBGIT2)
  TemporaryDirectory directory("uburu-libgit2-git-service-discover-test");
  createInitialCommit(directory.path());

  const uburu::git::Libgit2GitService service;
  const auto result = service.discoverRepository(directory.path() / "tracked.txt");
  const auto* repository = std::get_if<uburu::RepositoryInfo>(&result);

  REQUIRE(repository != nullptr);
  CHECK_FALSE(repository->id.empty());
  REQUIRE(repository->worktreeRoot.has_value());
  CHECK(std::filesystem::equivalent(*repository->worktreeRoot, directory.path()));
  CHECK_FALSE(repository->headOid.empty());
  CHECK_FALSE(repository->detachedHead);
#else
  SUCCEED("libgit2 is not available in this build");
#endif
}

TEST_CASE("libgit2 git service lists the main worktree")
{
#if defined(UBURU_HAS_LIBGIT2)
  TemporaryDirectory directory("uburu-libgit2-git-service-worktree-test");
  createInitialCommit(directory.path());

  const uburu::git::Libgit2GitService service;
  const auto repositoryResult = service.discoverRepository(directory.path());
  const auto& repository = std::get<uburu::RepositoryInfo>(repositoryResult);
  const auto worktreesResult = service.listWorktrees(repository);
  const auto* worktrees = std::get_if<std::vector<uburu::WorktreeInfo>>(&worktreesResult);

  REQUIRE(worktrees != nullptr);
  REQUIRE_FALSE(worktrees->empty());
  CHECK(std::filesystem::equivalent(worktrees->front().root, directory.path()));
  CHECK(worktrees->front().repositoryId == repository.id);
#else
  SUCCEED("libgit2 is not available in this build");
#endif
}

TEST_CASE("git cli fallback is isolated behind an explicit adapter")
{
  const uburu::git::GitCliGitService service;
  const auto result = service.discoverRepository(std::filesystem::temp_directory_path());
  const auto* error = std::get_if<uburu::git::GitError>(&result);

  REQUIRE(error != nullptr);
  CHECK(error->code == uburu::git::GitErrorCode::backendUnavailable);
}

TEST_CASE("libgit2 git service enumerates linked worktrees")
{
#if defined(UBURU_HAS_LIBGIT2)
  TemporaryDirectory directory("uburu-libgit2-git-service-linked-worktree-test");
  const auto repositoryRoot = directory.path() / "repo";
  const auto linkedRoot = directory.path() / "feature-worktree";

  std::filesystem::create_directories(repositoryRoot);
  createInitialCommit(repositoryRoot);
  addLinkedWorktree(repositoryRoot, linkedRoot);

  const uburu::git::Libgit2GitService service;
  const auto repositoryResult = service.discoverRepository(repositoryRoot);
  const auto& repository = std::get<uburu::RepositoryInfo>(repositoryResult);
  const auto worktreesResult = service.listWorktrees(repository);
  const auto& worktrees = std::get<std::vector<uburu::WorktreeInfo>>(worktreesResult);

  REQUIRE(worktrees.size() == 2);

  const auto hasMainWorktree = std::ranges::any_of(
    worktrees, [&](const auto& worktree) { return std::filesystem::equivalent(worktree.root, repositoryRoot); });
  const auto hasLinkedWorktree = std::ranges::any_of(
    worktrees, [&](const auto& worktree) { return std::filesystem::equivalent(worktree.root, linkedRoot); });

  CHECK(hasMainWorktree);
  CHECK(hasLinkedWorktree);
#else
  SUCCEED("libgit2 is not available in this build");
#endif
}

TEST_CASE("libgit2 git service reports locked and prunable worktrees")
{
#if defined(UBURU_HAS_LIBGIT2)
  TemporaryDirectory directory("uburu-libgit2-git-service-worktree-state-test");
  const auto repositoryRoot = directory.path() / "repo";
  const auto linkedRoot = directory.path() / "locked-worktree";
  const auto removedRoot = directory.path() / "removed-worktree";

  std::filesystem::create_directories(repositoryRoot);
  createInitialCommit(repositoryRoot);
  addLinkedWorktree(repositoryRoot, linkedRoot);
  lockLinkedWorktree(repositoryRoot, "feature");

  const uburu::git::Libgit2GitService service;
  const auto repositoryResult = service.discoverRepository(repositoryRoot);
  const auto& repository = std::get<uburu::RepositoryInfo>(repositoryResult);
  const auto worktreesResult = service.listWorktrees(repository);
  const auto& worktrees = std::get<std::vector<uburu::WorktreeInfo>>(worktreesResult);

  const auto locked = std::ranges::find_if(
    worktrees, [&](const auto& worktree) { return std::filesystem::equivalent(worktree.root, linkedRoot); });

  REQUIRE(locked != worktrees.end());
  CHECK(locked->locked);
  CHECK_FALSE(locked->lockReason.empty());

  addLinkedWorktree(repositoryRoot, removedRoot, "removed");
  std::error_code error;
  std::filesystem::remove_all(removedRoot, error);
  REQUIRE_FALSE(error);
  REQUIRE_FALSE(std::filesystem::exists(removedRoot));
  writeFile(repositoryRoot / ".git" / "worktrees" / "removed" / "gitdir", (removedRoot / ".git").string() + "\n");

  const auto refreshedResult = service.listWorktrees(repository);
  const auto& refreshed = std::get<std::vector<uburu::WorktreeInfo>>(refreshedResult);
  const auto removedRootKey = uburu::filesystem::normalizedPathKey(removedRoot);
  const auto prunable = std::ranges::find_if(refreshed, [&](const auto& worktree) {
    return uburu::filesystem::normalizedPathKey(worktree.root) == removedRootKey;
  });

  REQUIRE(prunable != refreshed.end());
  CHECK(prunable->prunable);
#else
  SUCCEED("libgit2 is not available in this build");
#endif
}

TEST_CASE("libgit2 git service detects detached HEAD")
{
#if defined(UBURU_HAS_LIBGIT2)
  TemporaryDirectory directory("uburu-libgit2-git-service-detached-test");
  createInitialCommit(directory.path());
  detachHead(directory.path());

  const uburu::git::Libgit2GitService service;
  const auto result = service.discoverRepository(directory.path());
  const auto* repository = std::get_if<uburu::RepositoryInfo>(&result);

  REQUIRE(repository != nullptr);
  CHECK(repository->detachedHead);
  CHECK_FALSE(repository->currentBranch.has_value());
  CHECK_FALSE(repository->headOid.empty());

  const auto worktreesResult = service.listWorktrees(*repository);
  const auto& worktree = std::get<std::vector<uburu::WorktreeInfo>>(worktreesResult).front();
  const auto state = service.changeState(worktree);
  const auto& changeState = std::get<uburu::git::GitChangeState>(state);

  CHECK(changeState.detachedHead);
  CHECK_FALSE(changeState.branch.has_value());
  CHECK_FALSE(changeState.headSignature.empty());
#else
  SUCCEED("libgit2 is not available in this build");
#endif
}

TEST_CASE("libgit2 git service reads file status and blob hashes")
{
#if defined(UBURU_HAS_LIBGIT2)
  TemporaryDirectory directory("uburu-libgit2-git-service-status-test");
  createInitialCommit(directory.path());

  const uburu::git::Libgit2GitService service;
  const auto repositoryResult = service.discoverRepository(directory.path());
  const auto& repository = std::get<uburu::RepositoryInfo>(repositoryResult);
  const auto worktreesResult = service.listWorktrees(repository);
  const auto& worktree = std::get<std::vector<uburu::WorktreeInfo>>(worktreesResult).front();

  const auto clean = service.fileStatus(worktree, "tracked.txt");
  CHECK(std::get<uburu::GitFileStatus>(clean) == uburu::GitFileStatus::clean);

  const auto blob = service.blobHash(worktree, "tracked.txt");
  const auto& blobHash = std::get<std::optional<std::string>>(blob);

  REQUIRE(blobHash.has_value());
  CHECK_FALSE(blobHash->empty());

  const auto stateBeforeIndexChange = service.changeState(worktree);
  const auto& beforeIndexChange = std::get<uburu::git::GitChangeState>(stateBeforeIndexChange);

  writeFile(directory.path() / "modify-me.txt", "modified\n");
  writeFile(directory.path() / "new.txt", "new\n");
  writeFile(directory.path() / "staged.txt", "staged\n");
  writeFile(directory.path() / "ignored.ignored", "ignored\n");
  std::filesystem::remove(directory.path() / "delete-me.txt");
  stageFile(directory.path(), "staged.txt");
  addIndexConflict(directory.path(), "tracked.txt");

  const auto modified = service.fileStatus(worktree, "modify-me.txt");
  const auto untracked = service.fileStatus(worktree, "new.txt");
  const auto added = service.fileStatus(worktree, "staged.txt");
  const auto deleted = service.fileStatus(worktree, "delete-me.txt");
  const auto ignored = service.fileStatus(worktree, "ignored.ignored");

  const auto conflicted = service.fileStatus(worktree, "tracked.txt");
  const auto stateAfterIndexChange = service.changeState(worktree);
  const auto& afterIndexChange = std::get<uburu::git::GitChangeState>(stateAfterIndexChange);

  CHECK(std::get<uburu::GitFileStatus>(modified) == uburu::GitFileStatus::modified);
  CHECK(std::get<uburu::GitFileStatus>(conflicted) == uburu::GitFileStatus::conflicted);
  CHECK(std::get<uburu::GitFileStatus>(untracked) == uburu::GitFileStatus::untracked);
  CHECK(std::get<uburu::GitFileStatus>(added) == uburu::GitFileStatus::added);
  CHECK(std::get<uburu::GitFileStatus>(deleted) == uburu::GitFileStatus::deleted);
  CHECK(std::get<uburu::GitFileStatus>(ignored) == uburu::GitFileStatus::ignored);
  CHECK(beforeIndexChange.indexSignature != afterIndexChange.indexSignature);
#else
  SUCCEED("libgit2 is not available in this build");
#endif
}

TEST_CASE("libgit2 git service models working tree overlay and rename reuse")
{
#if defined(UBURU_HAS_LIBGIT2)
  TemporaryDirectory directory("uburu-libgit2-git-service-overlay-test");
  createInitialCommit(directory.path());

  const uburu::git::Libgit2GitService service;
  const auto repositoryResult = service.discoverRepository(directory.path());
  const auto& repository = std::get<uburu::RepositoryInfo>(repositoryResult);
  const auto worktreesResult = service.listWorktrees(repository);
  const auto& worktree = std::get<std::vector<uburu::WorktreeInfo>>(worktreesResult).front();
  const auto algorithm = service.objectHashAlgorithm(repository);

  CHECK(std::get<uburu::GitObjectHashAlgorithm>(algorithm) == uburu::GitObjectHashAlgorithm::sha1);

  std::filesystem::rename(directory.path() / "tracked.txt", directory.path() / "renamed.txt");
  writeFile(directory.path() / "new.txt", "new\n");
  std::filesystem::remove(directory.path() / "delete-me.txt");

  const auto overlayResult = service.workingTreeOverlay(worktree);
  const auto& overlay = std::get<std::vector<uburu::GitOverlayEntry>>(overlayResult);

  const auto renamed =
    std::ranges::find_if(overlay, [](const auto& entry) { return entry.relativePath == "renamed.txt"; });
  const auto added = std::ranges::find_if(overlay, [](const auto& entry) { return entry.relativePath == "new.txt"; });
  const auto deleted =
    std::ranges::find_if(overlay, [](const auto& entry) { return entry.relativePath == "delete-me.txt"; });

  REQUIRE(renamed != overlay.end());
  CHECK(renamed->previousRelativePath == std::filesystem::path("tracked.txt"));
  REQUIRE(renamed->reusableBlob.has_value());
  CHECK(renamed->reusableBlob->algorithm == uburu::GitObjectHashAlgorithm::sha1);
  CHECK_FALSE(renamed->reusableBlob->value.empty());

  REQUIRE(added != overlay.end());
  CHECK(added->disposition == uburu::GitOverlayDisposition::addWorkingTreeFile);

  REQUIRE(deleted != overlay.end());
  CHECK(deleted->disposition == uburu::GitOverlayDisposition::hideIndexedContent);
#else
  SUCCEED("libgit2 is not available in this build");
#endif
}

TEST_CASE("libgit2 git service detects nested repository boundaries")
{
#if defined(UBURU_HAS_LIBGIT2)
  TemporaryDirectory directory("uburu-libgit2-git-service-boundary-test");
  createInitialCommit(directory.path());
  std::filesystem::create_directories(directory.path() / "nested");
  createInitialCommit(directory.path() / "nested");

  const uburu::git::Libgit2GitService service;
  const auto repositoryResult = service.discoverRepository(directory.path());
  const auto& repository = std::get<uburu::RepositoryInfo>(repositoryResult);
  const auto worktreesResult = service.listWorktrees(repository);
  const auto& worktree = std::get<std::vector<uburu::WorktreeInfo>>(worktreesResult).front();
  const auto boundaryResult = service.repositoryBoundary(worktree, "nested");
  const auto& boundary = std::get<uburu::GitRepositoryBoundary>(boundaryResult);

  CHECK(boundary.kind == uburu::GitRepositoryBoundaryKind::nestedRepository);
#else
  SUCCEED("libgit2 is not available in this build");
#endif
}

TEST_CASE("libgit2 git service detects submodule repository boundaries")
{
#if defined(UBURU_HAS_LIBGIT2)
  TemporaryDirectory directory("uburu-libgit2-git-service-submodule-boundary-test");
  const auto repositoryRoot = directory.path() / "repo";
  const auto submoduleRepositoryRoot = directory.path() / "library-source";

  std::filesystem::create_directories(repositoryRoot);
  std::filesystem::create_directories(submoduleRepositoryRoot);
  createInitialCommit(repositoryRoot);
  createInitialCommit(submoduleRepositoryRoot);
  addSubmodule(repositoryRoot, submoduleRepositoryRoot, "vendor/library");

  const uburu::git::Libgit2GitService service;
  const auto repositoryResult = service.discoverRepository(repositoryRoot);
  const auto& repository = std::get<uburu::RepositoryInfo>(repositoryResult);
  const auto worktreesResult = service.listWorktrees(repository);
  const auto& worktree = std::get<std::vector<uburu::WorktreeInfo>>(worktreesResult).front();
  const auto boundaryResult = service.repositoryBoundary(worktree, "vendor/library");
  const auto& boundary = std::get<uburu::GitRepositoryBoundary>(boundaryResult);

  CHECK(boundary.kind == uburu::GitRepositoryBoundaryKind::submodule);
#else
  SUCCEED("libgit2 is not available in this build");
#endif
}
