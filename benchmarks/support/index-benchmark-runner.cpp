#include "index-benchmark-runner.hpp"

#include <chrono>
#include <system_error>

namespace uburu::benchmarks
{
  namespace
  {

    constexpr std::uint64_t databaseNameMultiplier = 2'654'435'761U;
    constexpr std::string_view repositoryId = "benchmark-repository";
    constexpr std::string_view worktreeId = "benchmark-worktree";
    constexpr std::string_view mainBranch = "main";
    constexpr std::string_view featureBranch = "feature/benchmark";
    constexpr std::string_view mainHeadOid = "1111111111111111111111111111111111111111";
    constexpr std::string_view featureHeadOid = "2222222222222222222222222222222222222222";

    [[nodiscard]] std::filesystem::path uniqueDatabasePath(std::string_view datasetName)
    {
      const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
      auto fileName = std::string{"uburu-index-benchmark-"};
      fileName += datasetName;
      fileName += "-";
      fileName += std::to_string(static_cast<std::uint64_t>(stamp) * databaseNameMultiplier);
      fileName += ".db";

      return std::filesystem::temp_directory_path() / fileName;
    }

    [[nodiscard]] RepositoryInfo makeRepositoryInfo(const BenchmarkDataset& dataset)
    {
      return RepositoryInfo{.id = std::string{repositoryId},
                            .commonGitDirectory = dataset.root / ".git",
                            .worktreeRoot = dataset.root,
                            .currentBranch = std::string{mainBranch},
                            .headOid = std::string{mainHeadOid},
                            .detachedHead = false};
    }

    [[nodiscard]] WorktreeInfo makeWorktreeInfo(const BenchmarkDataset& dataset)
    {
      return WorktreeInfo{.id = std::string{worktreeId},
                          .repositoryId = std::string{repositoryId},
                          .root = dataset.root,
                          .gitDirectory = dataset.root / ".git",
                          .branch = std::string{mainBranch},
                          .headOid = std::string{mainHeadOid},
                          .locked = false,
                          .prunable = false,
                          .lockReason = {}};
    }

    [[nodiscard]] FileEntry makeFileEntry(const std::filesystem::path& root, const std::filesystem::path& path)
    {
      return FileEntry{.absolutePath = path,
                       .relativePath = std::filesystem::relative(path, root),
                       .size = std::filesystem::file_size(path),
                       .modifiedAt = std::filesystem::last_write_time(path),
                       .searchRoot = root};
    }

    [[nodiscard]] std::vector<FileEntry> collectFiles(const BenchmarkDataset& dataset)
    {
      std::vector<FileEntry> files;

      for (const auto& entry : std::filesystem::recursive_directory_iterator(dataset.root)) {
        if (!entry.is_regular_file())
          continue;

        files.push_back(makeFileEntry(dataset.root, entry.path()));
      }

      return files;
    }

  } // namespace

  IndexBenchmarkContext::IndexBenchmarkContext(const BenchmarkDataset& dataset)
    : benchmarkDataset(&dataset), databasePath(uniqueDatabasePath(dataset.name)), files(collectFiles(dataset)),
      storage(databasePath), indexService(storage), repository(makeRepositoryInfo(dataset)),
      currentWorktree(makeWorktreeInfo(dataset))
  {
    storage.initialize();
    storage.upsertRepository(repository);
    storage.upsertWorktree(currentWorktree);
  }

  IndexBenchmarkContext::~IndexBenchmarkContext()
  {
    std::error_code error;

    std::filesystem::remove(databasePath, error);
    std::filesystem::remove(databasePath.string() + "-wal", error);
    std::filesystem::remove(databasePath.string() + "-shm", error);
  }

  const BenchmarkDataset& IndexBenchmarkContext::dataset() const
  {
    return *benchmarkDataset;
  }

  const WorktreeInfo& IndexBenchmarkContext::worktree() const
  {
    return currentWorktree;
  }

  WorktreeInfo IndexBenchmarkContext::branchSwitchWorktree() const
  {
    auto switched = currentWorktree;
    switched.branch = std::string{featureBranch};
    switched.headOid = std::string{featureHeadOid};

    return switched;
  }

  IndexBenchmarkResult IndexBenchmarkContext::update(const WorktreeInfo& targetWorktree)
  {
    IndexBenchmarkResult result;
    const auto before = std::chrono::steady_clock::now();

    result.summary = indexService.update(targetWorktree, files, [&](const auto&) { ++result.progressEvents; });
    result.elapsed = std::chrono::steady_clock::now() - before;

    return result;
  }

  IndexBenchmarkResult IndexBenchmarkContext::updateAfterBranchSwitch()
  {
    const auto switched = branchSwitchWorktree();
    auto report = indexService.staleness(switched);
    auto result = update(switched);
    result.stalenessChecked = true;
    result.stale = report.state == index::IndexStalenessState::stale;
    result.headChanged = report.headChanged;
    result.branchChanged = report.branchChanged;

    return result;
  }

  std::unique_ptr<IndexBenchmarkContext> makeIndexBenchmarkContext(const BenchmarkDataset& dataset)
  {
    return std::make_unique<IndexBenchmarkContext>(dataset);
  }

} // namespace uburu::benchmarks
