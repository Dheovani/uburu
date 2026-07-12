#include "cli-application.hpp"

#include "cli-file-helper.hpp"
#include "cli-output.hpp"
#include "cli-runtime.hpp"

#include "app/services/search-service.hpp"
#include "core/filesystem/recursive-file-scanner.hpp"
#include "core/index/persistent-index-service.hpp"
#include "core/search/direct-search-engine.hpp"
#include "core/storage/sqlite-storage-service.hpp"
#include "core/storage/storage-paths.hpp"

#include <iostream>
#include <memory>
#include <optional>
#include <vector>

namespace uburu::cli
{

  namespace
  {

    constexpr auto cliDatabaseFileName = "uburu-cli-v1.db";

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
    std::shared_ptr<const uburu::search::SearchEngine> makeDirectSearchEngine()
    {
      auto scanner = std::make_shared<uburu::filesystem::RecursiveFileScanner>();

      return std::make_shared<uburu::search::DirectSearchEngine>(std::move(scanner));
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
    uburu::SearchQuery normalizedSearchQuery(uburu::SearchQuery query)
    {
      query.root = uburu::cli::normalizedAbsolutePath(query.root);

      if (!query.scope.roots.empty())
        query.scope.roots.front().path = query.root;

      return query;
    }

  }

  [[nodiscard]]
  uburu::cli::CliExitCode runSearch(const uburu::cli::CliOptions& options)
  {
    uburu::cli::CliCancellation cancellation;
    const auto engine = makeDirectSearchEngine();
    std::shared_ptr<uburu::storage::SQLiteStorageService> storage;

    if (options.searchStrategy != uburu::cli::CliSearchStrategy::direct)
      storage = makeStorageService(options.databasePath);

    auto indexService = storage ? makeIndexService(*storage) : nullptr;

    uburu::app::SearchServiceOptions serviceOptions;
    serviceOptions.strategy = toSearchServiceStrategy(options.searchStrategy);

    uburu::app::DefaultSearchService service(engine, std::move(indexService), serviceOptions);
    auto query = normalizedSearchQuery(options.query);
    uburu::cli::CliResultStream resultStream(std::cout, options.outputFormat);
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
    const auto worktree = uburu::cli::filesystemWorktree(options.query.root);
    auto storage = makeStorageService(options.databasePath);
    auto indexService = makeIndexService(*storage);
    const auto report = indexService->staleness(worktree);

    uburu::cli::writeIndexStatus(std::cout, report, options.outputFormat);

    return uburu::cli::CliExitCode::ok;
  }

  [[nodiscard]]
  uburu::cli::CliExitCode runIndexRebuild(const uburu::cli::CliOptions& options)
  {
    uburu::cli::CliCancellation cancellation;
    auto storage = makeStorageService(options.databasePath);
    auto indexService = makeIndexService(*storage);
    const auto worktree = uburu::cli::filesystemWorktree(options.query.root);
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

    storage->upsertRepository(uburu::cli::filesystemRepository(worktree));
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

}
