#include <sqlite3.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{

  constexpr int sqliteOk = SQLITE_OK;
  constexpr int sqliteDone = SQLITE_DONE;
  constexpr int sqliteRow = SQLITE_ROW;
  constexpr int sqliteReadUntilNullTerminator = -1;
  constexpr int sqliteFirstParameter = 1;
  constexpr int sqliteSecondParameter = 2;
  constexpr std::int64_t documentCount = 20000;
  constexpr std::int64_t matchingDocumentModulo = 7;
  constexpr int queryRepeatCount = 30;
  constexpr std::string_view benchmarkNeedle = "needle";
  constexpr std::string_view likeNeedlePattern = "%needle%";

  [[nodiscard]] std::string sqliteMessage(sqlite3* database, std::string_view operation)
  {
    return std::string{operation} + ": " + sqlite3_errmsg(database);
  }

  void requireSqlite(int result, sqlite3* database, std::string_view operation)
  {
    if (result == sqliteOk)
      return;

    throw std::runtime_error(sqliteMessage(database, operation));
  }

  void requireDone(int result, sqlite3* database, std::string_view operation)
  {
    if (result == sqliteDone)
      return;

    throw std::runtime_error(sqliteMessage(database, operation));
  }

  class Database
  {
  public:
    explicit Database(const std::filesystem::path& path)
    {
      const auto result = sqlite3_open(path.string().c_str(), &database);
      requireSqlite(result, database, "failed to open benchmark database");
    }

    ~Database()
    {
      sqlite3_close(database);
    }

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) noexcept = delete;
    Database& operator=(Database&&) noexcept = delete;

    [[nodiscard]] sqlite3* get() const
    {
      return database;
    }

  private:
    sqlite3* database{nullptr};
  };

  class Statement
  {
  public:
    Statement(sqlite3* database, std::string_view sql) : database(database)
    {
      const auto result =
          sqlite3_prepare_v2(database, std::string{sql}.c_str(), sqliteReadUntilNullTerminator, &statement, nullptr);
      requireSqlite(result, database, "failed to prepare benchmark statement");
    }

    ~Statement()
    {
      sqlite3_finalize(statement);
    }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&&) noexcept = delete;
    Statement& operator=(Statement&&) noexcept = delete;

    void reset()
    {
      requireSqlite(sqlite3_reset(statement), database, "failed to reset benchmark statement");
      requireSqlite(sqlite3_clear_bindings(statement), database, "failed to clear benchmark bindings");
    }

    void bindText(int index, std::string_view value)
    {
      const auto result =
          sqlite3_bind_text(statement, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
      requireSqlite(result, database, "failed to bind benchmark text");
    }

    void bindInt64(int index, std::int64_t value)
    {
      const auto result = sqlite3_bind_int64(statement, index, value);
      requireSqlite(result, database, "failed to bind benchmark integer");
    }

    [[nodiscard]] sqlite3_stmt* get() const
    {
      return statement;
    }

  private:
    sqlite3* database{nullptr};
    sqlite3_stmt* statement{nullptr};
  };

  void execute(sqlite3* database, std::string_view sql)
  {
    char* errorMessage = nullptr;
    const auto result = sqlite3_exec(database, std::string{sql}.c_str(), nullptr, nullptr, &errorMessage);

    if (result == sqliteOk)
      return;

    std::string message = "SQLite benchmark execution failed";
    if (errorMessage != nullptr) {
      message += ": ";
      message += errorMessage;
    }

    sqlite3_free(errorMessage);
    throw std::runtime_error(message);
  }

  [[nodiscard]] bool tryExecute(sqlite3* database, std::string_view sql)
  {
    char* errorMessage = nullptr;
    const auto result = sqlite3_exec(database, std::string{sql}.c_str(), nullptr, nullptr, &errorMessage);
    sqlite3_free(errorMessage);

    return result == sqliteOk;
  }

  [[nodiscard]] std::string documentContent(std::int64_t index)
  {
    if (index % matchingDocumentModulo == 0)
      return "alpha beta needle gamma deterministic benchmark document";

    return "alpha beta gamma deterministic benchmark document";
  }

  void insertDataset(sqlite3* database)
  {
    execute(database, "BEGIN IMMEDIATE");

    Statement insertDocument(database, "INSERT INTO documents(id, content) VALUES (?, ?)");
    Statement insertFtsDocument(database, "INSERT INTO documents_fts(rowid, content) VALUES (?, ?)");

    for (std::int64_t index = 0; index < documentCount; ++index) {
      const auto content = documentContent(index);
      const auto rowId = index + 1;

      insertDocument.reset();
      insertDocument.bindInt64(sqliteFirstParameter, rowId);
      insertDocument.bindText(sqliteSecondParameter, content);
      requireDone(sqlite3_step(insertDocument.get()), database, "failed to insert benchmark document");

      insertFtsDocument.reset();
      insertFtsDocument.bindInt64(sqliteFirstParameter, rowId);
      insertFtsDocument.bindText(sqliteSecondParameter, content);
      requireDone(sqlite3_step(insertFtsDocument.get()), database, "failed to insert benchmark FTS document");
    }

    execute(database, "COMMIT");
  }

  [[nodiscard]] std::int64_t countMatches(Statement& statement, std::string_view parameter)
  {
    statement.reset();
    statement.bindText(sqliteFirstParameter, parameter);

    if (sqlite3_step(statement.get()) != sqliteRow)
      throw std::runtime_error("benchmark count query returned no row");

    const auto count = sqlite3_column_int64(statement.get(), 0);

    if (sqlite3_step(statement.get()) != sqliteDone)
      throw std::runtime_error("benchmark count query returned more than one row");

    return count;
  }

  template <typename Callback> [[nodiscard]] std::chrono::microseconds measureRepeated(Callback&& callback)
  {
    callback();

    const auto startedAt = std::chrono::steady_clock::now();

    for (int repeat = 0; repeat < queryRepeatCount; ++repeat)
      callback();

    const auto finishedAt = std::chrono::steady_clock::now();

    return std::chrono::duration_cast<std::chrono::microseconds>(finishedAt - startedAt);
  }

  [[nodiscard]] std::filesystem::path benchmarkDatabasePath()
  {
    return std::filesystem::temp_directory_path() / "uburu-storage-fts5-benchmark.db";
  }

  int runBenchmark()
  {
    const auto databasePath = benchmarkDatabasePath();
    std::filesystem::remove(databasePath);

    Database database(databasePath);
    execute(database.get(), "PRAGMA journal_mode = WAL");
    execute(database.get(), "PRAGMA synchronous = NORMAL");
    execute(database.get(), "CREATE TABLE documents(id INTEGER PRIMARY KEY, content TEXT NOT NULL)");

    if (!tryExecute(database.get(), "CREATE VIRTUAL TABLE documents_fts USING fts5(content)")) {
      std::cout << "FTS5 unavailable in this SQLite build; benchmark skipped.\n";

      return 0;
    }

    insertDataset(database.get());

    Statement likeQuery(database.get(), "SELECT count(*) FROM documents WHERE content LIKE ?");
    Statement ftsQuery(database.get(), "SELECT count(*) FROM documents_fts WHERE documents_fts MATCH ?");

    const auto likeMatches = countMatches(likeQuery, likeNeedlePattern);
    const auto ftsMatches = countMatches(ftsQuery, benchmarkNeedle);

    if (likeMatches != ftsMatches)
      throw std::runtime_error("LIKE and FTS5 returned different match counts");

    const auto likeTime = measureRepeated([&likeQuery, likeMatches] {
      if (countMatches(likeQuery, likeNeedlePattern) != likeMatches)
        throw std::runtime_error("LIKE benchmark query returned an unstable match count");
    });
    const auto ftsTime = measureRepeated([&ftsQuery, ftsMatches] {
      if (countMatches(ftsQuery, benchmarkNeedle) != ftsMatches)
        throw std::runtime_error("FTS5 benchmark query returned an unstable match count");
    });

    std::cout << "dataset_documents=" << documentCount << '\n';
    std::cout << "matching_documents=" << likeMatches << '\n';
    std::cout << "repeats=" << queryRepeatCount << '\n';
    std::cout << "like_total_us=" << likeTime.count() << '\n';
    std::cout << "fts5_total_us=" << ftsTime.count() << '\n';

    return 0;
  }

} // namespace

int main()
{
  try {
    return runBenchmark();
  } catch (const std::exception& exception) {
    std::cerr << exception.what() << '\n';

    return 1;
  }
}
