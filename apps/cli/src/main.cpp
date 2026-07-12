#include "cli-options.hpp"
#include "cli-output.hpp"

#include "app/services/search-service.hpp"
#include "core/filesystem/recursive-file-scanner.hpp"
#include "core/index/persistent-index-service.hpp"
#include "core/search/direct-search-engine.hpp"
#include "core/storage/sqlite-storage-service.hpp"
#include "core/storage/storage-paths.hpp"

#include <chrono>
#include <cstddef>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{

  constexpr auto cliDatabaseFileName = "uburu-cli-v1.db";
  constexpr auto cancellationPollInterval = std::chrono::milliseconds{20};
  constexpr std::size_t cliOutputFlushInterval = 64;

  volatile std::sig_atomic_t cancellationSignalRequested = 0;

  void handleInterruptionSignal(int signal)
  {
    if (signal == SIGINT)
      cancellationSignalRequested = 1;
  }

  class CliCancellation final
  {
  public:
    CliCancellation()
    {
      watcher = std::jthread([this](std::stop_token watcherStopToken) {
        watchCancellationSignal(watcherStopToken);
      });
    }

    [[nodiscard]]
    std::stop_token stopToken() const
    {
      return stopSource.get_token();
    }

    [[nodiscard]]
    bool stopRequested() const
    {
      return stopSource.get_token().stop_requested();
    }

    void requestStop()
    {
      stopSource.request_stop();
    }

  private:
    void watchCancellationSignal(std::stop_token watcherStopToken)
    {
      while (!watcherStopToken.stop_requested()) {
        if (cancellationSignalRequested != 0) {
          stopSource.request_stop();

          return;
        }

        std::this_thread::sleep_for(cancellationPollInterval);
      }
    }

    std::stop_source stopSource;
    std::jthread watcher;
  };

  class CliResultStream final
  {
  public:
    CliResultStream(std::ostream& output, uburu::cli::CliOutputFormat format)
      : output(output), format(format)
    {}

    [[nodiscard]]
    bool write(uburu::SearchResult result)
    {
      if (failed)
        return false;

      uburu::cli::writeSearchResult(output, result, format);
      ++pendingResults;

      if (pendingResults >= cliOutputFlushInterval)
        flush();

      failed = failed || !output.good();

      return !failed;
    }

    void flush()
    {
      output.flush();
      pendingResults = 0;
      failed = failed || !output.good();
    }

    [[nodiscard]]
    bool outputFailed() const
    {
      return failed;
    }

  private:
    std::ostream& output;
    uburu::cli::CliOutputFormat format;
    std::size_t pendingResults{0};
    bool failed{false};
  };

#ifdef _WIN32
  void configureConsoleEncoding()
  {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
  }
#else
  void configureConsoleEncoding()
  {}
#endif

  [[nodiscard]]
  std::string pathIdentity(const std::filesystem::path& path)
  {
    return path.lexically_normal().generic_string();
  }

  [[nodiscard]]
  std::filesystem::path normalizedAbsolutePath(const std::filesystem::path& path)
  {
    std::error_code error;
    auto normalized = std::filesystem::absolute(path, error);

    if (!error)
      return normalized.lexically_normal();

    return path.lexically_normal();
  }

  [[nodiscard]]
  uburu::WorktreeInfo filesystemWorktree(const std::filesystem::path& root)
  {
    const auto normalizedRoot = normalizedAbsolutePath(root);
    const auto id = "filesystem:" + pathIdentity(normalizedRoot);

    uburu::WorktreeInfo worktree;
    worktree.id = id;
    worktree.repositoryId = id;
    worktree.root = normalizedRoot;
    worktree.gitDirectory = std::filesystem::path{};
    worktree.headOid = "filesystem";

    return worktree;
  }

  [[nodiscard]]
  uburu::RepositoryInfo filesystemRepository(const uburu::WorktreeInfo& worktree)
  {
    uburu::RepositoryInfo repository;
    repository.id = worktree.repositoryId;
    repository.commonGitDirectory = std::filesystem::path{};
    repository.worktreeRoot = worktree.root;
    repository.headOid = worktree.headOid;

    return repository;
  }

  [[nodiscard]]
  std::shared_ptr<uburu::storage::SQLiteStorageService> makeStorageService(
    const std::optional<std::filesystem::path>& databaseOverride)
  {
    const auto defaultDatabasePath = std::filesystem::current_path() / ".uburu-cli" / cliDatabaseFileName;
    const auto databasePath = databaseOverride.value_or(defaultDatabasePath);

    uburu::storage::ensurePrivateStorageDirectory(databasePath.parent_path());

    auto storage = std::make_shared<uburu::storage::SQLiteStorageService>(databasePath);
    storage->initialize();

    return storage;
  }

  [[nodiscard]]
  std::shared_ptr<uburu::index::PersistentIndexService> makeIndexService(
    uburu::storage::StorageService& storage)
  {
    return std::make_shared<uburu::index::PersistentIndexService>(storage);
  }

  [[nodiscard]]
  uburu::app::SearchStrategy toSearchServiceStrategy(uburu::cli::CliSearchStrategy strategy)
  {
    switch (strategy) {
    case uburu::cli::CliSearchStrategy::direct:
      return uburu::app::SearchStrategy::direct;
    case uburu::cli::CliSearchStrategy::indexed:
      return uburu::app::SearchStrategy::indexed;
    case uburu::cli::CliSearchStrategy::hybrid:
      return uburu::app::SearchStrategy::hybrid;
    }

    return uburu::app::SearchStrategy::direct;
  }

  [[nodiscard]]
  std::vector<std::string_view> argumentsWithoutExecutable(int argc, char** argv)
  {
    std::vector<std::string_view> arguments;

    for (int index = 1; index < argc; ++index)
      arguments.emplace_back(argv[index]);

    return arguments;
  }

  [[nodiscard]]
  int toProcessExitCode(uburu::cli::CliExitCode code)
  {
    return static_cast<int>(code);
  }

  [[nodiscard]]
  uburu::cli::CliExitCode exitCodeForSummary(const uburu::search::SearchSummary& summary)
  {
    if (summary.cancelled)
      return uburu::cli::CliExitCode::cancelled;

    if (!summary.errors.empty())
      return uburu::cli::CliExitCode::searchFailed;

    if (summary.matches == 0)
      return uburu::cli::CliExitCode::noMatches;

    return uburu::cli::CliExitCode::ok;
  }

  [[nodiscard]]
  std::shared_ptr<const uburu::search::SearchEngine> makeDirectSearchEngine()
  {
    auto scanner = std::make_shared<uburu::filesystem::RecursiveFileScanner>();

    return std::make_shared<uburu::search::DirectSearchEngine>(std::move(scanner));
  }

  [[nodiscard]]
  uburu::SearchQuery normalizedSearchQuery(uburu::SearchQuery query)
  {
    query.root = normalizedAbsolutePath(query.root);

    if (!query.scope.roots.empty())
      query.scope.roots.front().path = query.root;

    return query;
  }

  [[nodiscard]]
  uburu::cli::CliExitCode runSearch(const uburu::cli::CliOptions& options)
  {
    CliCancellation cancellation;
    const auto engine = makeDirectSearchEngine();
    std::shared_ptr<uburu::storage::SQLiteStorageService> storage;

    if (options.searchStrategy != uburu::cli::CliSearchStrategy::direct)
      storage = makeStorageService(options.databasePath);

    auto indexService = storage ? makeIndexService(*storage) : nullptr;

    uburu::app::SearchServiceOptions serviceOptions;
    serviceOptions.strategy = toSearchServiceStrategy(options.searchStrategy);

    uburu::app::DefaultSearchService service(engine, std::move(indexService), serviceOptions);
    auto query = normalizedSearchQuery(options.query);
    CliResultStream resultStream(std::cout, options.outputFormat);
    auto summary = service.search(query, [&](uburu::SearchResult result) {
      if (cancellation.stopRequested())
        return false;

      if (resultStream.write(std::move(result)))
        return true;

      cancellation.requestStop();

      return false;
    }, cancellation.stopToken());
    resultStream.flush();

    if (cancellation.stopRequested() || resultStream.outputFailed())
      summary.cancelled = true;

    uburu::cli::writeSearchSummary(std::cout, summary, options.outputFormat);

    return exitCodeForSummary(summary);
  }

  [[nodiscard]]
  uburu::cli::CliExitCode runIndexStatus(const uburu::cli::CliOptions& options)
  {
    const auto worktree = filesystemWorktree(options.query.root);
    auto storage = makeStorageService(options.databasePath);
    auto indexService = makeIndexService(*storage);
    const auto report = indexService->staleness(worktree);

    uburu::cli::writeIndexStatus(std::cout, report, options.outputFormat);

    return uburu::cli::CliExitCode::ok;
  }

  [[nodiscard]]
  uburu::cli::CliExitCode runIndexRebuild(const uburu::cli::CliOptions& options)
  {
    CliCancellation cancellation;
    auto storage = makeStorageService(options.databasePath);
    auto indexService = makeIndexService(*storage);
    const auto worktree = filesystemWorktree(options.query.root);
    uburu::filesystem::RecursiveFileScanner scanner;
    std::vector<uburu::FileEntry> files;

    scanner.scan(
      worktree.root,
      options.query.options,
      [&](uburu::FileEntry file) {
        if (cancellation.stopRequested())
          return false;

        files.push_back(std::move(file));

        return true;
      },
      cancellation.stopToken());

    if (cancellation.stopRequested()) {
      uburu::index::IndexUpdateSummary summary;
      summary.cancelled = true;
      uburu::cli::writeIndexUpdateSummary(std::cout, summary, options.outputFormat);

      return uburu::cli::CliExitCode::cancelled;
    }

    storage->upsertRepository(filesystemRepository(worktree));
    storage->upsertWorktree(worktree);
    auto summary = indexService->update(worktree, files, uburu::index::IndexProgressCallback{}, cancellation.stopToken());

    if (cancellation.stopRequested())
      summary.cancelled = true;

    uburu::cli::writeIndexUpdateSummary(std::cout, summary, options.outputFormat);

    if (summary.cancelled)
      return uburu::cli::CliExitCode::cancelled;

    if (summary.failed > 0)
      return uburu::cli::CliExitCode::searchFailed;

    return uburu::cli::CliExitCode::ok;
  }

} // namespace

int main(int argc, char** argv)
{
  configureConsoleEncoding();
  cancellationSignalRequested = 0;
  std::signal(SIGINT, handleInterruptionSignal);

  auto parseResult = uburu::cli::parseCliOptions(argumentsWithoutExecutable(argc, argv));

  if (!parseResult.options) {
    std::cerr << parseResult.error << "\n\n" << uburu::cli::cliHelpText();

    return toProcessExitCode(uburu::cli::CliExitCode::usageError);
  }

  const auto& options = *parseResult.options;

  if (options.showHelp) {
    std::cout << uburu::cli::cliHelpText();

    return toProcessExitCode(uburu::cli::CliExitCode::ok);
  }

  if (options.command == uburu::cli::CliCommand::search)
    return toProcessExitCode(runSearch(options));

  if (options.command == uburu::cli::CliCommand::indexStatus)
    return toProcessExitCode(runIndexStatus(options));

  if (options.command == uburu::cli::CliCommand::indexRebuild)
    return toProcessExitCode(runIndexRebuild(options));

  std::cout << uburu::cli::cliHelpText();

  return toProcessExitCode(uburu::cli::CliExitCode::ok);
}
