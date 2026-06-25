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
        : path_(std::filesystem::temp_directory_path() / unique_name(std::move(name)))
    {
      std::error_code error;

      std::filesystem::remove_all(path_, error);
      std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory()
    {
      std::error_code error;

      std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const
    {
      return path_;
    }

  private:
    [[nodiscard]] static std::string unique_name(std::string name)
    {
      const auto now = std::chrono::steady_clock::now().time_since_epoch().count();

      return name + "-" + std::to_string(now);
    }

    std::filesystem::path path_;
  };

  void write_file(const std::filesystem::path& path, std::string_view content)
  {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
  }

#if defined(UBURU_HAS_LIBGIT2)

  void create_initial_commit(const std::filesystem::path& repository_root)
  {
    git_libgit2_init();

    git_repository* repository = nullptr;
    REQUIRE(git_repository_init(&repository, repository_root.string().c_str(), false) == 0);

    write_file(repository_root / "tracked.txt", "tracked\n");
    write_file(repository_root / "modify-me.txt", "modify me\n");
    write_file(repository_root / "delete-me.txt", "delete me\n");
    write_file(repository_root / ".gitignore", "*.ignored\n");

    git_index* index = nullptr;
    REQUIRE(git_repository_index(&index, repository) == 0);
    REQUIRE(git_index_add_bypath(index, "tracked.txt") == 0);
    REQUIRE(git_index_add_bypath(index, "modify-me.txt") == 0);
    REQUIRE(git_index_add_bypath(index, "delete-me.txt") == 0);
    REQUIRE(git_index_add_bypath(index, ".gitignore") == 0);
    REQUIRE(git_index_write(index) == 0);

    git_oid tree_oid{};
    REQUIRE(git_index_write_tree(&tree_oid, index) == 0);

    git_tree* tree = nullptr;
    REQUIRE(git_tree_lookup(&tree, repository, &tree_oid) == 0);

    git_signature* signature = nullptr;
    REQUIRE(git_signature_now(&signature, "Uburu Tests", "tests@uburu.local") == 0);

    git_oid commit_oid{};
    REQUIRE(git_commit_create_v(&commit_oid, repository, "refs/heads/main", signature, signature, nullptr,
                                "initial commit", tree, 0) == 0);
    REQUIRE(git_repository_set_head(repository, "refs/heads/main") == 0);

    git_signature_free(signature);
    git_tree_free(tree);
    git_index_free(index);
    git_repository_free(repository);
    git_libgit2_shutdown();
  }

  void stage_file(const std::filesystem::path& repository_root, std::string_view relative_path)
  {
    git_libgit2_init();

    git_repository* repository = nullptr;
    REQUIRE(git_repository_open(&repository, (repository_root / ".git").string().c_str()) == 0);

    git_index* index = nullptr;
    REQUIRE(git_repository_index(&index, repository) == 0);
    REQUIRE(git_index_add_bypath(index, std::string(relative_path).c_str()) == 0);
    REQUIRE(git_index_write(index) == 0);

    git_index_free(index);
    git_repository_free(repository);
    git_libgit2_shutdown();
  }

  git_index_entry conflict_entry(const git_oid& oid, const char* path, std::uint16_t stage)
  {
    git_index_entry entry{};
    entry.mode = GIT_FILEMODE_BLOB;
    entry.id = oid;
    entry.path = path;
    entry.flags = static_cast<std::uint16_t>(stage << GIT_INDEX_ENTRY_STAGESHIFT);

    return entry;
  }

  void add_index_conflict(const std::filesystem::path& repository_root, const char* relative_path)
  {
    git_libgit2_init();

    git_repository* repository = nullptr;
    REQUIRE(git_repository_open(&repository, (repository_root / ".git").string().c_str()) == 0);

    git_oid ancestor_oid{};
    git_oid ours_oid{};
    git_oid theirs_oid{};
    REQUIRE(git_blob_create_frombuffer(&ancestor_oid, repository, "ancestor\n", 9) == 0);
    REQUIRE(git_blob_create_frombuffer(&ours_oid, repository, "ours\n", 5) == 0);
    REQUIRE(git_blob_create_frombuffer(&theirs_oid, repository, "theirs\n", 7) == 0);

    git_index* index = nullptr;
    REQUIRE(git_repository_index(&index, repository) == 0);

    const auto ancestor = conflict_entry(ancestor_oid, relative_path, 1);
    const auto ours = conflict_entry(ours_oid, relative_path, 2);
    const auto theirs = conflict_entry(theirs_oid, relative_path, 3);

    REQUIRE(git_index_conflict_add(index, &ancestor, &ours, &theirs) == 0);
    REQUIRE(git_index_write(index) == 0);

    git_index_free(index);
    git_repository_free(repository);
    git_libgit2_shutdown();
  }

  void add_linked_worktree(const std::filesystem::path& repository_root,
                           const std::filesystem::path& worktree_root)
  {
    git_libgit2_init();

    git_repository* repository = nullptr;
    REQUIRE(git_repository_open(&repository, (repository_root / ".git").string().c_str()) == 0);

    git_object* head_commit = nullptr;
    REQUIRE(git_revparse_single(&head_commit, repository, "HEAD^{commit}") == 0);

    git_reference* feature_branch = nullptr;
    REQUIRE(git_branch_create(&feature_branch, repository, "feature",
                              reinterpret_cast<const git_commit*>(head_commit), false) == 0);

    git_worktree_add_options options = GIT_WORKTREE_ADD_OPTIONS_INIT;
    options.ref = feature_branch;

    git_worktree* worktree = nullptr;
    REQUIRE(git_worktree_add(&worktree, repository, "feature", worktree_root.string().c_str(), &options) == 0);

    git_worktree_free(worktree);
    git_reference_free(feature_branch);
    git_object_free(head_commit);
    git_repository_free(repository);
    git_libgit2_shutdown();
  }

  void detach_head(const std::filesystem::path& repository_root)
  {
    git_libgit2_init();

    git_repository* repository = nullptr;
    REQUIRE(git_repository_open(&repository, (repository_root / ".git").string().c_str()) == 0);

    git_oid head_oid{};
    REQUIRE(git_reference_name_to_id(&head_oid, repository, "HEAD") == 0);
    REQUIRE(git_repository_set_head_detached(repository, &head_oid) == 0);

    git_repository_free(repository);
    git_libgit2_shutdown();
  }

#endif

} // namespace

TEST_CASE("libgit2 git service reports typed error outside repositories")
{
  TemporaryDirectory directory("uburu-libgit2-git-service-not-repo-test");
  const uburu::git::Libgit2GitService service;

  const auto result = service.discover_repository(directory.path());
  const auto* error = std::get_if<uburu::git::GitError>(&result);

  REQUIRE(error != nullptr);
  CHECK(error->code == uburu::git::GitErrorCode::not_repository);
}

TEST_CASE("libgit2 git service discovers repository metadata")
{
#if defined(UBURU_HAS_LIBGIT2)
  TemporaryDirectory directory("uburu-libgit2-git-service-discover-test");
  create_initial_commit(directory.path());

  const uburu::git::Libgit2GitService service;
  const auto result = service.discover_repository(directory.path() / "tracked.txt");
  const auto* repository = std::get_if<uburu::RepositoryInfo>(&result);

  REQUIRE(repository != nullptr);
  CHECK_FALSE(repository->id.empty());
  REQUIRE(repository->worktree_root.has_value());
  CHECK(std::filesystem::equivalent(*repository->worktree_root, directory.path()));
  CHECK_FALSE(repository->head_oid.empty());
  CHECK_FALSE(repository->detached_head);
#else
  SUCCEED("libgit2 is not available in this build");
#endif
}

TEST_CASE("libgit2 git service lists the main worktree")
{
#if defined(UBURU_HAS_LIBGIT2)
  TemporaryDirectory directory("uburu-libgit2-git-service-worktree-test");
  create_initial_commit(directory.path());

  const uburu::git::Libgit2GitService service;
  const auto repository_result = service.discover_repository(directory.path());
  const auto& repository = std::get<uburu::RepositoryInfo>(repository_result);
  const auto worktrees_result = service.list_worktrees(repository);
  const auto* worktrees = std::get_if<std::vector<uburu::WorktreeInfo>>(&worktrees_result);

  REQUIRE(worktrees != nullptr);
  REQUIRE_FALSE(worktrees->empty());
  CHECK(std::filesystem::equivalent(worktrees->front().root, directory.path()));
  CHECK(worktrees->front().repository_id == repository.id);
#else
  SUCCEED("libgit2 is not available in this build");
#endif
}

TEST_CASE("libgit2 git service enumerates linked worktrees")
{
#if defined(UBURU_HAS_LIBGIT2)
  TemporaryDirectory directory("uburu-libgit2-git-service-linked-worktree-test");
  const auto repository_root = directory.path() / "repo";
  const auto linked_root = directory.path() / "feature-worktree";

  std::filesystem::create_directories(repository_root);
  create_initial_commit(repository_root);
  add_linked_worktree(repository_root, linked_root);

  const uburu::git::Libgit2GitService service;
  const auto repository_result = service.discover_repository(repository_root);
  const auto& repository = std::get<uburu::RepositoryInfo>(repository_result);
  const auto worktrees_result = service.list_worktrees(repository);
  const auto& worktrees = std::get<std::vector<uburu::WorktreeInfo>>(worktrees_result);

  REQUIRE(worktrees.size() == 2);

  const auto has_main_worktree = std::ranges::any_of(worktrees, [&](const auto& worktree) {
    return std::filesystem::equivalent(worktree.root, repository_root);
  });
  const auto has_linked_worktree = std::ranges::any_of(worktrees, [&](const auto& worktree) {
    return std::filesystem::equivalent(worktree.root, linked_root);
  });

  CHECK(has_main_worktree);
  CHECK(has_linked_worktree);
#else
  SUCCEED("libgit2 is not available in this build");
#endif
}

TEST_CASE("libgit2 git service detects detached HEAD")
{
#if defined(UBURU_HAS_LIBGIT2)
  TemporaryDirectory directory("uburu-libgit2-git-service-detached-test");
  create_initial_commit(directory.path());
  detach_head(directory.path());

  const uburu::git::Libgit2GitService service;
  const auto result = service.discover_repository(directory.path());
  const auto* repository = std::get_if<uburu::RepositoryInfo>(&result);

  REQUIRE(repository != nullptr);
  CHECK(repository->detached_head);
  CHECK_FALSE(repository->current_branch.has_value());
  CHECK_FALSE(repository->head_oid.empty());

  const auto worktrees_result = service.list_worktrees(*repository);
  const auto& worktree = std::get<std::vector<uburu::WorktreeInfo>>(worktrees_result).front();
  const auto state = service.change_state(worktree);
  const auto& change_state = std::get<uburu::git::GitChangeState>(state);

  CHECK(change_state.detached_head);
  CHECK_FALSE(change_state.branch.has_value());
  CHECK_FALSE(change_state.head_signature.empty());
#else
  SUCCEED("libgit2 is not available in this build");
#endif
}

TEST_CASE("libgit2 git service reads file status and blob hashes")
{
#if defined(UBURU_HAS_LIBGIT2)
  TemporaryDirectory directory("uburu-libgit2-git-service-status-test");
  create_initial_commit(directory.path());

  const uburu::git::Libgit2GitService service;
  const auto repository_result = service.discover_repository(directory.path());
  const auto& repository = std::get<uburu::RepositoryInfo>(repository_result);
  const auto worktrees_result = service.list_worktrees(repository);
  const auto& worktree = std::get<std::vector<uburu::WorktreeInfo>>(worktrees_result).front();

  const auto clean = service.file_status(worktree, "tracked.txt");
  CHECK(std::get<uburu::GitFileStatus>(clean) == uburu::GitFileStatus::clean);

  const auto blob = service.blob_hash(worktree, "tracked.txt");
  const auto& blob_hash = std::get<std::optional<std::string>>(blob);

  REQUIRE(blob_hash.has_value());
  CHECK_FALSE(blob_hash->empty());

  const auto state_before_index_change = service.change_state(worktree);
  const auto& before_index_change = std::get<uburu::git::GitChangeState>(state_before_index_change);

  write_file(directory.path() / "modify-me.txt", "modified\n");
  write_file(directory.path() / "new.txt", "new\n");
  write_file(directory.path() / "staged.txt", "staged\n");
  write_file(directory.path() / "ignored.ignored", "ignored\n");
  std::filesystem::remove(directory.path() / "delete-me.txt");
  stage_file(directory.path(), "staged.txt");
  add_index_conflict(directory.path(), "tracked.txt");

  const auto modified = service.file_status(worktree, "modify-me.txt");
  const auto untracked = service.file_status(worktree, "new.txt");
  const auto added = service.file_status(worktree, "staged.txt");
  const auto deleted = service.file_status(worktree, "delete-me.txt");
  const auto ignored = service.file_status(worktree, "ignored.ignored");

  const auto conflicted = service.file_status(worktree, "tracked.txt");
  const auto state_after_index_change = service.change_state(worktree);
  const auto& after_index_change = std::get<uburu::git::GitChangeState>(state_after_index_change);

  CHECK(std::get<uburu::GitFileStatus>(modified) == uburu::GitFileStatus::modified);
  CHECK(std::get<uburu::GitFileStatus>(conflicted) == uburu::GitFileStatus::conflicted);
  CHECK(std::get<uburu::GitFileStatus>(untracked) == uburu::GitFileStatus::untracked);
  CHECK(std::get<uburu::GitFileStatus>(added) == uburu::GitFileStatus::added);
  CHECK(std::get<uburu::GitFileStatus>(deleted) == uburu::GitFileStatus::deleted);
  CHECK(std::get<uburu::GitFileStatus>(ignored) == uburu::GitFileStatus::ignored);
  CHECK(before_index_change.index_signature != after_index_change.index_signature);
#else
  SUCCEED("libgit2 is not available in this build");
#endif
}
