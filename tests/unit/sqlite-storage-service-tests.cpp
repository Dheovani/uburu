#include "core/storage/sqlite-storage-service.hpp"
#include "core/storage/storage-paths.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if defined(UBURU_HAS_SQLITE)
#include <sqlite3.h>
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

  [[nodiscard]] uburu::IndexDocument indexDocument(std::string contentHash,
                                                   std::filesystem::path relativePath = "src/main.cpp")
  {
    uburu::IndexDocument document;
    document.repositoryId = "repository-id";
    document.worktreeId = "worktree-id";
    document.relativePath = std::move(relativePath);
    document.contentHash = std::move(contentHash);
    document.contentHashAlgorithm = uburu::ContentHashAlgorithm::sha256;
    document.gitBlobHash = "blob123";
    document.gitBlobHashAlgorithm = uburu::GitObjectHashAlgorithm::sha1;
    document.status = uburu::GitFileStatus::clean;
    document.size = 42;
    document.modifiedAt = std::filesystem::file_time_type{std::chrono::seconds{4}};
    document.indexedAt = std::chrono::system_clock::time_point{std::chrono::milliseconds{1234}};

    return document;
  }

  [[nodiscard]] uburu::IndexGeneration indexGeneration(std::vector<uburu::IndexDocument> documents)
  {
    return uburu::IndexGeneration{.repositoryId = "repository-id",
                                  .worktreeId = "worktree-id",
                                  .headOid = "abc123",
                                  .branch = "main",
                                  .createdAt = std::chrono::system_clock::time_point{std::chrono::milliseconds{5678}},
                                  .documents = std::move(documents)};
  }

#if defined(UBURU_HAS_SQLITE)

  void executeSql(sqlite3* database, std::string_view sql)
  {
    char* errorMessage = nullptr;
    const auto result = sqlite3_exec(database, std::string(sql).c_str(), nullptr, nullptr, &errorMessage);

    if (result == SQLITE_OK)
      return;

    std::string message = "SQLite test helper failed";
    if (errorMessage != nullptr) {
      message += ": ";
      message += errorMessage;
    }

    sqlite3_free(errorMessage);
    FAIL(message);
  }

  void insertIncompleteGeneration(const std::filesystem::path& databasePath)
  {
    sqlite3* database = nullptr;
    REQUIRE(sqlite3_open_v2(databasePath.string().c_str(), &database, SQLITE_OPEN_READWRITE, nullptr) == SQLITE_OK);

    executeSql(database, R"sql(
      INSERT INTO generations (
        repository_id, worktree_id, head_oid, branch, created_at_unix_ms, published
      ) VALUES ('repository-id', 'worktree-id', 'interrupted', 'main', 1, 0);
    )sql");

    sqlite3_close(database);
  }

  void createLegacyVersionSixDatabase(const std::filesystem::path& databasePath)
  {
    sqlite3* database = nullptr;
    REQUIRE(
      sqlite3_open_v2(databasePath.string().c_str(), &database, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) ==
      SQLITE_OK);

    executeSql(database, R"sql(
      PRAGMA foreign_keys = ON;

      CREATE TABLE schema_migrations (
        version INTEGER PRIMARY KEY,
        applied_at_unix_ms INTEGER NOT NULL
      );

      CREATE TABLE repositories (
        id TEXT PRIMARY KEY,
        common_git_directory TEXT NOT NULL,
        worktree_root TEXT,
        current_branch TEXT,
        head_oid TEXT NOT NULL,
        detached_head INTEGER NOT NULL
      );

      CREATE TABLE worktrees (
        id TEXT PRIMARY KEY,
        repository_id TEXT NOT NULL REFERENCES repositories(id) ON DELETE CASCADE,
        root TEXT NOT NULL,
        git_directory TEXT NOT NULL,
        branch TEXT,
        head_oid TEXT NOT NULL,
        locked INTEGER NOT NULL,
        prunable INTEGER NOT NULL,
        lock_reason TEXT NOT NULL
      );

      CREATE TABLE generations (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        repository_id TEXT NOT NULL REFERENCES repositories(id) ON DELETE CASCADE,
        worktree_id TEXT NOT NULL REFERENCES worktrees(id) ON DELETE CASCADE,
        head_oid TEXT NOT NULL,
        branch TEXT,
        created_at_unix_ms INTEGER NOT NULL,
        published INTEGER NOT NULL
      );

      CREATE TABLE documents (
        content_hash TEXT NOT NULL,
        content_hash_algorithm INTEGER NOT NULL,
        git_blob_hash TEXT,
        git_blob_hash_algorithm INTEGER NOT NULL DEFAULT 0,
        size INTEGER NOT NULL,
        indexed_at_unix_ms INTEGER NOT NULL,
        format_version INTEGER NOT NULL DEFAULT 1,
        PRIMARY KEY (content_hash_algorithm, content_hash)
      );

      CREATE TABLE files (
        worktree_id TEXT NOT NULL REFERENCES worktrees(id) ON DELETE CASCADE,
        relative_path TEXT NOT NULL,
        repository_id TEXT NOT NULL REFERENCES repositories(id) ON DELETE CASCADE,
        content_hash TEXT NOT NULL,
        content_hash_algorithm INTEGER NOT NULL,
        format_version INTEGER NOT NULL DEFAULT 1,
        git_blob_hash TEXT,
        git_blob_hash_algorithm INTEGER NOT NULL DEFAULT 0,
        status INTEGER NOT NULL,
        size INTEGER NOT NULL,
        file_modified_at_ticks INTEGER NOT NULL DEFAULT 0,
        indexed_at_unix_ms INTEGER NOT NULL,
        deleted INTEGER NOT NULL,
        PRIMARY KEY (worktree_id, relative_path),
        FOREIGN KEY (content_hash_algorithm, content_hash)
          REFERENCES documents(content_hash_algorithm, content_hash)
      );

      CREATE TABLE overlays (
        worktree_id TEXT NOT NULL REFERENCES worktrees(id) ON DELETE CASCADE,
        relative_path TEXT NOT NULL,
        previous_relative_path TEXT,
        status INTEGER NOT NULL,
        disposition INTEGER NOT NULL,
        reusable_blob_algorithm INTEGER,
        reusable_blob_value TEXT,
        PRIMARY KEY (worktree_id, relative_path)
      );

      CREATE TABLE preferences (
        scope_id TEXT NOT NULL,
        key TEXT NOT NULL,
        value TEXT NOT NULL,
        updated_at_unix_ms INTEGER NOT NULL,
        PRIMARY KEY (scope_id, key)
      );

      CREATE TABLE search_history (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        root TEXT NOT NULL,
        expression TEXT NOT NULL,
        searched_at_unix_ms INTEGER NOT NULL
      );

      CREATE TABLE saved_searches (
        name TEXT PRIMARY KEY,
        root TEXT NOT NULL,
        expression TEXT NOT NULL,
        saved_at_unix_ms INTEGER NOT NULL
      );

      CREATE TABLE indexing_metrics (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT NOT NULL,
        value INTEGER NOT NULL,
        recorded_at_unix_ms INTEGER NOT NULL
      );

      INSERT INTO repositories (
        id, common_git_directory, worktree_root, current_branch, head_oid, detached_head
      ) VALUES ('repository-id', 'legacy-root/.git', 'legacy-root', 'main', 'abc123', 0);

      INSERT INTO worktrees (
        id, repository_id, root, git_directory, branch, head_oid, locked, prunable, lock_reason
      ) VALUES ('worktree-id', 'repository-id', 'legacy-root', 'legacy-root/.git', 'main', 'abc123', 0, 0, '');

      INSERT INTO documents (
        content_hash, content_hash_algorithm, git_blob_hash, git_blob_hash_algorithm, size, indexed_at_unix_ms,
        format_version
      ) VALUES ('legacy-content', 1, 'legacy-blob', 1, 42, 1234, 1);

      INSERT INTO files (
        worktree_id, relative_path, repository_id, content_hash, content_hash_algorithm, format_version, git_blob_hash,
        git_blob_hash_algorithm, status, size, file_modified_at_ticks, indexed_at_unix_ms, deleted
      ) VALUES (
        'worktree-id', 'src/legacy.cpp', 'repository-id', 'legacy-content', 1, 1, 'legacy-blob', 1, 0, 42, 4, 1234, 0
      );

      PRAGMA user_version = 6;
    )sql");

    sqlite3_close(database);
  }

  [[nodiscard]] int unpublishedGenerationCount(const std::filesystem::path& databasePath)
  {
    sqlite3* database = nullptr;
    REQUIRE(sqlite3_open_v2(databasePath.string().c_str(), &database, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK);

    sqlite3_stmt* statement = nullptr;
    REQUIRE(sqlite3_prepare_v2(
              database, "SELECT COUNT(*) FROM generations WHERE published = 0", -1, &statement, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(statement) == SQLITE_ROW);

    const auto count = sqlite3_column_int(statement, 0);

    sqlite3_finalize(statement);
    sqlite3_close(database);

    return count;
  }

  [[nodiscard]] std::int64_t integerScalar(const std::filesystem::path& databasePath, std::string_view sql)
  {
    sqlite3* database = nullptr;
    sqlite3_stmt* statement = nullptr;
    REQUIRE(sqlite3_open_v2(databasePath.string().c_str(), &database, SQLITE_OPEN_READONLY, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_prepare_v2(database, std::string(sql).c_str(), -1, &statement, nullptr) == SQLITE_OK);
    REQUIRE(sqlite3_step(statement) == SQLITE_ROW);

    const auto value = sqlite3_column_int64(statement, 0);

    sqlite3_finalize(statement);
    sqlite3_close(database);

    return value;
  }

#endif

} // namespace

TEST_CASE("sqlite storage persists repositories worktrees and documents")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-sqlite-storage-persist-test");
  const auto databasePath = directory.path() / "uburu.db";
  const auto repository = repositoryInfo(directory.path());
  const auto worktree = worktreeInfo(directory.path());

  {
    auto document = indexDocument("content123");
    document.indexedText = "indexed\ncontent";
    REQUIRE(document.modifiedAt == std::filesystem::file_time_type{std::chrono::seconds{4}});
    REQUIRE(document.modifiedAt.time_since_epoch().count() != 0);

    uburu::storage::SQLiteStorageService storage(databasePath);
    storage.initialize();
    storage.initialize();
    storage.upsertRepository(repository);
    storage.upsertWorktree(worktree);
    storage.upsertDocument(document);

    REQUIRE(
      integerScalar(databasePath, "SELECT file_modified_at_ticks FROM files WHERE relative_path = 'src/main.cpp'") ==
      document.modifiedAt.time_since_epoch().count());
  }

  uburu::storage::SQLiteStorageService reopened(databasePath);
  reopened.initialize();

  const auto document = reopened.findDocument("worktree-id", "src/main.cpp");

  REQUIRE(document.has_value());
  CHECK(document->formatVersion == uburu::latestIndexDocumentFormatVersion);
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
  CHECK(document->modifiedAt == std::filesystem::file_time_type{std::chrono::seconds{4}});
  CHECK(document->indexedAt == std::chrono::system_clock::time_point{std::chrono::milliseconds{1234}});
  CHECK_FALSE(document->deleted);
  REQUIRE(document->indexedText.has_value());
  CHECK(*document->indexedText == "indexed\ncontent");
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("sqlite storage migrates v6 documents to indexed text schema without dropping catalog entries")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-sqlite-storage-v6-migration-test");
  const auto databasePath = directory.path() / "uburu.db";

  createLegacyVersionSixDatabase(databasePath);

  {
    uburu::storage::SQLiteStorageService storage(databasePath);
    storage.initialize();

    const auto document = storage.findDocument("worktree-id", "src/legacy.cpp");

    REQUIRE(document.has_value());
    CHECK(document->contentHash == "legacy-content");
    CHECK(document->contentHashAlgorithm == uburu::ContentHashAlgorithm::sha256);
    CHECK_FALSE(document->indexedText.has_value());
  }

  CHECK(integerScalar(databasePath, "PRAGMA user_version") == 7);
  CHECK(integerScalar(databasePath,
                      "SELECT COUNT(*) FROM pragma_table_info('documents') WHERE name = 'indexed_text'") == 1);
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("sqlite storage reports configured pragmas and integrity")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-sqlite-storage-pragmas-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  storage.initialize();

  const auto pragmas = storage.pragmaSnapshot();
  const auto integrity = storage.validateIntegrity();

  CHECK(pragmas.foreignKeysEnabled);
  CHECK(pragmas.journalMode == "wal");
  CHECK(pragmas.synchronousMode == "NORMAL");
  CHECK(pragmas.busyTimeout == std::chrono::milliseconds{5000});
  CHECK(integrity.ok);
  CHECK(integrity.message == "ok");
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

TEST_CASE("sqlite storage rebuilds index catalog without removing metadata")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-sqlite-storage-rebuild-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  storage.initialize();
  storage.upsertRepository(repositoryInfo(directory.path()));
  storage.upsertWorktree(worktreeInfo(directory.path()));
  storage.setPreference(std::nullopt, "theme", "dark");
  storage.publishGeneration(indexGeneration({
    indexDocument("content-a", "src/a.cpp"),
  }));

  REQUIRE(storage.findDocument("worktree-id", "src/a.cpp").has_value());

  storage.rebuildIndexCatalog();

  CHECK_FALSE(storage.findDocument("worktree-id", "src/a.cpp").has_value());
  CHECK(storage.preference(std::nullopt, "theme") == "dark");
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

TEST_CASE("sqlite storage lists only visible indexed documents for a worktree root")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-sqlite-storage-visible-documents-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");
  auto deleted = indexDocument("deleted-content", "src/deleted.cpp");

  deleted.deleted = true;
  deleted.status = uburu::GitFileStatus::deleted;

  storage.initialize();
  storage.upsertRepository(repositoryInfo(directory.path()));
  storage.upsertWorktree(worktreeInfo(directory.path()));
  storage.publishGeneration(indexGeneration({
    indexDocument("visible-content", "src/visible.cpp"),
    deleted,
  }));

  const auto documents = storage.visibleDocumentsForRoot(directory.path());

  REQUIRE(documents.size() == 1);
  CHECK(documents.front().relativePath == std::filesystem::path("src/visible.cpp"));
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("sqlite storage returns the latest published generation for a worktree root")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-sqlite-storage-latest-generation-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  storage.initialize();
  storage.upsertRepository(repositoryInfo(directory.path()));
  storage.upsertWorktree(worktreeInfo(directory.path()));

  auto first = indexGeneration({
    indexDocument("content-a", "src/a.cpp"),
  });
  first.headOid = "head-a";
  first.createdAt = std::chrono::system_clock::time_point{std::chrono::milliseconds{10}};

  auto second = indexGeneration({
    indexDocument("content-b", "src/b.cpp"),
  });
  second.headOid = "head-b";
  second.branch = "feature";
  second.createdAt = std::chrono::system_clock::time_point{std::chrono::milliseconds{20}};

  storage.publishGeneration(first);
  storage.publishGeneration(second);

  const auto latest = storage.latestGenerationForRoot(directory.path());
  const auto missing = storage.latestGenerationForRoot(directory.path() / "missing");

  REQUIRE(latest.has_value());
  CHECK(latest->repositoryId == "repository-id");
  CHECK(latest->worktreeId == "worktree-id");
  CHECK(latest->headOid == "head-b");
  CHECK(latest->branch == "feature");
  CHECK_FALSE(missing.has_value());
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("sqlite storage persists global and repository preferences")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-sqlite-storage-preferences-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  storage.initialize();
  storage.setPreference(std::nullopt, "language", "pt-BR");
  storage.setPreference("repository-id", "language", "en-US");
  storage.setPreference("repository-id", "language", "pt-BR");

  CHECK(storage.preference(std::nullopt, "language") == "pt-BR");
  CHECK(storage.preference("repository-id", "language") == "pt-BR");
  CHECK_FALSE(storage.preference("other-repository", "language").has_value());
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("sqlite storage persists search history and saved searches")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-sqlite-storage-search-history-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  storage.initialize();
  storage.recordSearch(uburu::SearchHistoryEntry{.root = "repo", .expression = "first", .searchedAt = {}}, 2);
  storage.recordSearch(
    uburu::SearchHistoryEntry{.root = "repo",
                              .expression = "second",
                              .searchedAt = std::chrono::system_clock::time_point{std::chrono::milliseconds{1}}},
    2);
  storage.recordSearch(
    uburu::SearchHistoryEntry{.root = "repo",
                              .expression = "third",
                              .searchedAt = std::chrono::system_clock::time_point{std::chrono::milliseconds{2}}},
    2);
  storage.saveSearch(
    uburu::SavedSearch{.name = "default",
                       .root = "repo",
                       .expression = "needle",
                       .savedAt = std::chrono::system_clock::time_point{std::chrono::milliseconds{3}}});
  storage.saveSearch(
    uburu::SavedSearch{.name = "default",
                       .root = "repo",
                       .expression = "updated",
                       .savedAt = std::chrono::system_clock::time_point{std::chrono::milliseconds{4}}});

  const auto history = storage.recentSearches(10);
  const auto saved = storage.savedSearches();

  REQUIRE(history.size() == 2);
  CHECK(history[0].expression == "third");
  CHECK(history[1].expression == "second");
  REQUIRE(saved.size() == 1);
  CHECK(saved.front().name == "default");
  CHECK(saved.front().expression == "updated");
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("sqlite storage persists indexing metrics with retention")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-sqlite-storage-metrics-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  storage.initialize();
  storage.recordIndexingMetric(
    uburu::IndexingMetric{.name = "files-indexed",
                          .value = 1,
                          .recordedAt = std::chrono::system_clock::time_point{std::chrono::milliseconds{1}}},
    2);
  storage.recordIndexingMetric(
    uburu::IndexingMetric{.name = "files-indexed",
                          .value = 2,
                          .recordedAt = std::chrono::system_clock::time_point{std::chrono::milliseconds{2}}},
    2);
  storage.recordIndexingMetric(
    uburu::IndexingMetric{.name = "files-indexed",
                          .value = 3,
                          .recordedAt = std::chrono::system_clock::time_point{std::chrono::milliseconds{3}}},
    2);
  storage.recordIndexingMetric(
    uburu::IndexingMetric{.name = "bytes-indexed",
                          .value = 99,
                          .recordedAt = std::chrono::system_clock::time_point{std::chrono::milliseconds{4}}},
    2);

  const auto fileMetrics = storage.recentIndexingMetrics("files-indexed", 10);
  const auto byteMetrics = storage.recentIndexingMetrics("bytes-indexed", 10);

  REQUIRE(fileMetrics.size() == 2);
  CHECK(fileMetrics[0].value == 3);
  CHECK(fileMetrics[1].value == 2);
  REQUIRE(byteMetrics.size() == 1);
  CHECK(byteMetrics.front().value == 99);
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("sqlite storage keeps readers and writers on separate connections consistent")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-sqlite-storage-concurrency-test");
  const auto databasePath = directory.path() / "uburu.db";
  uburu::storage::SQLiteStorageService writer(databasePath);
  uburu::storage::SQLiteStorageService reader(databasePath);

  writer.initialize();
  writer.upsertRepository(repositoryInfo(directory.path()));
  writer.upsertWorktree(worktreeInfo(directory.path()));
  writer.publishGeneration(indexGeneration({
    indexDocument("content-before", "src/a.cpp"),
  }));

  reader.initialize();
  REQUIRE(reader.findDocument("worktree-id", "src/a.cpp").has_value());

  writer.publishGeneration(indexGeneration({
    indexDocument("content-after", "src/a.cpp"),
  }));

  const auto document = reader.findDocument("worktree-id", "src/a.cpp");

  REQUIRE(document.has_value());
  CHECK(document->contentHash == "content-after");
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

TEST_CASE("sqlite storage finds reusable documents by content hash")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-sqlite-storage-content-reuse-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  auto document = indexDocument("reusable-content", "src/reusable.cpp");
  document.gitBlobHash = "blob-reusable";
  document.size = 123;

  storage.initialize();
  storage.upsertRepository(repositoryInfo(directory.path()));
  storage.upsertWorktree(worktreeInfo(directory.path()));
  storage.publishGeneration(indexGeneration({
    document,
  }));

  const auto reusable =
    storage.findReusableDocumentByContentHash(uburu::ContentHashAlgorithm::sha256, "reusable-content");
  const auto wrongAlgorithm =
    storage.findReusableDocumentByContentHash(uburu::ContentHashAlgorithm::unknown, "reusable-content");

  REQUIRE(reusable.has_value());
  CHECK(reusable->formatVersion == uburu::latestIndexDocumentFormatVersion);
  CHECK(reusable->contentHash == "reusable-content");
  CHECK(reusable->contentHashAlgorithm == uburu::ContentHashAlgorithm::sha256);
  REQUIRE(reusable->gitBlobHash.has_value());
  CHECK(*reusable->gitBlobHash == "blob-reusable");
  CHECK(reusable->gitBlobHashAlgorithm == uburu::GitObjectHashAlgorithm::sha1);
  CHECK(reusable->size == 123);
  CHECK_FALSE(wrongAlgorithm.has_value());
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("sqlite storage finds reusable documents by git blob hash")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-sqlite-storage-blob-reuse-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  auto older = indexDocument("older-content", "src/older.cpp");
  older.gitBlobHash = "shared-blob";
  older.size = 10;
  older.indexedAt = std::chrono::system_clock::time_point{std::chrono::milliseconds{10}};

  auto newer = indexDocument("newer-content", "src/newer.cpp");
  newer.gitBlobHash = "shared-blob";
  newer.size = 20;
  newer.indexedAt = std::chrono::system_clock::time_point{std::chrono::milliseconds{20}};

  storage.initialize();
  storage.upsertRepository(repositoryInfo(directory.path()));
  storage.upsertWorktree(worktreeInfo(directory.path()));
  storage.publishGeneration(indexGeneration({
    older,
    newer,
  }));

  const auto reusable = storage.findReusableDocumentByGitBlobHash(uburu::GitObjectHashAlgorithm::sha1, "shared-blob");
  const auto wrongAlgorithm =
    storage.findReusableDocumentByGitBlobHash(uburu::GitObjectHashAlgorithm::sha256, "shared-blob");

  REQUIRE(reusable.has_value());
  CHECK(reusable->contentHash == "newer-content");
  CHECK(reusable->contentHashAlgorithm == uburu::ContentHashAlgorithm::sha256);
  REQUIRE(reusable->gitBlobHash.has_value());
  CHECK(*reusable->gitBlobHash == "shared-blob");
  CHECK(reusable->gitBlobHashAlgorithm == uburu::GitObjectHashAlgorithm::sha1);
  CHECK(reusable->size == 20);
  CHECK_FALSE(wrongAlgorithm.has_value());
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

TEST_CASE("sqlite storage recovers incomplete generations on initialization")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-sqlite-storage-recovery-test");
  const auto databasePath = directory.path() / "uburu.db";

  {
    uburu::storage::SQLiteStorageService storage(databasePath);
    storage.initialize();
    storage.upsertRepository(repositoryInfo(directory.path()));
    storage.upsertWorktree(worktreeInfo(directory.path()));
  }

  insertIncompleteGeneration(databasePath);
  REQUIRE(unpublishedGenerationCount(databasePath) == 1);

  uburu::storage::SQLiteStorageService storage(databasePath);
  storage.initialize();

  CHECK(unpublishedGenerationCount(databasePath) == 0);
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("sqlite storage collects orphan documents")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-sqlite-storage-orphan-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  storage.initialize();
  storage.upsertRepository(repositoryInfo(directory.path()));
  storage.upsertWorktree(worktreeInfo(directory.path()));
  storage.publishGeneration(indexGeneration({
    indexDocument("orphan-a", "src/a.cpp"),
    indexDocument("orphan-b", "src/b.cpp"),
  }));
  storage.publishGeneration(indexGeneration({
    indexDocument("live-c", "src/c.cpp"),
  }));

  CHECK(storage.collectOrphanDocuments() == 2);
  CHECK(storage.collectOrphanDocuments() == 0);
  REQUIRE(storage.findDocument("worktree-id", "src/c.cpp").has_value());
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("sqlite storage evicts orphan documents to satisfy the document budget")
{
#if defined(UBURU_HAS_SQLITE)
  constexpr std::uintmax_t orphanDocumentSize = 80;
  constexpr std::uintmax_t liveDocumentSize = 60;
  constexpr std::uintmax_t documentBudget = 70;

  TemporaryDirectory directory("uburu-sqlite-storage-budget-eviction-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  auto orphanDocument = indexDocument("orphan-a", "src/a.cpp");
  orphanDocument.size = orphanDocumentSize;

  auto liveDocument = indexDocument("live-b", "src/b.cpp");
  liveDocument.size = liveDocumentSize;

  storage.initialize();
  storage.upsertRepository(repositoryInfo(directory.path()));
  storage.upsertWorktree(worktreeInfo(directory.path()));
  storage.publishGeneration(indexGeneration({
    orphanDocument,
  }));
  storage.publishGeneration(indexGeneration({
    liveDocument,
  }));

  const auto report = storage.enforceDocumentBudget(documentBudget);

  CHECK(report.bytesBefore == orphanDocumentSize + liveDocumentSize);
  CHECK(report.bytesAfter == liveDocumentSize);
  CHECK(report.documentsRemoved == 1);
  CHECK_FALSE(report.budgetExceeded);
  CHECK_FALSE(storage.findReusableDocumentByContentHash(uburu::ContentHashAlgorithm::sha256, "orphan-a").has_value());
  CHECK(storage.findReusableDocumentByContentHash(uburu::ContentHashAlgorithm::sha256, "live-b").has_value());
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("sqlite storage preserves live documents when the document budget cannot be satisfied")
{
#if defined(UBURU_HAS_SQLITE)
  constexpr std::uintmax_t liveDocumentSize = 100;
  constexpr std::uintmax_t documentBudget = 10;

  TemporaryDirectory directory("uburu-sqlite-storage-budget-preserves-live-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  auto liveDocument = indexDocument("live-a", "src/a.cpp");
  liveDocument.size = liveDocumentSize;

  storage.initialize();
  storage.upsertRepository(repositoryInfo(directory.path()));
  storage.upsertWorktree(worktreeInfo(directory.path()));
  storage.publishGeneration(indexGeneration({
    liveDocument,
  }));

  const auto report = storage.enforceDocumentBudget(documentBudget);

  CHECK(report.bytesBefore == liveDocumentSize);
  CHECK(report.bytesAfter == liveDocumentSize);
  CHECK(report.documentsRemoved == 0);
  CHECK(report.budgetExceeded);
  CHECK(storage.findReusableDocumentByContentHash(uburu::ContentHashAlgorithm::sha256, "live-a").has_value());
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

TEST_CASE("storage paths provide default database location")
{
  const auto path = uburu::storage::defaultStorageDatabasePath();

  CHECK_FALSE(path.empty());
  CHECK(path.filename() == "uburu.db");
}

TEST_CASE("storage paths create private storage directory")
{
  TemporaryDirectory directory("uburu-storage-private-directory-test");
  const auto privateDirectory = directory.path() / "state";

  uburu::storage::ensurePrivateStorageDirectory(privateDirectory);

  CHECK(std::filesystem::is_directory(privateDirectory));

#if !defined(_WIN32)
  const auto permissions = std::filesystem::status(privateDirectory).permissions();
  constexpr auto groupOrOtherPermissions = std::filesystem::perms::group_all | std::filesystem::perms::others_all;

  CHECK((permissions & std::filesystem::perms::owner_all) == std::filesystem::perms::owner_all);
  CHECK((permissions & groupOrOtherPermissions) == std::filesystem::perms::none);
#endif
}

TEST_CASE("storage paths migrate database files without deleting the source")
{
  TemporaryDirectory directory("uburu-storage-paths-migration-test");
  const auto source = directory.path() / "source" / "uburu.db";
  const auto target = directory.path() / "target" / "custom.db";

  std::filesystem::create_directories(source.parent_path());
  {
    std::ofstream database(source, std::ios::binary);
    database << "database";
  }
  {
    std::ofstream wal(source.string() + "-wal", std::ios::binary);
    wal << "wal";
  }
  {
    std::ofstream sharedMemory(source.string() + "-shm", std::ios::binary);
    sharedMemory << "shm";
  }

  const auto result = uburu::storage::migrateStorageDatabase(source, target);

  CHECK(result.copiedDatabase);
  CHECK(result.copiedWriteAheadLog);
  CHECK(result.copiedSharedMemory);
  CHECK(std::filesystem::exists(source));
  CHECK(std::filesystem::exists(target));
  CHECK(std::filesystem::exists(target.string() + "-wal"));
  CHECK(std::filesystem::exists(target.string() + "-shm"));
}
