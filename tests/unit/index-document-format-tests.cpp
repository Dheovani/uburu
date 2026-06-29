#include "core/index/index-document-format.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

namespace
{

  [[nodiscard]] uburu::IndexDocument validDocument()
  {
    return uburu::IndexDocument{.repositoryId = "repository-id",
                                .worktreeId = "worktree-id",
                                .relativePath = "src/main.cpp",
                                .contentHash = "sha256-content",
                                .contentHashAlgorithm = uburu::ContentHashAlgorithm::sha256,
                                .gitBlobHash = "blob",
                                .gitBlobHashAlgorithm = uburu::GitObjectHashAlgorithm::sha1,
                                .status = uburu::GitFileStatus::clean,
                                .size = 42,
                                .indexedAt = std::chrono::system_clock::now(),
                                .deleted = false};
  }

} // namespace

TEST_CASE("index document format exposes the current descriptor")
{
  const auto descriptor = uburu::index::currentIndexDocumentFormat();

  CHECK(descriptor.version == uburu::latestIndexDocumentFormatVersion);
  CHECK(descriptor.contentHashAlgorithm == uburu::ContentHashAlgorithm::sha256);
  CHECK(descriptor.contentAddressed);
  CHECK(descriptor.storesGitBlobHash);
  CHECK(descriptor.storesWorkingTreeOverlay);
}

TEST_CASE("index document format accepts only supported versions")
{
  CHECK(uburu::index::isSupportedIndexDocumentFormatVersion(uburu::initialIndexDocumentFormatVersion));
  CHECK(uburu::index::isSupportedIndexDocumentFormatVersion(uburu::latestIndexDocumentFormatVersion));
  CHECK_FALSE(uburu::index::isSupportedIndexDocumentFormatVersion(0));
  CHECK_FALSE(uburu::index::isSupportedIndexDocumentFormatVersion(uburu::latestIndexDocumentFormatVersion + 1U));
}

TEST_CASE("index document format validates persisted document metadata")
{
  CHECK_FALSE(uburu::index::validateIndexDocumentFormat(validDocument()).has_value());

  auto unsupportedVersion = validDocument();
  unsupportedVersion.formatVersion = uburu::latestIndexDocumentFormatVersion + 1U;

  CHECK(uburu::index::validateIndexDocumentFormat(unsupportedVersion) == "unsupported index document format version");

  auto unsupportedHashAlgorithm = validDocument();
  unsupportedHashAlgorithm.contentHashAlgorithm = uburu::ContentHashAlgorithm::unknown;

  CHECK(uburu::index::validateIndexDocumentFormat(unsupportedHashAlgorithm) ==
        "unsupported index content hash algorithm");

  auto emptyHash = validDocument();
  emptyHash.contentHash.clear();

  CHECK(uburu::index::validateIndexDocumentFormat(emptyHash) == "index document content hash is empty");
}
