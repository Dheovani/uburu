#include "core/storage/sqlite-storage-service.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
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
    return uburu::RepositoryInfo{
      .id = "repository-id",
      .commonGitDirectory = root / ".git",
      .worktreeRoot = root,
      .currentBranch = "main",
      .headOid = "abc123",
      .detachedHead = false};
  }

  [[nodiscard]] uburu::WorktreeInfo worktreeInfo(const std::filesystem::path& root)
  {
    return uburu::WorktreeInfo{
      .id = "worktree-id",
      .repositoryId = "repository-id",
      .root = root,
      .gitDirectory = root / ".git",
      .branch = "main",
      .headOid = "abc123",
      .locked = false,
      .prunable = false,
      .lockReason = {}};
  }

  [[nodiscard]] uburu::IndexDocument indexDocument(std::string contentHash,
                                                   std::filesystem::path relativePath = "src/main.cpp")
  {
    return uburu::IndexDocument{
      .repositoryId = "repository-id",
      .worktreeId = "worktree-id",
      .relativePath = std::move(relativePath),
      .contentHash = std::move(contentHash),
      .contentHashAlgorithm = uburu::ContentHashAlgorithm::sha256,
      .gitBlobHash = "blob123",
      .gitBlobHashAlgorithm = uburu::GitObjectHashAlgorithm::sha1,
      .status = uburu::GitFileStatus::clean,
      .size = 42,
      .indexedAt = std::chrono::system_clock::time_point{std::chrono::milliseconds{1234}},
      .deleted = false};
  }

  [[nodiscard]] uburu::IndexGeneration indexGeneration(std::vector<uburu::IndexDocument> documents)
  {
    return uburu::IndexGeneration{
      .repositoryId = "repository-id",
      .worktreeId = "worktree-id",
      .headOid = "abc123",
      .branch = "main",
      .createdAt = std::chrono::system_clock::time_point{std::chrono::milliseconds{5678}},
      .documents = std::move(documents)};
  }

} // namespace

TEST_CASE("sqlite storage persists repositories worktrees and documents")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-sqlite-storage-persist-test");
  const auto databasePath = directory.path() / "uburu.db";
  const auto repository = repositoryInfo(directory.path());
  const auto worktree = worktreeInfo(directory.path());

  {
    uburu::storage::SQLiteStorageService storage(databasePath);
    storage.initialize();
    storage.initialize();
    storage.upsertRepository(repository);
    storage.upsertWorktree(worktree);
    storage.upsertDocument(indexDocument("content123"));
  }

  uburu::storage::SQLiteStorageService reopened(databasePath);
  reopened.initialize();

  const auto document = reopened.findDocument("worktree-id", "src/main.cpp");

  REQUIRE(document.has_value());
  CHECK(document->repositoryId == "repository-id");
  CHECK(document->worktreeId == "worktree-id");
  CHECK(document->relativePath == std::filesystem::path("src/main.cpp"));
  CHECK(document->contentHash == "content123");
  CHECK(document->contentHashAlgorithm == uburu::ContentHashAlgorithm::sha256);
  REQUIRE(document->gitBlobHash.has_value());
  CHECK(*document->gitBlobHash == "blob123");
  CHECK(document->gitBlobHashAlgorithm == uburu::GitObjectHashAlgorithm::sha1);
  CHECK(document->status == uburu::GitFileStatus::clean);
  CHECK(document->size == 42);
  CHECK(document->indexedAt == std::chrono::system_clock::time_point{std::chrono::milliseconds{1234}});
  CHECK_FALSE(document->deleted);
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("sqlite storage updates and logically removes indexed files")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-sqlite-storage-remove-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  storage.initialize();
  storage.upsertRepository(repositoryInfo(directory.path()));
  storage.upsertWorktree(worktreeInfo(directory.path()));
  storage.upsertDocument(indexDocument("content-before"));

  auto updated = indexDocument("content-after");
  updated.size = 84;
  storage.upsertDocument(updated);
  storage.removeDocument("worktree-id", "src/main.cpp");

  const auto document = storage.findDocument("worktree-id", "src/main.cpp");

  REQUIRE(document.has_value());
  CHECK(document->contentHash == "content-after");
  CHECK(document->size == 84);
  CHECK(document->status == uburu::GitFileStatus::deleted);
  CHECK(document->deleted);
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("sqlite storage publishes generations atomically")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-sqlite-storage-generation-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  storage.initialize();
  storage.upsertRepository(repositoryInfo(directory.path()));
  storage.upsertWorktree(worktreeInfo(directory.path()));

  storage.publishGeneration(indexGeneration({
    indexDocument("content-a", "src/a.cpp"),
    indexDocument("content-b", "src/b.cpp"),
  }));

  REQUIRE(storage.findDocument("worktree-id", "src/a.cpp").has_value());
  REQUIRE(storage.findDocument("worktree-id", "src/b.cpp").has_value());

  storage.publishGeneration(indexGeneration({
    indexDocument("content-a-updated", "src/a.cpp"),
  }));

  const auto updated = storage.findDocument("worktree-id", "src/a.cpp");
  const auto removed = storage.findDocument("worktree-id", "src/b.cpp");

  REQUIRE(updated.has_value());
  CHECK(updated->contentHash == "content-a-updated");
  CHECK_FALSE(removed.has_value());
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("sqlite storage treats hash algorithm as part of document identity")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-sqlite-storage-hash-identity-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  auto first = indexDocument("same-hash-text", "src/sha256.cpp");
  first.contentHashAlgorithm = uburu::ContentHashAlgorithm::sha256;
  first.size = 10;

  auto second = indexDocument("same-hash-text", "src/unknown.cpp");
  second.contentHashAlgorithm = uburu::ContentHashAlgorithm::unknown;
  second.size = 20;

  storage.initialize();
  storage.upsertRepository(repositoryInfo(directory.path()));
  storage.upsertWorktree(worktreeInfo(directory.path()));
  storage.publishGeneration(indexGeneration({
    first,
    second,
  }));

  const auto sha256Document = storage.findDocument("worktree-id", "src/sha256.cpp");
  const auto unknownDocument = storage.findDocument("worktree-id", "src/unknown.cpp");

  REQUIRE(sha256Document.has_value());
  REQUIRE(unknownDocument.has_value());
  CHECK(sha256Document->contentHash == unknownDocument->contentHash);
  CHECK(sha256Document->contentHashAlgorithm == uburu::ContentHashAlgorithm::sha256);
  CHECK(unknownDocument->contentHashAlgorithm == uburu::ContentHashAlgorithm::unknown);
  CHECK(sha256Document->size == 10);
  CHECK(unknownDocument->size == 20);
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("sqlite storage rolls back invalid generation publication")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-sqlite-storage-generation-rollback-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  storage.initialize();
  storage.upsertRepository(repositoryInfo(directory.path()));
  storage.upsertWorktree(worktreeInfo(directory.path()));
  storage.publishGeneration(indexGeneration({
    indexDocument("content-before", "src/a.cpp"),
  }));

  auto invalidDocument = indexDocument("content-invalid", "src/invalid.cpp");
  invalidDocument.worktreeId = "another-worktree";

  CHECK_THROWS(storage.publishGeneration(indexGeneration({
    invalidDocument,
  })));

  const auto preserved = storage.findDocument("worktree-id", "src/a.cpp");
  const auto invalid = storage.findDocument("worktree-id", "src/invalid.cpp");

  REQUIRE(preserved.has_value());
  CHECK(preserved->contentHash == "content-before");
  CHECK_FALSE(invalid.has_value());
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("sqlite storage returns empty for unknown indexed files")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-sqlite-storage-missing-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  storage.initialize();

  const auto document = storage.findDocument("missing-worktree", "missing.cpp");

  CHECK_FALSE(document.has_value());
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("sqlite storage reports use before initialization")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-sqlite-storage-uninitialized-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  CHECK_THROWS(storage.findDocument("worktree-id", "src/main.cpp"));
#else
  SUCCEED("SQLite is not available in this build");
#endif
}
