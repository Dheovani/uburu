#include "core/git/libgit2-git-service.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
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

    git_index* index = nullptr;
    REQUIRE(git_repository_index(&index, repository) == 0);
    REQUIRE(git_index_add_bypath(index, "tracked.txt") == 0);
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

  write_file(directory.path() / "tracked.txt", "modified\n");
  write_file(directory.path() / "new.txt", "new\n");

  const auto modified = service.file_status(worktree, "tracked.txt");
  const auto untracked = service.file_status(worktree, "new.txt");

  CHECK(std::get<uburu::GitFileStatus>(modified) == uburu::GitFileStatus::modified);
  CHECK(std::get<uburu::GitFileStatus>(untracked) == uburu::GitFileStatus::untracked);
#else
  SUCCEED("libgit2 is not available in this build");
#endif
}
