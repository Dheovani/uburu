#pragma once

#include "benchmark-dataset.hpp"
#include "core/index/persistent-index-service.hpp"
#include "core/storage/sqlite-storage-service.hpp"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <vector>

namespace uburu::benchmarks
{

  struct IndexBenchmarkResult
  {
    index::IndexUpdateSummary summary;
    std::chrono::nanoseconds elapsed{};
    std::size_t progressEvents{0};
    bool stalenessChecked{false};
    bool stale{false};
    bool headChanged{false};
    bool branchChanged{false};
  };

  class IndexBenchmarkContext
  {
  public:
    explicit IndexBenchmarkContext(const BenchmarkDataset& dataset);
    ~IndexBenchmarkContext();

    IndexBenchmarkContext(const IndexBenchmarkContext&) = delete;
    IndexBenchmarkContext& operator=(const IndexBenchmarkContext&) = delete;
    IndexBenchmarkContext(IndexBenchmarkContext&&) noexcept = delete;
    IndexBenchmarkContext& operator=(IndexBenchmarkContext&&) noexcept = delete;

    [[nodiscard]] const BenchmarkDataset& dataset() const;
    [[nodiscard]] const WorktreeInfo& worktree() const;
    [[nodiscard]] WorktreeInfo branchSwitchWorktree() const;

    [[nodiscard]] IndexBenchmarkResult update(const WorktreeInfo& targetWorktree);
    [[nodiscard]] IndexBenchmarkResult updateAfterBranchSwitch();

  private:
    const BenchmarkDataset* benchmarkDataset{nullptr};
    std::filesystem::path databasePath;
    std::vector<FileEntry> files;
    storage::SQLiteStorageService storage;
    index::PersistentIndexService indexService;
    RepositoryInfo repository;
    WorktreeInfo currentWorktree;
  };

  [[nodiscard]] std::unique_ptr<IndexBenchmarkContext> makeIndexBenchmarkContext(const BenchmarkDataset& dataset);

} // namespace uburu::benchmarks
