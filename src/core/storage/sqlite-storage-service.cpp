#include "core/storage/sqlite-storage-service.hpp"

#include <chrono>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#if defined(UBURU_HAS_SQLITE)
#include <sqlite3.h>
#endif

namespace uburu::storage
{
  namespace
  {

#if defined(UBURU_HAS_SQLITE)

    constexpr int initialSchemaVersion = 1;
    constexpr int hashAlgorithmSchemaVersion = 2;
    constexpr int contentHashIdentitySchemaVersion = 3;
    constexpr int currentSchemaVersion = contentHashIdentitySchemaVersion;
    constexpr int sqliteOk = SQLITE_OK;
    constexpr int sqliteDone = SQLITE_DONE;
    constexpr int sqliteRow = SQLITE_ROW;
    constexpr int busyTimeoutMilliseconds = 5000;

    [[nodiscard]] sqlite3* asDatabase(void* handle)
    {
      return static_cast<sqlite3*>(handle);
    }

    [[nodiscard]] void* asHandle(sqlite3* database)
    {
      return database;
    }

    [[nodiscard]] sqlite3* requireDatabase(void* handle)
    {
      if (handle == nullptr)
        throw std::runtime_error("SQLite storage service was used before initialize()");

      return asDatabase(handle);
    }

    [[nodiscard]] std::string sqliteMessage(sqlite3* database, std::string_view prefix)
    {
      return std::string(prefix) + ": " + sqlite3_errmsg(database);
    }

    void requireSqlite(int result, sqlite3* database, std::string_view operation)
    {
      if (result == sqliteOk)
        return;

      throw std::runtime_error(sqliteMessage(database, operation));
    }

    void execute(sqlite3* database, std::string_view sql)
    {
      char* errorMessage = nullptr;
      const auto result = sqlite3_exec(database, std::string(sql).c_str(), nullptr, nullptr, &errorMessage);

      if (result == sqliteOk)
        return;

      std::string message = std::string("SQLite execution failed");
      if (errorMessage != nullptr) {
        message += ": ";
        message += errorMessage;
      }

      sqlite3_free(errorMessage);
      throw std::runtime_error(message);
    }

    [[nodiscard]] std::int64_t toUnixMilliseconds(std::chrono::system_clock::time_point value)
    {
      return std::chrono::duration_cast<std::chrono::milliseconds>(value.time_since_epoch()).count();
    }

    [[nodiscard]] std::chrono::system_clock::time_point fromUnixMilliseconds(std::int64_t value)
    {
      return std::chrono::system_clock::time_point{std::chrono::milliseconds{value}};
    }

    [[nodiscard]] std::optional<std::string> optionalText(sqlite3_stmt* statement, int column)
    {
      if (sqlite3_column_type(statement, column) == SQLITE_NULL)
        return std::nullopt;

      const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(statement, column));

      if (text == nullptr)
        return std::nullopt;

      return std::string{text};
    }

    [[nodiscard]] GitFileStatus toGitFileStatus(int value)
    {
      return static_cast<GitFileStatus>(value);
    }

    [[nodiscard]] ContentHashAlgorithm toContentHashAlgorithm(int value)
    {
      return static_cast<ContentHashAlgorithm>(value);
    }

    [[nodiscard]] GitObjectHashAlgorithm toGitObjectHashAlgorithm(int value)
    {
      return static_cast<GitObjectHashAlgorithm>(value);
    }

    class Statement
    {
    public:
      Statement(sqlite3* database, std::string_view sql) : database(database)
      {
        const auto result = sqlite3_prepare_v2(database, std::string(sql).c_str(), -1, &statement, nullptr);
        requireSqlite(result, database, "failed to prepare SQLite statement");
      }

      ~Statement()
      {
        sqlite3_finalize(statement);
      }

      Statement(const Statement&) = delete;
      Statement& operator=(const Statement&) = delete;
      Statement(Statement&&) noexcept = delete;
      Statement& operator=(Statement&&) noexcept = delete;

      void bindText(int index, std::string_view value)
      {
        const auto result = sqlite3_bind_text(statement, index, value.data(), static_cast<int>(value.size()),
                                             SQLITE_TRANSIENT);
        requireSqlite(result, database, "failed to bind SQLite text");
      }

      void bindPath(int index, const std::filesystem::path& value)
      {
        bindText(index, value.generic_string());
      }

      void bindOptionalText(int index, const std::optional<std::string>& value)
      {
        if (!value) {
          bindNull(index);

          return;
        }

        bindText(index, *value);
      }

      void bindOptionalPath(int index, const std::optional<std::filesystem::path>& value)
      {
        if (!value) {
          bindNull(index);

          return;
        }

        bindPath(index, *value);
      }

      void bindInt64(int index, std::int64_t value)
      {
        const auto result = sqlite3_bind_int64(statement, index, value);
        requireSqlite(result, database, "failed to bind SQLite integer");
      }

      void bindBool(int index, bool value)
      {
        bindInt64(index, value ? 1 : 0);
      }

      void bindNull(int index)
      {
        const auto result = sqlite3_bind_null(statement, index);
        requireSqlite(result, database, "failed to bind SQLite null");
      }

      [[nodiscard]] bool stepRow()
      {
        const auto result = sqlite3_step(statement);

        if (result == sqliteRow)
          return true;

        if (result == sqliteDone)
          return false;

        throw std::runtime_error(sqliteMessage(database, "failed to step SQLite statement"));
      }

      void executeDone()
      {
        const auto result = sqlite3_step(statement);

        if (result == sqliteDone)
          return;

        throw std::runtime_error(sqliteMessage(database, "failed to execute SQLite statement"));
      }

      [[nodiscard]] sqlite3_stmt* get() const
      {
        return statement;
      }

    private:
      sqlite3* database{nullptr};
      sqlite3_stmt* statement{nullptr};
    };

    void applyInitialSchema(sqlite3* database)
    {
      execute(database, "BEGIN IMMEDIATE");
      try {
        execute(database, R"sql(
          CREATE TABLE IF NOT EXISTS schema_migrations (
            version INTEGER PRIMARY KEY,
            applied_at_unix_ms INTEGER NOT NULL
          );
        )sql");
        execute(database, R"sql(
          CREATE TABLE IF NOT EXISTS repositories (
            id TEXT PRIMARY KEY,
            common_git_directory TEXT NOT NULL,
            worktree_root TEXT,
            current_branch TEXT,
            head_oid TEXT NOT NULL,
            detached_head INTEGER NOT NULL
          );
        )sql");
        execute(database, R"sql(
          CREATE TABLE IF NOT EXISTS worktrees (
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
        )sql");
        execute(database, R"sql(
          CREATE TABLE IF NOT EXISTS generations (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            repository_id TEXT NOT NULL REFERENCES repositories(id) ON DELETE CASCADE,
            worktree_id TEXT NOT NULL REFERENCES worktrees(id) ON DELETE CASCADE,
            head_oid TEXT NOT NULL,
            branch TEXT,
            created_at_unix_ms INTEGER NOT NULL,
            published INTEGER NOT NULL
          );
        )sql");
        execute(database, R"sql(
          CREATE TABLE IF NOT EXISTS documents (
            content_hash TEXT PRIMARY KEY,
            git_blob_hash TEXT,
            size INTEGER NOT NULL,
            indexed_at_unix_ms INTEGER NOT NULL
          );
        )sql");
        execute(database, R"sql(
          CREATE TABLE IF NOT EXISTS files (
            worktree_id TEXT NOT NULL REFERENCES worktrees(id) ON DELETE CASCADE,
            relative_path TEXT NOT NULL,
            repository_id TEXT NOT NULL REFERENCES repositories(id) ON DELETE CASCADE,
            content_hash TEXT NOT NULL REFERENCES documents(content_hash),
            git_blob_hash TEXT,
            status INTEGER NOT NULL,
            size INTEGER NOT NULL,
            indexed_at_unix_ms INTEGER NOT NULL,
            deleted INTEGER NOT NULL,
            PRIMARY KEY (worktree_id, relative_path)
          );
        )sql");
        execute(database, R"sql(
          CREATE TABLE IF NOT EXISTS overlays (
            worktree_id TEXT NOT NULL REFERENCES worktrees(id) ON DELETE CASCADE,
            relative_path TEXT NOT NULL,
            previous_relative_path TEXT,
            status INTEGER NOT NULL,
            disposition INTEGER NOT NULL,
            reusable_blob_algorithm INTEGER,
            reusable_blob_value TEXT,
            PRIMARY KEY (worktree_id, relative_path)
          );
        )sql");
        Statement migrationStatement(database, R"sql(
          INSERT OR IGNORE INTO schema_migrations (version, applied_at_unix_ms)
          VALUES (?, ?);
        )sql");

        migrationStatement.bindInt64(1, initialSchemaVersion);
        migrationStatement.bindInt64(2, toUnixMilliseconds(std::chrono::system_clock::now()));
        migrationStatement.executeDone();
        execute(database, "PRAGMA user_version = " + std::to_string(initialSchemaVersion));
        execute(database, "COMMIT");
      } catch (...) {
        execute(database, "ROLLBACK");

        throw;
      }
    }

    [[nodiscard]] int schemaVersion(sqlite3* database)
    {
      Statement statement(database, "PRAGMA user_version");

      if (!statement.stepRow())
        return 0;

      return sqlite3_column_int(statement.get(), 0);
    }

    [[nodiscard]] bool columnExists(sqlite3* database, std::string_view tableName, std::string_view columnName)
    {
      Statement statement(database, "PRAGMA table_info(" + std::string(tableName) + ")");

      while (statement.stepRow()) {
        const auto* name = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 1));

        if (name != nullptr && columnName == name)
          return true;
      }

      return false;
    }

    void addColumnIfMissing(sqlite3* database,
                            std::string_view tableName,
                            std::string_view columnName,
                            std::string_view definition)
    {
      if (columnExists(database, tableName, columnName))
        return;

      execute(database, "ALTER TABLE " + std::string(tableName) + " ADD COLUMN " + std::string(definition));
    }

    void applyHashAlgorithmMigration(sqlite3* database)
    {
      execute(database, "BEGIN IMMEDIATE");
      try {
        addColumnIfMissing(database, "documents", "content_hash_algorithm",
                           "content_hash_algorithm INTEGER NOT NULL DEFAULT 0");
        addColumnIfMissing(database, "documents", "git_blob_hash_algorithm",
                           "git_blob_hash_algorithm INTEGER NOT NULL DEFAULT 0");
        addColumnIfMissing(database, "files", "content_hash_algorithm",
                           "content_hash_algorithm INTEGER NOT NULL DEFAULT 0");
        addColumnIfMissing(database, "files", "git_blob_hash_algorithm",
                           "git_blob_hash_algorithm INTEGER NOT NULL DEFAULT 0");

        Statement migrationStatement(database, R"sql(
          INSERT OR IGNORE INTO schema_migrations (version, applied_at_unix_ms)
          VALUES (?, ?);
        )sql");

        migrationStatement.bindInt64(1, hashAlgorithmSchemaVersion);
        migrationStatement.bindInt64(2, toUnixMilliseconds(std::chrono::system_clock::now()));
        migrationStatement.executeDone();
        execute(database, "PRAGMA user_version = " + std::to_string(hashAlgorithmSchemaVersion));
        execute(database, "COMMIT");
      } catch (...) {
        execute(database, "ROLLBACK");

        throw;
      }
    }

    void applyContentHashIdentityMigration(sqlite3* database)
    {
      execute(database, "BEGIN IMMEDIATE");
      try {
        execute(database, R"sql(
          CREATE TABLE documents_v3 (
            content_hash TEXT NOT NULL,
            content_hash_algorithm INTEGER NOT NULL,
            git_blob_hash TEXT,
            git_blob_hash_algorithm INTEGER NOT NULL DEFAULT 0,
            size INTEGER NOT NULL,
            indexed_at_unix_ms INTEGER NOT NULL,
            PRIMARY KEY (content_hash_algorithm, content_hash)
          );
        )sql");
        execute(database, R"sql(
          INSERT OR IGNORE INTO documents_v3 (
            content_hash, content_hash_algorithm, git_blob_hash, git_blob_hash_algorithm, size, indexed_at_unix_ms
          )
          SELECT content_hash, content_hash_algorithm, git_blob_hash, git_blob_hash_algorithm, size,
                 indexed_at_unix_ms
          FROM documents;
        )sql");
        execute(database, R"sql(
          CREATE TABLE files_v3 (
            worktree_id TEXT NOT NULL REFERENCES worktrees(id) ON DELETE CASCADE,
            relative_path TEXT NOT NULL,
            repository_id TEXT NOT NULL REFERENCES repositories(id) ON DELETE CASCADE,
            content_hash TEXT NOT NULL,
            content_hash_algorithm INTEGER NOT NULL,
            git_blob_hash TEXT,
            git_blob_hash_algorithm INTEGER NOT NULL DEFAULT 0,
            status INTEGER NOT NULL,
            size INTEGER NOT NULL,
            indexed_at_unix_ms INTEGER NOT NULL,
            deleted INTEGER NOT NULL,
            PRIMARY KEY (worktree_id, relative_path),
            FOREIGN KEY (content_hash_algorithm, content_hash)
              REFERENCES documents_v3(content_hash_algorithm, content_hash)
          );
        )sql");
        execute(database, R"sql(
          INSERT INTO files_v3 (
            worktree_id, relative_path, repository_id, content_hash, content_hash_algorithm, git_blob_hash,
            git_blob_hash_algorithm, status, size, indexed_at_unix_ms, deleted
          )
          SELECT worktree_id, relative_path, repository_id, content_hash, content_hash_algorithm, git_blob_hash,
                 git_blob_hash_algorithm, status, size, indexed_at_unix_ms, deleted
          FROM files;
        )sql");
        execute(database, "DROP TABLE files");
        execute(database, "DROP TABLE documents");
        execute(database, "ALTER TABLE documents_v3 RENAME TO documents");
        execute(database, "ALTER TABLE files_v3 RENAME TO files");

        Statement migrationStatement(database, R"sql(
          INSERT OR IGNORE INTO schema_migrations (version, applied_at_unix_ms)
          VALUES (?, ?);
        )sql");

        migrationStatement.bindInt64(1, contentHashIdentitySchemaVersion);
        migrationStatement.bindInt64(2, toUnixMilliseconds(std::chrono::system_clock::now()));
        migrationStatement.executeDone();
        execute(database, "PRAGMA user_version = " + std::to_string(contentHashIdentitySchemaVersion));
        execute(database, "COMMIT");
      } catch (...) {
        execute(database, "ROLLBACK");

        throw;
      }
    }

    void applyMigrations(sqlite3* database)
    {
      if (schemaVersion(database) < hashAlgorithmSchemaVersion)
        applyHashAlgorithmMigration(database);

      if (schemaVersion(database) < contentHashIdentitySchemaVersion)
        applyContentHashIdentityMigration(database);

      if (schemaVersion(database) != currentSchemaVersion)
        throw std::runtime_error("SQLite database schema version is newer than this build supports");
    }

    void upsertDocumentRecord(sqlite3* database, const IndexDocument& document)
    {
      Statement documentStatement(database, R"sql(
        INSERT INTO documents (
          content_hash, content_hash_algorithm, git_blob_hash, git_blob_hash_algorithm, size, indexed_at_unix_ms
        ) VALUES (?, ?, ?, ?, ?, ?)
        ON CONFLICT(content_hash_algorithm, content_hash) DO UPDATE SET
          content_hash_algorithm = excluded.content_hash_algorithm,
          git_blob_hash = COALESCE(excluded.git_blob_hash, documents.git_blob_hash),
          git_blob_hash_algorithm = excluded.git_blob_hash_algorithm,
          size = excluded.size,
          indexed_at_unix_ms = excluded.indexed_at_unix_ms;
      )sql");

      documentStatement.bindText(1, document.contentHash);
      documentStatement.bindInt64(2, static_cast<std::int64_t>(document.contentHashAlgorithm));
      documentStatement.bindOptionalText(3, document.gitBlobHash);
      documentStatement.bindInt64(4, static_cast<std::int64_t>(document.gitBlobHashAlgorithm));
      documentStatement.bindInt64(5, static_cast<std::int64_t>(document.size));
      documentStatement.bindInt64(6, toUnixMilliseconds(document.indexedAt));
      documentStatement.executeDone();

      Statement fileStatement(database, R"sql(
        INSERT INTO files (
          worktree_id, relative_path, repository_id, content_hash, content_hash_algorithm, git_blob_hash,
          git_blob_hash_algorithm, status, size, indexed_at_unix_ms, deleted
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(worktree_id, relative_path) DO UPDATE SET
          repository_id = excluded.repository_id,
          content_hash = excluded.content_hash,
          content_hash_algorithm = excluded.content_hash_algorithm,
          git_blob_hash = excluded.git_blob_hash,
          git_blob_hash_algorithm = excluded.git_blob_hash_algorithm,
          status = excluded.status,
          size = excluded.size,
          indexed_at_unix_ms = excluded.indexed_at_unix_ms,
          deleted = excluded.deleted;
      )sql");

      fileStatement.bindText(1, document.worktreeId);
      fileStatement.bindPath(2, document.relativePath);
      fileStatement.bindText(3, document.repositoryId);
      fileStatement.bindText(4, document.contentHash);
      fileStatement.bindInt64(5, static_cast<std::int64_t>(document.contentHashAlgorithm));
      fileStatement.bindOptionalText(6, document.gitBlobHash);
      fileStatement.bindInt64(7, static_cast<std::int64_t>(document.gitBlobHashAlgorithm));
      fileStatement.bindInt64(8, static_cast<std::int64_t>(document.status));
      fileStatement.bindInt64(9, static_cast<std::int64_t>(document.size));
      fileStatement.bindInt64(10, toUnixMilliseconds(document.indexedAt));
      fileStatement.bindBool(11, document.deleted);
      fileStatement.executeDone();
    }

    void deleteFilesForWorktree(sqlite3* database, const WorktreeId& worktreeId)
    {
      Statement statement(database, "DELETE FROM files WHERE worktree_id = ?");

      statement.bindText(1, worktreeId);
      statement.executeDone();
    }

    [[nodiscard]] std::int64_t insertGenerationRecord(sqlite3* database, const IndexGeneration& generation)
    {
      Statement statement(database, R"sql(
        INSERT INTO generations (
          repository_id, worktree_id, head_oid, branch, created_at_unix_ms, published
        ) VALUES (?, ?, ?, ?, ?, 0);
      )sql");

      statement.bindText(1, generation.repositoryId);
      statement.bindText(2, generation.worktreeId);
      statement.bindText(3, generation.headOid);
      statement.bindOptionalText(4, generation.branch);
      statement.bindInt64(5, toUnixMilliseconds(generation.createdAt));
      statement.executeDone();

      return sqlite3_last_insert_rowid(database);
    }

    void markGenerationPublished(sqlite3* database, std::int64_t generationId)
    {
      Statement statement(database, "UPDATE generations SET published = 1 WHERE id = ?");

      statement.bindInt64(1, generationId);
      statement.executeDone();
    }

    void validateGenerationDocument(const IndexGeneration& generation, const IndexDocument& document)
    {
      if (document.repositoryId != generation.repositoryId)
        throw std::invalid_argument("generation document belongs to another repository");

      if (document.worktreeId != generation.worktreeId)
        throw std::invalid_argument("generation document belongs to another worktree");
    }

#endif

  } // namespace

  SQLiteStorageService::SQLiteStorageService(std::filesystem::path databasePath)
    : databasePath(std::move(databasePath))
  {}

  SQLiteStorageService::~SQLiteStorageService()
  {
#if defined(UBURU_HAS_SQLITE)
    if (databaseHandle != nullptr)
      sqlite3_close(asDatabase(databaseHandle));
#endif
  }

  void SQLiteStorageService::initialize()
  {
#if defined(UBURU_HAS_SQLITE)
    if (databaseHandle == nullptr) {
      sqlite3* openedDatabase = nullptr;
      const auto flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX;
      const auto result = sqlite3_open_v2(databasePath.string().c_str(), &openedDatabase, flags, nullptr);

      if (result != sqliteOk) {
        const auto message = openedDatabase == nullptr ? std::string{"failed to open SQLite database"}
                                                       : sqliteMessage(openedDatabase, "failed to open SQLite database");
        if (openedDatabase != nullptr)
          sqlite3_close(openedDatabase);

        throw std::runtime_error(message);
      }

      databaseHandle = asHandle(openedDatabase);
    }

    auto* database = requireDatabase(databaseHandle);
    requireSqlite(sqlite3_busy_timeout(database, busyTimeoutMilliseconds), database, "failed to set SQLite busy timeout");
    execute(database, "PRAGMA foreign_keys = ON");
    execute(database, "PRAGMA journal_mode = WAL");
    execute(database, "PRAGMA synchronous = NORMAL");
    applyInitialSchema(database);
    applyMigrations(database);
#else
    throw std::runtime_error("SQLite support is not available in this build");
#endif
  }

  void SQLiteStorageService::upsertRepository(const RepositoryInfo& repository)
  {
#if defined(UBURU_HAS_SQLITE)
    auto* database = requireDatabase(databaseHandle);
    Statement statement(database, R"sql(
      INSERT INTO repositories (
        id, common_git_directory, worktree_root, current_branch, head_oid, detached_head
      ) VALUES (?, ?, ?, ?, ?, ?)
      ON CONFLICT(id) DO UPDATE SET
        common_git_directory = excluded.common_git_directory,
        worktree_root = excluded.worktree_root,
        current_branch = excluded.current_branch,
        head_oid = excluded.head_oid,
        detached_head = excluded.detached_head;
    )sql");

    statement.bindText(1, repository.id);
    statement.bindPath(2, repository.commonGitDirectory);
    statement.bindOptionalPath(3, repository.worktreeRoot);
    statement.bindOptionalText(4, repository.currentBranch);
    statement.bindText(5, repository.headOid);
    statement.bindBool(6, repository.detachedHead);
    statement.executeDone();
#else
    static_cast<void>(repository);
    throw std::runtime_error("SQLite support is not available in this build");
#endif
  }

  void SQLiteStorageService::upsertWorktree(const WorktreeInfo& worktree)
  {
#if defined(UBURU_HAS_SQLITE)
    auto* database = requireDatabase(databaseHandle);
    Statement statement(database, R"sql(
      INSERT INTO worktrees (
        id, repository_id, root, git_directory, branch, head_oid, locked, prunable, lock_reason
      ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
      ON CONFLICT(id) DO UPDATE SET
        repository_id = excluded.repository_id,
        root = excluded.root,
        git_directory = excluded.git_directory,
        branch = excluded.branch,
        head_oid = excluded.head_oid,
        locked = excluded.locked,
        prunable = excluded.prunable,
        lock_reason = excluded.lock_reason;
    )sql");

    statement.bindText(1, worktree.id);
    statement.bindText(2, worktree.repositoryId);
    statement.bindPath(3, worktree.root);
    statement.bindPath(4, worktree.gitDirectory);
    statement.bindOptionalText(5, worktree.branch);
    statement.bindText(6, worktree.headOid);
    statement.bindBool(7, worktree.locked);
    statement.bindBool(8, worktree.prunable);
    statement.bindText(9, worktree.lockReason);
    statement.executeDone();
#else
    static_cast<void>(worktree);
    throw std::runtime_error("SQLite support is not available in this build");
#endif
  }

  void SQLiteStorageService::upsertDocument(const IndexDocument& document)
  {
#if defined(UBURU_HAS_SQLITE)
    auto* database = requireDatabase(databaseHandle);
    execute(database, "BEGIN IMMEDIATE");
    try {
      upsertDocumentRecord(database, document);
      execute(database, "COMMIT");
    } catch (...) {
      execute(database, "ROLLBACK");

      throw;
    }
#else
    static_cast<void>(document);
    throw std::runtime_error("SQLite support is not available in this build");
#endif
  }

  void SQLiteStorageService::publishGeneration(const IndexGeneration& generation)
  {
#if defined(UBURU_HAS_SQLITE)
    auto* database = requireDatabase(databaseHandle);
    execute(database, "BEGIN IMMEDIATE");
    try {
      const auto generationId = insertGenerationRecord(database, generation);
      deleteFilesForWorktree(database, generation.worktreeId);

      for (const auto& document : generation.documents) {
        validateGenerationDocument(generation, document);
        upsertDocumentRecord(database, document);
      }

      markGenerationPublished(database, generationId);
      execute(database, "COMMIT");
    } catch (...) {
      execute(database, "ROLLBACK");

      throw;
    }
#else
    static_cast<void>(generation);
    throw std::runtime_error("SQLite support is not available in this build");
#endif
  }

  void SQLiteStorageService::removeDocument(const WorktreeId& worktreeId, const std::filesystem::path& relativePath)
  {
#if defined(UBURU_HAS_SQLITE)
    auto* database = requireDatabase(databaseHandle);
    Statement statement(database, R"sql(
      UPDATE files
      SET deleted = 1, status = ?
      WHERE worktree_id = ? AND relative_path = ?;
    )sql");

    statement.bindInt64(1, static_cast<std::int64_t>(GitFileStatus::deleted));
    statement.bindText(2, worktreeId);
    statement.bindPath(3, relativePath);
    statement.executeDone();
#else
    static_cast<void>(worktreeId);
    static_cast<void>(relativePath);
    throw std::runtime_error("SQLite support is not available in this build");
#endif
  }

  std::optional<IndexDocument> SQLiteStorageService::findDocument(const WorktreeId& worktreeId,
                                                                  const std::filesystem::path& relativePath) const
  {
#if defined(UBURU_HAS_SQLITE)
    auto* database = requireDatabase(databaseHandle);
    Statement statement(database, R"sql(
      SELECT repository_id, worktree_id, relative_path, content_hash, content_hash_algorithm, git_blob_hash,
             git_blob_hash_algorithm, status, size, indexed_at_unix_ms, deleted
      FROM files
      WHERE worktree_id = ? AND relative_path = ?;
    )sql");

    statement.bindText(1, worktreeId);
    statement.bindPath(2, relativePath);

    if (!statement.stepRow())
      return std::nullopt;

    auto* row = statement.get();
    IndexDocument document{
      .repositoryId = reinterpret_cast<const char*>(sqlite3_column_text(row, 0)),
      .worktreeId = reinterpret_cast<const char*>(sqlite3_column_text(row, 1)),
      .relativePath = reinterpret_cast<const char*>(sqlite3_column_text(row, 2)),
      .contentHash = reinterpret_cast<const char*>(sqlite3_column_text(row, 3)),
      .contentHashAlgorithm = toContentHashAlgorithm(sqlite3_column_int(row, 4)),
      .gitBlobHash = optionalText(row, 5),
      .gitBlobHashAlgorithm = toGitObjectHashAlgorithm(sqlite3_column_int(row, 6)),
      .status = toGitFileStatus(sqlite3_column_int(row, 7)),
      .size = static_cast<std::uintmax_t>(sqlite3_column_int64(row, 8)),
      .indexedAt = fromUnixMilliseconds(sqlite3_column_int64(row, 9)),
      .deleted = sqlite3_column_int(row, 10) != 0};

    return document;
#else
    static_cast<void>(worktreeId);
    static_cast<void>(relativePath);
    throw std::runtime_error("SQLite support is not available in this build");
#endif
  }

} // namespace uburu::storage
