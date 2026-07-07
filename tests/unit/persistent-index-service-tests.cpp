#include "core/index/persistent-index-service.hpp"
#include "core/storage/sqlite-storage-service.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stop_token>
#include <string>
#include <string_view>
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

  void writeFile(const std::filesystem::path& path, std::string_view content)
  {
    std::filesystem::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::binary);
    file << content;
  }

  [[nodiscard]] uburu::FileEntry fileEntry(const std::filesystem::path& root, const std::filesystem::path& relativePath)
  {
    const auto absolutePath = root / relativePath;

    return uburu::FileEntry{.absolutePath = absolutePath,
                            .relativePath = relativePath,
                            .size = std::filesystem::file_size(absolutePath),
                            .modifiedAt = std::filesystem::last_write_time(absolutePath),
                            .searchRoot = root};
  }

  [[nodiscard]] uburu::IndexDocument
  indexedDocument(std::string contentHash, std::string gitBlobHash, std::filesystem::path relativePath)
  {
    uburu::IndexDocument document;
    document.repositoryId = "repository-id";
    document.worktreeId = "worktree-id";
    document.relativePath = std::move(relativePath);
    document.contentHash = std::move(contentHash);
    document.contentHashAlgorithm = uburu::ContentHashAlgorithm::sha256;
    document.gitBlobHash = std::move(gitBlobHash);
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

} // namespace

TEST_CASE("persistent index service publishes an initial generation with content hashes")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-persistent-index-initial-test");
  const auto root = directory.path() / "repo";

  writeFile(root / "src" / "a.txt", "same content");
  writeFile(root / "src" / "b.txt", "same content");

  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");
  storage.initialize();
  storage.upsertRepository(repositoryInfo(root));
  storage.upsertWorktree(worktreeInfo(root));

  uburu::index::PersistentIndexService indexService(storage);
  std::vector<uburu::index::IndexUpdateProgress> progressEvents;
  const std::vector files{
    fileEntry(root, "src/a.txt"),
    fileEntry(root, "src/b.txt"),
  };

  const auto summary =
    indexService.update(worktreeInfo(root), files, [&](const auto& progress) { progressEvents.push_back(progress); });

  const auto first = storage.findDocument("worktree-id", "src/a.txt");
  const auto second = storage.findDocument("worktree-id", "src/b.txt");

  CHECK_FALSE(summary.cancelled);
  CHECK(summary.failed == 0);
  CHECK(summary.indexed == 1);
  CHECK(summary.reusedByHash == 1);
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());
  CHECK(first->contentHash == second->contentHash);
  CHECK(first->contentHashAlgorithm == uburu::ContentHashAlgorithm::sha256);
  CHECK(second->contentHashAlgorithm == uburu::ContentHashAlgorithm::sha256);
  REQUIRE(progressEvents.size() == 2);
  CHECK(progressEvents.back().processed == 2);
  CHECK(progressEvents.back().total == 2);
  CHECK(progressEvents.back().indexed == 1);
  CHECK(progressEvents.back().reusedByHash == 1);
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("persistent index service reuses unchanged catalog entries without rehashing")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-persistent-index-catalog-reuse-test");
  const auto root = directory.path() / "repo";

  writeFile(root / "src" / "stable.txt", "stable content");

  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");
  storage.initialize();
  storage.upsertRepository(repositoryInfo(root));
  storage.upsertWorktree(worktreeInfo(root));

  uburu::index::PersistentIndexService indexService(storage);
  const std::vector files{
    fileEntry(root, "src/stable.txt"),
  };

  const auto initialSummary = indexService.update(worktreeInfo(root), files);
  const auto incrementalSummary = indexService.update(worktreeInfo(root), files);
  const auto document = storage.findDocument("worktree-id", "src/stable.txt");

  CHECK(initialSummary.indexed == 1);
  CHECK(initialSummary.reusedByCatalog == 0);
  CHECK(incrementalSummary.indexed == 0);
  CHECK(incrementalSummary.reusedByCatalog == 1);
  CHECK(incrementalSummary.reusedByHash == 0);
  REQUIRE(document.has_value());
  CHECK(document->modifiedAt == files.front().modifiedAt);
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("persistent index service reindexes git modified files even when size and mtime match")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-persistent-index-modified-overlay-test");
  const auto root = directory.path() / "repo";

  writeFile(root / "src" / "tracked.txt", "aaaa");

  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");
  storage.initialize();
  storage.upsertRepository(repositoryInfo(root));
  storage.upsertWorktree(worktreeInfo(root));

  uburu::index::PersistentIndexService indexService(storage);
  const auto cleanFile = fileEntry(root, "src/tracked.txt");
  const std::vector cleanFiles{
    cleanFile,
  };

  const auto initialSummary = indexService.update(worktreeInfo(root), cleanFiles);
  const auto initialDocument = storage.findDocument("worktree-id", "src/tracked.txt");

  writeFile(root / "src" / "tracked.txt", "bbbb");
  std::filesystem::last_write_time(root / "src" / "tracked.txt", cleanFile.modifiedAt);

  auto modifiedFile = fileEntry(root, "src/tracked.txt");
  modifiedFile.modifiedAt = cleanFile.modifiedAt;

  const std::vector modifiedCandidates{
    uburu::index::IndexFileCandidate{
      .file = modifiedFile,
      .metadata = uburu::index::IndexFileMetadata{.status = uburu::GitFileStatus::modified, .gitBlob = {}},
    },
  };

  const auto modifiedSummary = indexService.update(worktreeInfo(root), modifiedCandidates);
  const auto modifiedDocument = storage.findDocument("worktree-id", "src/tracked.txt");

  CHECK(initialSummary.indexed == 1);
  REQUIRE(initialDocument.has_value());
  CHECK(modifiedSummary.indexed == 1);
  CHECK(modifiedSummary.reusedByCatalog == 0);
  REQUIRE(modifiedDocument.has_value());
  CHECK(modifiedDocument->status == uburu::GitFileStatus::modified);
  CHECK(modifiedDocument->contentHash != initialDocument->contentHash);
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("persistent index service publishes deleted git overlay tombstones")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-persistent-index-deleted-overlay-test");
  const auto root = directory.path() / "repo";

  writeFile(root / "src" / "deleted.txt", "indexed before deletion");

  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");
  storage.initialize();
  storage.upsertRepository(repositoryInfo(root));
  storage.upsertWorktree(worktreeInfo(root));

  uburu::index::PersistentIndexService indexService(storage);
  const auto trackedFile = fileEntry(root, "src/deleted.txt");
  const std::vector trackedFiles{
    trackedFile,
  };

  const auto initialSummary = indexService.update(worktreeInfo(root), trackedFiles);
  const auto initialDocument = storage.findDocument("worktree-id", "src/deleted.txt");

  std::filesystem::remove(root / "src" / "deleted.txt");

  const std::vector deletedCandidates{
    uburu::index::IndexFileCandidate{
      .file = trackedFile,
      .metadata = uburu::index::IndexFileMetadata{.status = uburu::GitFileStatus::deleted, .gitBlob = {}},
    },
  };

  const auto deletedSummary = indexService.update(worktreeInfo(root), deletedCandidates);
  const auto deletedDocument = storage.findDocument("worktree-id", "src/deleted.txt");

  CHECK(initialSummary.indexed == 1);
  REQUIRE(initialDocument.has_value());
  CHECK(deletedSummary.removed == 1);
  CHECK(deletedSummary.indexed == 0);
  CHECK(deletedSummary.failed == 0);
  REQUIRE(deletedDocument.has_value());
  CHECK(deletedDocument->deleted);
  CHECK(deletedDocument->status == uburu::GitFileStatus::deleted);
  CHECK(deletedDocument->contentHash == initialDocument->contentHash);
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("persistent index service updates from scanned files and git overlay entries")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-persistent-index-overlay-update-test");
  const auto root = directory.path() / "repo";

  writeFile(root / "src" / "previous.cpp", "old content");

  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");
  storage.initialize();
  storage.upsertRepository(repositoryInfo(root));
  storage.upsertWorktree(worktreeInfo(root));

  uburu::index::PersistentIndexService indexService(storage);
  const std::vector initialFiles{
    fileEntry(root, "src/previous.cpp"),
  };

  const auto initialSummary = indexService.update(worktreeInfo(root), initialFiles);
  const auto initialDocument = storage.findDocument("worktree-id", "src/previous.cpp");

  writeFile(root / "src" / "current.cpp", "new content");
  std::filesystem::remove(root / "src" / "previous.cpp");

  const std::vector scannedFiles{
    fileEntry(root, "src/current.cpp"),
  };
  const std::vector overlay{
    uburu::GitOverlayEntry{.relativePath = "src/current.cpp",
                           .previousRelativePath = std::filesystem::path("src/previous.cpp"),
                           .status = uburu::GitFileStatus::modified,
                           .disposition = uburu::GitOverlayDisposition::replaceWithWorkingTree,
                           .reusableBlob = {}},
  };

  const auto overlaySummary = indexService.update(worktreeInfo(root), scannedFiles, overlay);
  const auto currentDocument = storage.findDocument("worktree-id", "src/current.cpp");
  const auto previousDocument = storage.findDocument("worktree-id", "src/previous.cpp");

  CHECK(initialSummary.indexed == 1);
  REQUIRE(initialDocument.has_value());
  CHECK(overlaySummary.indexed == 1);
  CHECK(overlaySummary.removed == 1);
  REQUIRE(currentDocument.has_value());
  CHECK(currentDocument->status == uburu::GitFileStatus::modified);
  CHECK_FALSE(currentDocument->deleted);
  REQUIRE(previousDocument.has_value());
  CHECK(previousDocument->status == uburu::GitFileStatus::deleted);
  CHECK(previousDocument->deleted);
  CHECK(previousDocument->contentHash == initialDocument->contentHash);
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("persistent index search hides deleted paths and returns modified replacements")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-persistent-index-search-overlay-test");
  const auto root = directory.path() / "repo";

  writeFile(root / "src" / "previous.cpp", "old content");

  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");
  storage.initialize();
  storage.upsertRepository(repositoryInfo(root));
  storage.upsertWorktree(worktreeInfo(root));

  uburu::index::PersistentIndexService indexService(storage);
  const std::vector initialFiles{
    fileEntry(root, "src/previous.cpp"),
  };

  const auto initialSummary = indexService.update(worktreeInfo(root), initialFiles);

  writeFile(root / "src" / "current.cpp", "new content");
  std::filesystem::remove(root / "src" / "previous.cpp");

  const std::vector scannedFiles{
    fileEntry(root, "src/current.cpp"),
  };
  const std::vector overlay{
    uburu::GitOverlayEntry{.relativePath = "src/current.cpp",
                           .previousRelativePath = std::filesystem::path("src/previous.cpp"),
                           .status = uburu::GitFileStatus::modified,
                           .disposition = uburu::GitOverlayDisposition::replaceWithWorkingTree,
                           .reusableBlob = {}},
  };

  const auto overlaySummary = indexService.update(worktreeInfo(root), scannedFiles, overlay);

  uburu::SearchQuery query{.root = root, .scope = {}, .expression = "cpp", .options = {}};
  query.options.target = uburu::SearchTarget::fileName;

  const auto results = indexService.search(query);

  CHECK(initialSummary.indexed == 1);
  CHECK(overlaySummary.indexed == 1);
  CHECK(overlaySummary.removed == 1);
  REQUIRE(results.size() == 1);
  CHECK(results.front().kind == uburu::SearchResultKind::fileName);
  CHECK(results.front().path == std::filesystem::path("src/current.cpp"));
  CHECK(results.front().lineText == "src/current.cpp");
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("persistent index search returns indexed content matches")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-persistent-index-search-content-test");
  const auto root = directory.path() / "repo";

  writeFile(root / "src" / "main.cpp", "before\nneedle here\nafter\n");

  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");
  storage.initialize();
  storage.upsertRepository(repositoryInfo(root));
  storage.upsertWorktree(worktreeInfo(root));

  uburu::index::PersistentIndexService indexService(storage);
  const std::vector files{
    fileEntry(root, "src/main.cpp"),
  };

  const auto summary = indexService.update(worktreeInfo(root), files);

  uburu::SearchQuery query{.root = root, .scope = {}, .expression = "needle", .options = {}};
  query.options.target = uburu::SearchTarget::content;

  const auto results = indexService.search(query);

  CHECK(summary.indexed == 1);
  REQUIRE(results.size() == 1);
  CHECK(results.front().kind == uburu::SearchResultKind::content);
  CHECK(results.front().path == std::filesystem::path("src/main.cpp"));
  CHECK(results.front().line == 2);
  CHECK(results.front().lineText == "needle here");
  CHECK(results.front().column == 1);
  CHECK(results.front().matchLength == 6);
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("persistent index service reports missing fresh and stale generations")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-persistent-index-staleness-test");
  const auto root = directory.path() / "repo";

  writeFile(root / "src" / "main.cpp", "content\n");

  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");
  storage.initialize();
  storage.upsertRepository(repositoryInfo(root));
  storage.upsertWorktree(worktreeInfo(root));

  uburu::index::PersistentIndexService indexService(storage);
  auto currentWorktree = worktreeInfo(root);

  const auto missing = indexService.staleness(currentWorktree);

  const std::vector files{
    fileEntry(root, "src/main.cpp"),
  };
  const auto updateSummary = indexService.update(currentWorktree, files);
  const auto fresh = indexService.staleness(currentWorktree);

  auto changedHead = currentWorktree;
  changedHead.headOid = "def456";

  auto changedBranch = currentWorktree;
  changedBranch.branch = "feature";

  const auto staleHead = indexService.staleness(changedHead);
  const auto staleBranch = indexService.staleness(changedBranch);

  CHECK(missing.state == uburu::index::IndexStalenessState::missing);
  CHECK(updateSummary.indexed == 1);
  CHECK(fresh.state == uburu::index::IndexStalenessState::fresh);
  REQUIRE(fresh.latestGeneration.has_value());
  CHECK(fresh.latestGeneration->headOid == currentWorktree.headOid);
  CHECK(staleHead.state == uburu::index::IndexStalenessState::stale);
  CHECK(staleHead.headChanged);
  CHECK_FALSE(staleHead.branchChanged);
  CHECK(staleBranch.state == uburu::index::IndexStalenessState::stale);
  CHECK_FALSE(staleBranch.headChanged);
  CHECK(staleBranch.branchChanged);
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("persistent index service reuses git blob documents before reading files")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-persistent-index-blob-reuse-test");
  const auto root = directory.path() / "repo";
  const auto missingFile = root / "src" / "renamed.txt";

  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");
  storage.initialize();
  storage.upsertRepository(repositoryInfo(root));
  storage.upsertWorktree(worktreeInfo(root));
  storage.publishGeneration(indexGeneration({
    indexedDocument("content-from-blob", "tracked-blob", "src/original.txt"),
  }));

  uburu::index::PersistentIndexService indexService(storage);
  const uburu::FileEntry missingEntry{.absolutePath = missingFile,
                                      .relativePath = "src/renamed.txt",
                                      .size = 42,
                                      .modifiedAt = std::filesystem::file_time_type{std::chrono::seconds{99}},
                                      .searchRoot = root};
  const std::vector candidates{
    uburu::index::IndexFileCandidate{
      .file = missingEntry,
      .metadata =
        uburu::index::IndexFileMetadata{
          .status = uburu::GitFileStatus::clean,
          .gitBlob = uburu::GitObjectId{.algorithm = uburu::GitObjectHashAlgorithm::sha1, .value = "tracked-blob"},
        },
    },
  };

  const auto summary = indexService.update(worktreeInfo(root), candidates);
  const auto reused = storage.findDocument("worktree-id", "src/renamed.txt");

  CHECK_FALSE(summary.cancelled);
  CHECK(summary.indexed == 0);
  CHECK(summary.reusedByBlob == 1);
  CHECK(summary.failed == 0);
  REQUIRE(reused.has_value());
  CHECK(reused->contentHash == "content-from-blob");
  CHECK(reused->gitBlobHash == "tracked-blob");
  CHECK(reused->relativePath == std::filesystem::path("src/renamed.txt"));
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("persistent index service observes cancellation before publishing")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-persistent-index-cancel-test");
  const auto root = directory.path() / "repo";
  std::stop_source stopSource;

  writeFile(root / "src" / "a.txt", "content");

  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");
  storage.initialize();
  storage.upsertRepository(repositoryInfo(root));
  storage.upsertWorktree(worktreeInfo(root));

  uburu::index::PersistentIndexService indexService(storage);
  const std::vector files{
    fileEntry(root, "src/a.txt"),
  };

  stopSource.request_stop();

  const auto summary = indexService.update(worktreeInfo(root), files, {}, stopSource.get_token());
  const auto document = storage.findDocument("worktree-id", "src/a.txt");

  CHECK(summary.cancelled);
  CHECK(summary.indexed == 0);
  CHECK(summary.reusedByHash == 0);
  CHECK_FALSE(document.has_value());
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("persistent index service reports unreadable files without aborting the generation")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-persistent-index-failure-test");
  const auto root = directory.path() / "repo";

  writeFile(root / "src" / "valid.txt", "valid");

  auto missing = fileEntry(root, "src/valid.txt");
  missing.absolutePath = root / "src" / "missing.txt";
  missing.relativePath = "src/missing.txt";

  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");
  storage.initialize();
  storage.upsertRepository(repositoryInfo(root));
  storage.upsertWorktree(worktreeInfo(root));

  uburu::index::PersistentIndexService indexService(storage);
  const std::vector files{
    fileEntry(root, "src/valid.txt"),
    missing,
  };

  const auto summary = indexService.update(worktreeInfo(root), files);
  const auto valid = storage.findDocument("worktree-id", "src/valid.txt");
  const auto invalid = storage.findDocument("worktree-id", "src/missing.txt");

  CHECK_FALSE(summary.cancelled);
  CHECK(summary.indexed == 1);
  CHECK(summary.failed == 1);
  REQUIRE(valid.has_value());
  CHECK_FALSE(invalid.has_value());
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("persistent index service classifies unsupported document formats without reporting failures")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-persistent-index-unsupported-format-test");
  const auto root = directory.path() / "repo";

  writeFile(root / "src" / "valid.txt", "valid");
  writeFile(root / "src" / "document.docx", "packaged content placeholder");

  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");
  storage.initialize();
  storage.upsertRepository(repositoryInfo(root));
  storage.upsertWorktree(worktreeInfo(root));

  uburu::index::PersistentIndexService indexService(storage);
  const std::vector files{
    fileEntry(root, "src/valid.txt"),
    fileEntry(root, "src/document.docx"),
  };

  const auto summary = indexService.update(worktreeInfo(root), files);
  const auto valid = storage.findDocument("worktree-id", "src/valid.txt");
  const auto unsupported = storage.findDocument("worktree-id", "src/document.docx");

  CHECK_FALSE(summary.cancelled);
  CHECK(summary.indexed == 1);
  CHECK(summary.failed == 0);
  CHECK(summary.skippedUnsupportedFormat == 1);
  CHECK(summary.skippedBinary == 0);
  CHECK(summary.skippedTemporaryLimitation == 0);
  REQUIRE(valid.has_value());
  CHECK_FALSE(unsupported.has_value());
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("persistent index service classifies binary files without reporting failures")
{
#if defined(UBURU_HAS_SQLITE)
  TemporaryDirectory directory("uburu-persistent-index-binary-skip-test");
  const auto root = directory.path() / "repo";

  writeFile(root / "src" / "valid.txt", "valid");
  writeFile(root / "src" / "data.bin", "binary placeholder");

  auto binary = fileEntry(root, "src/data.bin");
  binary.binary = true;

  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");
  storage.initialize();
  storage.upsertRepository(repositoryInfo(root));
  storage.upsertWorktree(worktreeInfo(root));

  uburu::index::PersistentIndexService indexService(storage);
  const std::vector files{
    fileEntry(root, "src/valid.txt"),
    binary,
  };

  const auto summary = indexService.update(worktreeInfo(root), files);
  const auto valid = storage.findDocument("worktree-id", "src/valid.txt");
  const auto skipped = storage.findDocument("worktree-id", "src/data.bin");

  CHECK_FALSE(summary.cancelled);
  CHECK(summary.indexed == 1);
  CHECK(summary.failed == 0);
  CHECK(summary.skippedUnsupportedFormat == 0);
  CHECK(summary.skippedBinary == 1);
  CHECK(summary.skippedTemporaryLimitation == 0);
  REQUIRE(valid.has_value());
  CHECK_FALSE(skipped.has_value());
#else
  SUCCEED("SQLite is not available in this build");
#endif
}
