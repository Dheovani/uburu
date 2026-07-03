#include "app/services/indexing-service.hpp"
#include "helpers/temporary-paths.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <optional>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace
{

  [[nodiscard]] uburu::WorktreeInfo worktreeInfo()
  {
    const auto root = uburu::tests::uniqueTemporaryPath("uburu-indexing-service-test");

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

  [[nodiscard]] uburu::FileEntry fileEntry(const uburu::WorktreeInfo& worktree,
                                           const std::filesystem::path& relativePath)
  {
    return uburu::FileEntry{.absolutePath = worktree.root / relativePath,
                            .relativePath = relativePath,
                            .size = 42,
                            .modifiedAt = std::filesystem::file_time_type{std::chrono::seconds{4}},
                            .searchRoot = worktree.root};
  }

  class FakeScanner final : public uburu::filesystem::FileScanner
  {
  public:
    void scan(const std::filesystem::path& root,
              const uburu::SearchOptions& options,
              uburu::filesystem::FileSink sink,
              std::stop_token stopToken = {},
              uburu::diagnostics::SearchMetrics* metrics = nullptr) const override
    {
      static_cast<void>(options);
      static_cast<void>(metrics);

      scannedRoot = root;
      ++scanCount;

      for (const auto& file : files) {
        if (stopToken.stop_requested())
          return;

        if (!sink(file))
          return;
      }
    }

    mutable std::filesystem::path scannedRoot;
    mutable std::size_t scanCount{0};
    std::vector<uburu::FileEntry> files;
  };

  class FakeGitService final : public uburu::git::GitService
  {
  public:
    [[nodiscard]] uburu::git::GitResult<uburu::RepositoryInfo>
    discoverRepository(const std::filesystem::path& path) const override
    {
      static_cast<void>(path);

      return unavailable();
    }

    [[nodiscard]] uburu::git::GitResult<std::vector<uburu::WorktreeInfo>>
    listWorktrees(const uburu::RepositoryInfo& repository) const override
    {
      static_cast<void>(repository);

      return unavailable();
    }

    [[nodiscard]] uburu::git::GitResult<uburu::GitFileStatus>
    fileStatus(const uburu::WorktreeInfo& worktree, const std::filesystem::path& relativePath) const override
    {
      static_cast<void>(worktree);
      static_cast<void>(relativePath);

      return unavailable();
    }

    [[nodiscard]] uburu::git::GitResult<std::optional<std::string>>
    blobHash(const uburu::WorktreeInfo& worktree, const std::filesystem::path& relativePath) const override
    {
      static_cast<void>(worktree);
      static_cast<void>(relativePath);

      return unavailable();
    }

    [[nodiscard]] uburu::git::GitResult<std::vector<uburu::GitOverlayEntry>>
    workingTreeOverlay(const uburu::WorktreeInfo& worktree) const override
    {
      requestedWorktree = worktree;

      if (failOverlay)
        return unavailable();

      return overlay;
    }

    [[nodiscard]] uburu::git::GitResult<uburu::GitRepositoryBoundary>
    repositoryBoundary(const uburu::WorktreeInfo& worktree, const std::filesystem::path& relativePath) const override
    {
      static_cast<void>(worktree);
      static_cast<void>(relativePath);

      return unavailable();
    }

    [[nodiscard]] uburu::git::GitResult<uburu::GitObjectHashAlgorithm>
    objectHashAlgorithm(const uburu::RepositoryInfo& repository) const override
    {
      static_cast<void>(repository);

      return unavailable();
    }

    [[nodiscard]] uburu::git::GitResult<uburu::git::GitChangeState>
    changeState(const uburu::WorktreeInfo& worktree) const override
    {
      static_cast<void>(worktree);

      return unavailable();
    }

    [[nodiscard]] static uburu::git::GitError unavailable()
    {
      return uburu::git::GitError{.code = uburu::git::GitErrorCode::backendUnavailable, .message = "unavailable"};
    }

    mutable std::optional<uburu::WorktreeInfo> requestedWorktree;
    std::vector<uburu::GitOverlayEntry> overlay;
    bool failOverlay{false};
  };

  class FakeIndexService final : public uburu::index::IndexService
  {
  public:
    [[nodiscard]] uburu::index::IndexUpdateSummary update(const uburu::WorktreeInfo& worktree,
                                                          std::span<const uburu::FileEntry> files,
                                                          const uburu::index::IndexProgressCallback& onProgress = {},
                                                          std::stop_token stopToken = {}) override
    {
      static_cast<void>(worktree);
      static_cast<void>(files);
      static_cast<void>(onProgress);
      static_cast<void>(stopToken);

      return {};
    }

    [[nodiscard]] uburu::index::IndexUpdateSummary update(const uburu::WorktreeInfo& worktree,
                                                          std::span<const uburu::index::IndexFileCandidate> files,
                                                          const uburu::index::IndexProgressCallback& onProgress = {},
                                                          std::stop_token stopToken = {}) override
    {
      static_cast<void>(worktree);
      static_cast<void>(files);
      static_cast<void>(onProgress);
      static_cast<void>(stopToken);

      return {};
    }

    [[nodiscard]] uburu::index::IndexUpdateSummary update(const uburu::WorktreeInfo& worktree,
                                                          std::span<const uburu::FileEntry> files,
                                                          std::span<const uburu::GitOverlayEntry> overlay,
                                                          const uburu::index::IndexProgressCallback& onProgress = {},
                                                          std::stop_token stopToken = {}) override
    {
      requestedWorktree = worktree;
      receivedFiles.assign(files.begin(), files.end());
      receivedOverlay.assign(overlay.begin(), overlay.end());
      static_cast<void>(onProgress);
      static_cast<void>(stopToken);

      uburu::index::IndexUpdateSummary summary;
      summary.indexed = receivedFiles.size();

      return summary;
    }

    [[nodiscard]] std::vector<uburu::SearchResult> search(const uburu::SearchQuery& query,
                                                          std::stop_token stopToken = {}) const override
    {
      static_cast<void>(query);
      static_cast<void>(stopToken);

      return {};
    }

    [[nodiscard]] uburu::index::IndexStalenessReport staleness(const uburu::WorktreeInfo& worktree) const override
    {
      static_cast<void>(worktree);

      uburu::index::IndexStalenessReport report;
      report.state = uburu::index::IndexStalenessState::fresh;

      return report;
    }

    std::optional<uburu::WorktreeInfo> requestedWorktree;
    std::vector<uburu::FileEntry> receivedFiles;
    std::vector<uburu::GitOverlayEntry> receivedOverlay;
  };

} // namespace

TEST_CASE("default indexing service scans files and passes git overlay to the index")
{
  const auto worktree = worktreeInfo();
  auto scanner = std::make_shared<FakeScanner>();
  auto gitService = std::make_shared<FakeGitService>();
  auto indexService = std::make_shared<FakeIndexService>();

  scanner->files.push_back(fileEntry(worktree, "src/main.cpp"));
  gitService->overlay.push_back(uburu::GitOverlayEntry{
    .relativePath = "src/main.cpp",
    .status = uburu::GitFileStatus::modified,
    .disposition = uburu::GitOverlayDisposition::replaceWithWorkingTree,
  });

  uburu::app::DefaultIndexingService service(scanner, gitService, indexService);
  const auto summary = service.update(worktree, uburu::SearchOptions{});

  CHECK(summary.indexed == 1);
  CHECK(scanner->scannedRoot == worktree.root);
  REQUIRE(gitService->requestedWorktree.has_value());
  CHECK(gitService->requestedWorktree->id == worktree.id);
  REQUIRE(indexService->requestedWorktree.has_value());
  CHECK(indexService->requestedWorktree->id == worktree.id);
  REQUIRE(indexService->receivedFiles.size() == 1);
  CHECK(indexService->receivedFiles.front().relativePath == std::filesystem::path("src/main.cpp"));
  REQUIRE(indexService->receivedOverlay.size() == 1);
  CHECK(indexService->receivedOverlay.front().status == uburu::GitFileStatus::modified);
}

TEST_CASE("default indexing service does not publish a generation when git overlay fails")
{
  const auto worktree = worktreeInfo();
  auto scanner = std::make_shared<FakeScanner>();
  auto gitService = std::make_shared<FakeGitService>();
  auto indexService = std::make_shared<FakeIndexService>();

  scanner->files.push_back(fileEntry(worktree, "src/main.cpp"));
  gitService->failOverlay = true;

  uburu::app::DefaultIndexingService service(scanner, gitService, indexService);
  const auto summary = service.update(worktree, uburu::SearchOptions{});

  CHECK(summary.failed == 1);
  CHECK_FALSE(indexService->requestedWorktree.has_value());
}

TEST_CASE("default indexing service observes cancellation before git overlay")
{
  const auto worktree = worktreeInfo();
  auto scanner = std::make_shared<FakeScanner>();
  auto gitService = std::make_shared<FakeGitService>();
  auto indexService = std::make_shared<FakeIndexService>();
  std::stop_source stopSource;

  scanner->files.push_back(fileEntry(worktree, "src/main.cpp"));
  stopSource.request_stop();

  uburu::app::DefaultIndexingService service(scanner, gitService, indexService);
  const auto summary = service.update(worktree, uburu::SearchOptions{}, {}, stopSource.get_token());

  CHECK(summary.cancelled);
  CHECK_FALSE(gitService->requestedWorktree.has_value());
  CHECK_FALSE(indexService->requestedWorktree.has_value());
}

TEST_CASE("default indexing service ignores empty watcher reconciliation batches")
{
  const auto worktree = worktreeInfo();
  auto scanner = std::make_shared<FakeScanner>();
  auto gitService = std::make_shared<FakeGitService>();
  auto indexService = std::make_shared<FakeIndexService>();

  uburu::app::DefaultIndexingService service(scanner, gitService, indexService);
  const auto summary = service.reconcile(worktree, uburu::SearchOptions{}, uburu::filesystem::FileChangeBatch{});

  CHECK(summary.indexed == 0);
  CHECK(scanner->scanCount == 0);
  CHECK_FALSE(gitService->requestedWorktree.has_value());
  CHECK_FALSE(indexService->requestedWorktree.has_value());
}

TEST_CASE("default indexing service reconciles watcher batches as a single index update")
{
  const auto worktree = worktreeInfo();
  auto scanner = std::make_shared<FakeScanner>();
  auto gitService = std::make_shared<FakeGitService>();
  auto indexService = std::make_shared<FakeIndexService>();
  const uburu::filesystem::FileChangeBatch batch{
    .events = {uburu::filesystem::FileChangeEvent{
      .relativePath = "src/main.cpp",
      .kind = uburu::filesystem::FileChangeKind::modified,
      .directory = false,
    }},
    .eventsMayBeIncomplete = false,
    .requiresRescan = false,
  };

  scanner->files.push_back(fileEntry(worktree, "src/main.cpp"));

  uburu::app::DefaultIndexingService service(scanner, gitService, indexService);
  const auto summary = service.reconcile(worktree, uburu::SearchOptions{}, batch);

  CHECK(summary.indexed == 1);
  CHECK(scanner->scanCount == 1);
  REQUIRE(indexService->requestedWorktree.has_value());
  CHECK(indexService->requestedWorktree->id == worktree.id);
}

TEST_CASE("default indexing service pauses and resumes indexing work")
{
  const auto worktree = worktreeInfo();
  auto scanner = std::make_shared<FakeScanner>();
  auto gitService = std::make_shared<FakeGitService>();
  auto indexService = std::make_shared<FakeIndexService>();

  scanner->files.push_back(fileEntry(worktree, "src/main.cpp"));

  uburu::app::DefaultIndexingService service(scanner, gitService, indexService);

  service.pause();
  const auto pausedSummary = service.update(worktree, uburu::SearchOptions{});

  CHECK(service.state() == uburu::app::IndexingServiceState::paused);
  CHECK(pausedSummary.cancelled);
  CHECK(scanner->scanCount == 0);
  CHECK_FALSE(indexService->requestedWorktree.has_value());

  service.resume();
  const auto resumedSummary = service.update(worktree, uburu::SearchOptions{});

  CHECK(service.state() == uburu::app::IndexingServiceState::running);
  CHECK(resumedSummary.indexed == 1);
  CHECK(scanner->scanCount == 1);
  REQUIRE(indexService->requestedWorktree.has_value());
  CHECK(indexService->requestedWorktree->id == worktree.id);
}

TEST_CASE("default indexing service runs manual reindex through the update pipeline")
{
  const auto worktree = worktreeInfo();
  auto scanner = std::make_shared<FakeScanner>();
  auto gitService = std::make_shared<FakeGitService>();
  auto indexService = std::make_shared<FakeIndexService>();

  scanner->files.push_back(fileEntry(worktree, "src/manual.cpp"));

  uburu::app::DefaultIndexingService service(scanner, gitService, indexService);
  const auto summary = service.requestManualReindex(worktree, uburu::SearchOptions{});

  CHECK(summary.indexed == 1);
  CHECK(scanner->scanCount == 1);
  REQUIRE(indexService->requestedWorktree.has_value());
  CHECK(indexService->receivedFiles.front().relativePath == std::filesystem::path("src/manual.cpp"));
}

TEST_CASE("default indexing service rejects missing dependencies")
{
  auto scanner = std::make_shared<FakeScanner>();
  auto gitService = std::make_shared<FakeGitService>();
  auto indexService = std::make_shared<FakeIndexService>();

  CHECK_THROWS_AS(uburu::app::DefaultIndexingService(nullptr, gitService, indexService), std::invalid_argument);
  CHECK_THROWS_AS(uburu::app::DefaultIndexingService(scanner, nullptr, indexService), std::invalid_argument);
  CHECK_THROWS_AS(uburu::app::DefaultIndexingService(scanner, gitService, nullptr), std::invalid_argument);
}
