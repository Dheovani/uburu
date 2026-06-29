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
    constexpr int metadataSchemaVersion = 4;
    constexpr int documentFormatSchemaVersion = 5;
    constexpr int currentSchemaVersion = documentFormatSchemaVersion;
    constexpr int defaultDocumentFormatVersion = static_cast<int>(currentIndexDocumentFormatVersion);
    constexpr int sqliteOk = SQLITE_OK;
    constexpr int sqliteDone = SQLITE_DONE;
    constexpr int sqliteRow = SQLITE_ROW;
    constexpr int busyTimeoutMilliseconds = 5000;
    constexpr int sqliteSynchronousOff = 0;
    constexpr int sqliteSynchronousNormal = 1;
    constexpr int sqliteSynchronousFull = 2;
    constexpr int sqliteSynchronousExtra = 3;

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

    [[nodiscard]] std::int64_t nowUnixMilliseconds()
    {
      return toUnixMilliseconds(std::chrono::system_clock::now());
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

    [[nodiscard]] std::string preferenceScope(std::optional<RepositoryId> repositoryId)
    {
      return repositoryId.value_or(std::string{});
    }

    [[nodiscard]] std::string synchronousModeName(int value)
    {
      switch (value) {
      case sqliteSynchronousOff:
        return "OFF";
      case sqliteSynchronousNormal:
        return "NORMAL";
      case sqliteSynchronousFull:
        return "FULL";
      case sqliteSynchronousExtra:
        return "EXTRA";
      }

      return "UNKNOWN";
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
        const auto result =
            sqlite3_bind_text(statement, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
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

    [[nodiscard]] int scalarInt(sqlite3* database, std::string_view sql)
    {
      Statement statement(database, sql);

      if (!statement.stepRow())
        return 0;

      return sqlite3_column_int(statement.get(), 0);
    }

    [[nodiscard]] std::string scalarText(sqlite3* database, std::string_view sql)
    {
      Statement statement(database, sql);

      if (!statement.stepRow())
        return {};

      const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 0));

      return text == nullptr ? std::string{} : std::string{text};
    }

    void deleteRowsBeyondRetention(sqlite3* database, std::string_view tableName, std::string_view orderColumn,
                                   std::size_t retentionLimit)
    {
      Statement statement(database, "DELETE FROM " + std::string(tableName) + " WHERE id NOT IN (SELECT id FROM " +
                                        std::string(tableName) + " ORDER BY " + std::string(orderColumn) +
                                        " DESC, id DESC LIMIT ?)");

      statement.bindInt64(1, static_cast<std::int64_t>(retentionLimit));
      statement.executeDone();
    }

    void deleteMetricRowsBeyondRetention(sqlite3* database, const std::string& name, std::size_t retentionLimit)
    {
      Statement statement(database, R"sql(
        DELETE FROM indexing_metrics
        WHERE name = ?
          AND id NOT IN (
            SELECT id
            FROM indexing_metrics
            WHERE name = ?
            ORDER BY recorded_at_unix_ms DESC, id DESC
            LIMIT ?
          );
      )sql");

      statement.bindText(1, name);
      statement.bindText(2, name);
      statement.bindInt64(3, static_cast<std::int64_t>(retentionLimit));
      statement.executeDone();
    }

    void deleteAllIndexCatalogRows(sqlite3* database)
    {
      execute(database, "BEGIN IMMEDIATE");
      try {
        execute(database, "DELETE FROM overlays");
        execute(database, "DELETE FROM files");
        execute(database, "DELETE FROM documents");
        execute(database, "DELETE FROM generations");
        execute(database, "COMMIT");
      } catch (...) {
        execute(database, "ROLLBACK");

        throw;
      }
    }

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

    void addColumnIfMissing(sqlite3* database, std::string_view tableName, std::string_view columnName,
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

    void applyMetadataMigration(sqlite3* database)
    {
      execute(database, "BEGIN IMMEDIATE");
      try {
        execute(database, R"sql(
          CREATE TABLE IF NOT EXISTS preferences (
            scope_id TEXT NOT NULL,
            key TEXT NOT NULL,
            value TEXT NOT NULL,
            updated_at_unix_ms INTEGER NOT NULL,
            PRIMARY KEY (scope_id, key)
          );
        )sql");
        execute(database, R"sql(
          CREATE TABLE IF NOT EXISTS search_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            root TEXT NOT NULL,
            expression TEXT NOT NULL,
            searched_at_unix_ms INTEGER NOT NULL
          );
        )sql");
        execute(database, R"sql(
          CREATE TABLE IF NOT EXISTS saved_searches (
            name TEXT PRIMARY KEY,
            root TEXT NOT NULL,
            expression TEXT NOT NULL,
            saved_at_unix_ms INTEGER NOT NULL
          );
        )sql");
        execute(database, R"sql(
          CREATE TABLE IF NOT EXISTS indexing_metrics (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            value INTEGER NOT NULL,
            recorded_at_unix_ms INTEGER NOT NULL
          );
        )sql");

        Statement migrationStatement(database, R"sql(
          INSERT OR IGNORE INTO schema_migrations (version, applied_at_unix_ms)
          VALUES (?, ?);
        )sql");

        migrationStatement.bindInt64(1, metadataSchemaVersion);
        migrationStatement.bindInt64(2, nowUnixMilliseconds());
        migrationStatement.executeDone();
        execute(database, "PRAGMA user_version = " + std::to_string(metadataSchemaVersion));
        execute(database, "COMMIT");
      } catch (...) {
        execute(database, "ROLLBACK");

        throw;
      }
    }

    void applyDocumentFormatMigration(sqlite3* database)
    {
      execute(database, "BEGIN IMMEDIATE");
      try {
        const auto definition =
          "format_version INTEGER NOT NULL DEFAULT " + std::to_string(defaultDocumentFormatVersion);

        addColumnIfMissing(database, "documents", "format_version", definition);
        addColumnIfMissing(database, "files", "format_version", definition);

        Statement migrationStatement(database, R"sql(
          INSERT OR IGNORE INTO schema_migrations (version, applied_at_unix_ms)
          VALUES (?, ?);
        )sql");

        migrationStatement.bindInt64(1, documentFormatSchemaVersion);
        migrationStatement.bindInt64(2, toUnixMilliseconds(std::chrono::system_clock::now()));
        migrationStatement.executeDone();
        execute(database, "PRAGMA user_version = " + std::to_string(documentFormatSchemaVersion));
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

      if (schemaVersion(database) < metadataSchemaVersion)
        applyMetadataMigration(database);

      if (schemaVersion(database) < documentFormatSchemaVersion)
        applyDocumentFormatMigration(database);

      if (schemaVersion(database) != currentSchemaVersion)
        throw std::runtime_error("SQLite database schema version is newer than this build supports");
    }

    void upsertDocumentRecord(sqlite3* database, const IndexDocument& document)
    {
      Statement documentStatement(database, R"sql(
        INSERT INTO documents (
          content_hash, content_hash_algorithm, format_version, git_blob_hash, git_blob_hash_algorithm, size,
          indexed_at_unix_ms
        ) VALUES (?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(content_hash_algorithm, content_hash) DO UPDATE SET
          content_hash_algorithm = excluded.content_hash_algorithm,
          format_version = excluded.format_version,
          git_blob_hash = COALESCE(excluded.git_blob_hash, documents.git_blob_hash),
          git_blob_hash_algorithm = excluded.git_blob_hash_algorithm,
          size = excluded.size,
          indexed_at_unix_ms = excluded.indexed_at_unix_ms;
      )sql");

      documentStatement.bindText(1, document.contentHash);
      documentStatement.bindInt64(2, static_cast<std::int64_t>(document.contentHashAlgorithm));
      documentStatement.bindInt64(3, static_cast<std::int64_t>(document.formatVersion));
      documentStatement.bindOptionalText(4, document.gitBlobHash);
      documentStatement.bindInt64(5, static_cast<std::int64_t>(document.gitBlobHashAlgorithm));
      documentStatement.bindInt64(6, static_cast<std::int64_t>(document.size));
      documentStatement.bindInt64(7, toUnixMilliseconds(document.indexedAt));
      documentStatement.executeDone();

      Statement fileStatement(database, R"sql(
        INSERT INTO files (
          worktree_id, relative_path, repository_id, content_hash, content_hash_algorithm, format_version, git_blob_hash,
          git_blob_hash_algorithm, status, size, indexed_at_unix_ms, deleted
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ON CONFLICT(worktree_id, relative_path) DO UPDATE SET
          repository_id = excluded.repository_id,
          content_hash = excluded.content_hash,
          content_hash_algorithm = excluded.content_hash_algorithm,
          format_version = excluded.format_version,
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
      fileStatement.bindInt64(6, static_cast<std::int64_t>(document.formatVersion));
      fileStatement.bindOptionalText(7, document.gitBlobHash);
      fileStatement.bindInt64(8, static_cast<std::int64_t>(document.gitBlobHashAlgorithm));
      fileStatement.bindInt64(9, static_cast<std::int64_t>(document.status));
      fileStatement.bindInt64(10, static_cast<std::int64_t>(document.size));
      fileStatement.bindInt64(11, toUnixMilliseconds(document.indexedAt));
      fileStatement.bindBool(12, document.deleted);
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

    [[nodiscard]] std::size_t changedRowCount(sqlite3* database)
    {
      return static_cast<std::size_t>(sqlite3_changes64(database));
    }

    [[nodiscard]] std::size_t recoverIncompleteGenerationRecords(sqlite3* database)
    {
      Statement statement(database, "DELETE FROM generations WHERE published = 0");

      statement.executeDone();

      return changedRowCount(database);
    }

    [[nodiscard]] std::size_t collectOrphanDocumentRecords(sqlite3* database)
    {
      Statement statement(database, R"sql(
        DELETE FROM documents
        WHERE NOT EXISTS (
          SELECT 1
          FROM files
          WHERE files.content_hash_algorithm = documents.content_hash_algorithm
            AND files.content_hash = documents.content_hash
        );
      )sql");

      statement.executeDone();

      return changedRowCount(database);
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

  SQLiteStorageService::SQLiteStorageService(std::filesystem::path databasePath) : databasePath(std::move(databasePath))
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
        const auto message = openedDatabase == nullptr
                                 ? std::string{"failed to open SQLite database"}
                                 : sqliteMessage(openedDatabase, "failed to open SQLite database");
        if (openedDatabase != nullptr)
          sqlite3_close(openedDatabase);

        throw std::runtime_error(message);
      }

      databaseHandle = asHandle(openedDatabase);
    }

    auto* database = requireDatabase(databaseHandle);
    requireSqlite(sqlite3_busy_timeout(database, busyTimeoutMilliseconds), database,
                  "failed to set SQLite busy timeout");
    execute(database, "PRAGMA foreign_keys = ON");
    execute(database, "PRAGMA journal_mode = WAL");
    execute(database, "PRAGMA synchronous = NORMAL");
    applyInitialSchema(database);
    applyMigrations(database);
    static_cast<void>(recoverIncompleteGenerationRecords(database));
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

  std::size_t SQLiteStorageService::recoverIncompleteGenerations()
  {
#if defined(UBURU_HAS_SQLITE)
    auto* database = requireDatabase(databaseHandle);

    return recoverIncompleteGenerationRecords(database);
#else
    throw std::runtime_error("SQLite support is not available in this build");
#endif
  }

  std::size_t SQLiteStorageService::collectOrphanDocuments()
  {
#if defined(UBURU_HAS_SQLITE)
    auto* database = requireDatabase(databaseHandle);

    return collectOrphanDocumentRecords(database);
#else
    throw std::runtime_error("SQLite support is not available in this build");
#endif
  }

  StoragePragmaSnapshot SQLiteStorageService::pragmaSnapshot() const
  {
#if defined(UBURU_HAS_SQLITE)
    auto* database = requireDatabase(databaseHandle);

    return StoragePragmaSnapshot{.foreignKeysEnabled = scalarInt(database, "PRAGMA foreign_keys") != 0,
                                 .journalMode = scalarText(database, "PRAGMA journal_mode"),
                                 .synchronousMode = synchronousModeName(scalarInt(database, "PRAGMA synchronous")),
                                 .busyTimeout = std::chrono::milliseconds{scalarInt(database, "PRAGMA busy_timeout")}};
#else
    throw std::runtime_error("SQLite support is not available in this build");
#endif
  }

  StorageIntegrityReport SQLiteStorageService::validateIntegrity() const
  {
#if defined(UBURU_HAS_SQLITE)
    auto* database = requireDatabase(databaseHandle);
    const auto message = scalarText(database, "PRAGMA integrity_check");

    return StorageIntegrityReport{.ok = message == "ok", .message = message};
#else
    throw std::runtime_error("SQLite support is not available in this build");
#endif
  }

  void SQLiteStorageService::rebuildIndexCatalog()
  {
#if defined(UBURU_HAS_SQLITE)
    auto* database = requireDatabase(databaseHandle);
    deleteAllIndexCatalogRows(database);
#else
    throw std::runtime_error("SQLite support is not available in this build");
#endif
  }

  void SQLiteStorageService::setPreference(std::optional<RepositoryId> repositoryId, const std::string& key,
                                           const std::string& value)
  {
#if defined(UBURU_HAS_SQLITE)
    auto* database = requireDatabase(databaseHandle);
    Statement statement(database, R"sql(
      INSERT INTO preferences (scope_id, key, value, updated_at_unix_ms)
      VALUES (?, ?, ?, ?)
      ON CONFLICT(scope_id, key) DO UPDATE SET
        value = excluded.value,
        updated_at_unix_ms = excluded.updated_at_unix_ms;
    )sql");

    statement.bindText(1, preferenceScope(std::move(repositoryId)));
    statement.bindText(2, key);
    statement.bindText(3, value);
    statement.bindInt64(4, nowUnixMilliseconds());
    statement.executeDone();
#else
    static_cast<void>(repositoryId);
    static_cast<void>(key);
    static_cast<void>(value);
    throw std::runtime_error("SQLite support is not available in this build");
#endif
  }

  std::optional<std::string> SQLiteStorageService::preference(std::optional<RepositoryId> repositoryId,
                                                              const std::string& key) const
  {
#if defined(UBURU_HAS_SQLITE)
    auto* database = requireDatabase(databaseHandle);
    Statement statement(database, R"sql(
      SELECT value
      FROM preferences
      WHERE scope_id = ? AND key = ?;
    )sql");

    statement.bindText(1, preferenceScope(std::move(repositoryId)));
    statement.bindText(2, key);

    if (!statement.stepRow())
      return std::nullopt;

    return optionalText(statement.get(), 0);
#else
    static_cast<void>(repositoryId);
    static_cast<void>(key);
    throw std::runtime_error("SQLite support is not available in this build");
#endif
  }

  void SQLiteStorageService::recordSearch(const SearchHistoryEntry& entry, std::size_t retentionLimit)
  {
#if defined(UBURU_HAS_SQLITE)
    auto* database = requireDatabase(databaseHandle);
    execute(database, "BEGIN IMMEDIATE");
    try {
      Statement statement(database, R"sql(
        INSERT INTO search_history (root, expression, searched_at_unix_ms)
        VALUES (?, ?, ?);
      )sql");

      statement.bindPath(1, entry.root);
      statement.bindText(2, entry.expression);
      statement.bindInt64(3, toUnixMilliseconds(entry.searchedAt));
      statement.executeDone();
      deleteRowsBeyondRetention(database, "search_history", "searched_at_unix_ms", retentionLimit);
      execute(database, "COMMIT");
    } catch (...) {
      execute(database, "ROLLBACK");

      throw;
    }
#else
    static_cast<void>(entry);
    static_cast<void>(retentionLimit);
    throw std::runtime_error("SQLite support is not available in this build");
#endif
  }

  std::vector<SearchHistoryEntry> SQLiteStorageService::recentSearches(std::size_t limit) const
  {
#if defined(UBURU_HAS_SQLITE)
    auto* database = requireDatabase(databaseHandle);
    Statement statement(database, R"sql(
      SELECT root, expression, searched_at_unix_ms
      FROM search_history
      ORDER BY searched_at_unix_ms DESC, id DESC
      LIMIT ?;
    )sql");

    statement.bindInt64(1, static_cast<std::int64_t>(limit));
    std::vector<SearchHistoryEntry> entries;

    while (statement.stepRow()) {
      entries.push_back(
          SearchHistoryEntry{.root = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 0)),
                             .expression = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 1)),
                             .searchedAt = fromUnixMilliseconds(sqlite3_column_int64(statement.get(), 2))});
    }

    return entries;
#else
    static_cast<void>(limit);
    throw std::runtime_error("SQLite support is not available in this build");
#endif
  }

  void SQLiteStorageService::saveSearch(const SavedSearch& search)
  {
#if defined(UBURU_HAS_SQLITE)
    auto* database = requireDatabase(databaseHandle);
    Statement statement(database, R"sql(
      INSERT INTO saved_searches (name, root, expression, saved_at_unix_ms)
      VALUES (?, ?, ?, ?)
      ON CONFLICT(name) DO UPDATE SET
        root = excluded.root,
        expression = excluded.expression,
        saved_at_unix_ms = excluded.saved_at_unix_ms;
    )sql");

    statement.bindText(1, search.name);
    statement.bindPath(2, search.root);
    statement.bindText(3, search.expression);
    statement.bindInt64(4, toUnixMilliseconds(search.savedAt));
    statement.executeDone();
#else
    static_cast<void>(search);
    throw std::runtime_error("SQLite support is not available in this build");
#endif
  }

  std::vector<SavedSearch> SQLiteStorageService::savedSearches() const
  {
#if defined(UBURU_HAS_SQLITE)
    auto* database = requireDatabase(databaseHandle);
    Statement statement(database, R"sql(
      SELECT name, root, expression, saved_at_unix_ms
      FROM saved_searches
      ORDER BY name ASC;
    )sql");
    std::vector<SavedSearch> searches;

    while (statement.stepRow()) {
      searches.push_back(
          SavedSearch{.name = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 0)),
                      .root = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 1)),
                      .expression = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 2)),
                      .savedAt = fromUnixMilliseconds(sqlite3_column_int64(statement.get(), 3))});
    }

    return searches;
#else
    throw std::runtime_error("SQLite support is not available in this build");
#endif
  }

  void SQLiteStorageService::recordIndexingMetric(const IndexingMetric& metric, std::size_t retentionLimit)
  {
#if defined(UBURU_HAS_SQLITE)
    auto* database = requireDatabase(databaseHandle);
    execute(database, "BEGIN IMMEDIATE");
    try {
      Statement statement(database, R"sql(
        INSERT INTO indexing_metrics (name, value, recorded_at_unix_ms)
        VALUES (?, ?, ?);
      )sql");

      statement.bindText(1, metric.name);
      statement.bindInt64(2, metric.value);
      statement.bindInt64(3, toUnixMilliseconds(metric.recordedAt));
      statement.executeDone();
      deleteMetricRowsBeyondRetention(database, metric.name, retentionLimit);
      execute(database, "COMMIT");
    } catch (...) {
      execute(database, "ROLLBACK");

      throw;
    }
#else
    static_cast<void>(metric);
    static_cast<void>(retentionLimit);
    throw std::runtime_error("SQLite support is not available in this build");
#endif
  }

  std::vector<IndexingMetric> SQLiteStorageService::recentIndexingMetrics(const std::string& name,
                                                                          std::size_t limit) const
  {
#if defined(UBURU_HAS_SQLITE)
    auto* database = requireDatabase(databaseHandle);
    Statement statement(database, R"sql(
      SELECT name, value, recorded_at_unix_ms
      FROM indexing_metrics
      WHERE name = ?
      ORDER BY recorded_at_unix_ms DESC, id DESC
      LIMIT ?;
    )sql");

    statement.bindText(1, name);
    statement.bindInt64(2, static_cast<std::int64_t>(limit));
    std::vector<IndexingMetric> metrics;

    while (statement.stepRow()) {
      metrics.push_back(IndexingMetric{.name = reinterpret_cast<const char*>(sqlite3_column_text(statement.get(), 0)),
                                       .value = sqlite3_column_int64(statement.get(), 1),
                                       .recordedAt = fromUnixMilliseconds(sqlite3_column_int64(statement.get(), 2))});
    }

    return metrics;
#else
    static_cast<void>(name);
    static_cast<void>(limit);
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
      SELECT repository_id, worktree_id, relative_path, content_hash, content_hash_algorithm, format_version,
             git_blob_hash, git_blob_hash_algorithm, status, size, indexed_at_unix_ms, deleted
      FROM files
      WHERE worktree_id = ? AND relative_path = ?;
    )sql");

    statement.bindText(1, worktreeId);
    statement.bindPath(2, relativePath);

    if (!statement.stepRow())
      return std::nullopt;

    auto* row = statement.get();
    IndexDocument document{.formatVersion = static_cast<std::uint32_t>(sqlite3_column_int(row, 5)),
                           .repositoryId = reinterpret_cast<const char*>(sqlite3_column_text(row, 0)),
                           .worktreeId = reinterpret_cast<const char*>(sqlite3_column_text(row, 1)),
                           .relativePath = reinterpret_cast<const char*>(sqlite3_column_text(row, 2)),
                           .contentHash = reinterpret_cast<const char*>(sqlite3_column_text(row, 3)),
                           .contentHashAlgorithm = toContentHashAlgorithm(sqlite3_column_int(row, 4)),
                           .gitBlobHash = optionalText(row, 6),
                           .gitBlobHashAlgorithm = toGitObjectHashAlgorithm(sqlite3_column_int(row, 7)),
                           .status = toGitFileStatus(sqlite3_column_int(row, 8)),
                           .size = static_cast<std::uintmax_t>(sqlite3_column_int64(row, 9)),
                           .indexedAt = fromUnixMilliseconds(sqlite3_column_int64(row, 10)),
                           .deleted = sqlite3_column_int(row, 11) != 0};

    return document;
#else
    static_cast<void>(worktreeId);
    static_cast<void>(relativePath);
    throw std::runtime_error("SQLite support is not available in this build");
#endif
  }

} // namespace uburu::storage
