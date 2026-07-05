#include "core/index/index-overlay.hpp"
#include "helpers/temporary-paths.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <vector>

namespace
{

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

  [[nodiscard]] uburu::FileEntry fileEntry(const std::filesystem::path& root, const std::filesystem::path& relativePath)
  {
    return uburu::FileEntry{.absolutePath = root / relativePath,
                            .relativePath = relativePath,
                            .size = 42,
                            .modifiedAt = std::filesystem::file_time_type{std::chrono::seconds{4}},
                            .searchRoot = root};
  }

} // namespace

TEST_CASE("index overlay marks scanned modified files as working tree candidates")
{
  const auto root = uburu::tests::uniqueTemporaryPath("uburu-index-overlay-modified-test");
  const auto worktree = worktreeInfo(root);
  const std::vector scannedFiles{
    fileEntry(root, "src/clean.cpp"),
    fileEntry(root, "src/modified.cpp"),
  };
  const std::vector overlay{
    uburu::GitOverlayEntry{.relativePath = "src/modified.cpp",
                           .previousRelativePath = {},
                           .status = uburu::GitFileStatus::modified,
                           .disposition = uburu::GitOverlayDisposition::replaceWithWorkingTree,
                           .reusableBlob = {}},
  };

  const auto plan = uburu::index::buildOverlayIndexCandidates(worktree, scannedFiles, overlay);

  REQUIRE(plan.candidates.size() == 2);
  CHECK(plan.hiddenIndexedPaths == 0);
  CHECK(plan.missingWorkingTreeFiles == 0);
  CHECK(plan.candidates[0].metadata.status == uburu::GitFileStatus::clean);
  CHECK(plan.candidates[1].metadata.status == uburu::GitFileStatus::modified);
}

TEST_CASE("index overlay creates tombstones for deleted indexed paths")
{
  const auto root = uburu::tests::uniqueTemporaryPath("uburu-index-overlay-deleted-test");
  const auto worktree = worktreeInfo(root);
  const std::vector<uburu::FileEntry> scannedFiles;
  const std::vector overlay{
    uburu::GitOverlayEntry{.relativePath = "src/deleted.cpp",
                           .previousRelativePath = {},
                           .status = uburu::GitFileStatus::deleted,
                           .disposition = uburu::GitOverlayDisposition::hideIndexedContent,
                           .reusableBlob = {}},
  };

  const auto plan = uburu::index::buildOverlayIndexCandidates(worktree, scannedFiles, overlay);

  REQUIRE(plan.candidates.size() == 1);
  CHECK(plan.hiddenIndexedPaths == 1);
  CHECK(plan.missingWorkingTreeFiles == 0);
  CHECK(plan.candidates.front().file.absolutePath == root / "src/deleted.cpp");
  CHECK(plan.candidates.front().file.relativePath == std::filesystem::path("src/deleted.cpp"));
  CHECK(plan.candidates.front().metadata.status == uburu::GitFileStatus::deleted);
}

TEST_CASE("index overlay models renames as reusable current path plus hidden previous path")
{
  const auto root = uburu::tests::uniqueTemporaryPath("uburu-index-overlay-rename-test");
  const auto worktree = worktreeInfo(root);
  const std::vector scannedFiles{
    fileEntry(root, "src/current.cpp"),
  };
  const std::vector overlay{
    uburu::GitOverlayEntry{.relativePath = "src/current.cpp",
                           .previousRelativePath = std::filesystem::path("src/previous.cpp"),
                           .status = uburu::GitFileStatus::modified,
                           .disposition = uburu::GitOverlayDisposition::replaceWithWorkingTree,
                           .reusableBlob =
                             uburu::GitObjectId{.algorithm = uburu::GitObjectHashAlgorithm::sha1, .value = "blob-id"}},
  };

  const auto plan = uburu::index::buildOverlayIndexCandidates(worktree, scannedFiles, overlay);

  REQUIRE(plan.candidates.size() == 2);
  CHECK(plan.hiddenIndexedPaths == 1);
  CHECK(plan.missingWorkingTreeFiles == 0);
  CHECK(plan.candidates[0].file.relativePath == std::filesystem::path("src/current.cpp"));
  CHECK(plan.candidates[0].metadata.status == uburu::GitFileStatus::modified);
  REQUIRE(plan.candidates[0].metadata.gitBlob.has_value());
  CHECK(plan.candidates[0].metadata.gitBlob->value == "blob-id");
  CHECK(plan.candidates[1].file.relativePath == std::filesystem::path("src/previous.cpp"));
  CHECK(plan.candidates[1].metadata.status == uburu::GitFileStatus::deleted);
}

TEST_CASE("index overlay reports missing working tree files without fabricating content candidates")
{
  const auto root = uburu::tests::uniqueTemporaryPath("uburu-index-overlay-missing-test");
  const auto worktree = worktreeInfo(root);
  const std::vector<uburu::FileEntry> scannedFiles;
  const std::vector overlay{
    uburu::GitOverlayEntry{.relativePath = "src/missing.cpp",
                           .previousRelativePath = {},
                           .status = uburu::GitFileStatus::modified,
                           .disposition = uburu::GitOverlayDisposition::replaceWithWorkingTree,
                           .reusableBlob = {}},
  };

  const auto plan = uburu::index::buildOverlayIndexCandidates(worktree, scannedFiles, overlay);

  CHECK(plan.candidates.empty());
  CHECK(plan.hiddenIndexedPaths == 0);
  CHECK(plan.missingWorkingTreeFiles == 1);
}
