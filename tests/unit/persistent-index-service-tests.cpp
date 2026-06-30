#include "core/index/persistent-index-service.hpp"
#include "core/storage/sqlite-storage-service.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

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

  [[nodiscard]] uburu::RepositoryInfo repositoryInfo(const std::filesystem::path& root)
  {
    return uburu::RepositoryInfo{.id = "repository-id",
                                 .commonGitDirectory = root / ".git",
                                 .worktreeRoot = root,
                                 .currentBranch = "main",
                                 .headOid = "abc123",
                                 .detachedHead = false};
  }

  [[nodiscard]] uburu::WorktreeInfo worktreeInfo(const std::filesystem::path& root)
  {
    return uburu::WorktreeInfo{.id = "worktree-id",
                               .repositoryId = "repository-id",
                               .root = root,
                               .gitDirectory = root / ".git",
                               .branch = "main",
                               .headOid = "abc123",
                               .locked = false,
                               .prunable = false,
                               .lockReason = {}};
  }

  void writeFile(const std::filesystem::path& path, std::string_view content)
  {
    std::filesystem::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::binary);
    file << content;
  }

  [[nodiscard]] uburu::FileEntry fileEntry(const std::filesystem::path& root, const std::filesystem::path& relativePath)
  {
    const auto absolutePath = root / relativePath;

    return uburu::FileEntry{.absolutePath = absolutePath,
                            .relativePath = relativePath,
                            .size = std::filesystem::file_size(absolutePath),
                            .modifiedAt = std::filesystem::last_write_time(absolutePath),
                            .searchRoot = root};
  }

} // namespace

TEST_CASE("persistent index service publishes an initial generation with content hashes")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-persistent-index-initial-test");
  const auto root = directory.path() / "repo";

  writeFile(root / "src" / "a.txt", "same content");
  writeFile(root / "src" / "b.txt", "same content");

  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");
  storage.initialize();
  storage.upsertRepository(repositoryInfo(root));
  storage.upsertWorktree(worktreeInfo(root));

  uburu::index::PersistentIndexService indexService(storage);
  std::vector<uburu::index::IndexUpdateProgress> progressEvents;
  const std::vector files{
    fileEntry(root, "src/a.txt"),
    fileEntry(root, "src/b.txt"),
  };

  const auto summary =
    indexService.update(worktreeInfo(root), files, [&](const auto& progress) { progressEvents.push_back(progress); });

  const auto first = storage.findDocument("worktree-id", "src/a.txt");
  const auto second = storage.findDocument("worktree-id", "src/b.txt");

  CHECK_FALSE(summary.cancelled);
  CHECK(summary.failed == 0);
  CHECK(summary.indexed == 1);
  CHECK(summary.reusedByHash == 1);
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());
  CHECK(first->contentHash == second->contentHash);
  CHECK(first->contentHashAlgorithm == uburu::ContentHashAlgorithm::sha256);
  CHECK(second->contentHashAlgorithm == uburu::ContentHashAlgorithm::sha256);
  REQUIRE(progressEvents.size() == 2);
  CHECK(progressEvents.back().processed == 2);
  CHECK(progressEvents.back().total == 2);
  CHECK(progressEvents.back().indexed == 1);
  CHECK(progressEvents.back().reusedByHash == 1);
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("persistent index service observes cancellation before publishing")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-persistent-index-cancel-test");
  const auto root = directory.path() / "repo";
  std::stop_source stopSource;

  writeFile(root / "src" / "a.txt", "content");

  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");
  storage.initialize();
  storage.upsertRepository(repositoryInfo(root));
  storage.upsertWorktree(worktreeInfo(root));

  uburu::index::PersistentIndexService indexService(storage);
  const std::vector files{
    fileEntry(root, "src/a.txt"),
  };

  stopSource.request_stop();

  const auto summary = indexService.update(worktreeInfo(root), files, {}, stopSource.get_token());
  const auto document = storage.findDocument("worktree-id", "src/a.txt");

  CHECK(summary.cancelled);
  CHECK(summary.indexed == 0);
  CHECK(summary.reusedByHash == 0);
  CHECK_FALSE(document.has_value());
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("persistent index service reports unreadable files without aborting the generation")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-persistent-index-failure-test");
  const auto root = directory.path() / "repo";

  writeFile(root / "src" / "valid.txt", "valid");

  auto missing = fileEntry(root, "src/valid.txt");
  missing.absolutePath = root / "src" / "missing.txt";
  missing.relativePath = "src/missing.txt";

  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");
  storage.initialize();
  storage.upsertRepository(repositoryInfo(root));
  storage.upsertWorktree(worktreeInfo(root));

  uburu::index::PersistentIndexService indexService(storage);
  const std::vector files{
    fileEntry(root, "src/valid.txt"),
    missing,
  };

  const auto summary = indexService.update(worktreeInfo(root), files);
  const auto valid = storage.findDocument("worktree-id", "src/valid.txt");
  const auto invalid = storage.findDocument("worktree-id", "src/missing.txt");

  CHECK_FALSE(summary.cancelled);
  CHECK(summary.indexed == 1);
  CHECK(summary.failed == 1);
  REQUIRE(valid.has_value());
  CHECK_FALSE(invalid.has_value());
#else
  SUCCEED("SQLite is not available in this build");
#endif
}
