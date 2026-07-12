#include "core/filesystem/file-watcher-backend.hpp"
#include "core/index/index-backend.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <span>
#include <variant>
#include <vector>

namespace
{

  class FakeIndexService final : public uburu::index::IndexService
  {
  public:
    [[nodiscard]]
    uburu::index::IndexUpdateSummary update(
      const uburu::WorktreeInfo&,
      std::span<const uburu::FileEntry>,
      const uburu::index::IndexProgressCallback& = {},
      std::stop_token = {}) override
    {
      return {};
    }

    [[nodiscard]]
    uburu::index::IndexUpdateSummary update(
      const uburu::WorktreeInfo&,
      std::span<const uburu::index::IndexFileCandidate>,
      const uburu::index::IndexProgressCallback& = {},
      std::stop_token = {}) override
    {
      return {};
    }

    [[nodiscard]]
    uburu::index::IndexUpdateSummary update(
      const uburu::WorktreeInfo&,
      std::span<const uburu::FileEntry>,
      std::span<const uburu::GitOverlayEntry>,
      const uburu::index::IndexProgressCallback& = {},
      std::stop_token = {}) override
    {
      return {};
    }

    [[nodiscard]]
    uburu::index::IndexStalenessReport staleness(const uburu::WorktreeInfo&) const override
    {
      return {};
    }

    [[nodiscard]]
    std::vector<uburu::SearchResult> search(const uburu::SearchQuery&, std::stop_token = {}) const override
    {
      return {};
    }
  };

  class FakeIndexBackendFactory final : public uburu::index::IndexBackendFactory
  {
  public:
    [[nodiscard]]
    uburu::index::IndexBackendCapabilities capabilities() const override
    {
      uburu::index::IndexBackendCapabilities capabilities;
      capabilities.name = "fake-index";
      capabilities.version = "1";
      capabilities.persistent = true;
      capabilities.contentAddressedDocuments = true;
      capabilities.textSearch = true;

      return capabilities;
    }

    [[nodiscard]]
    uburu::index::IndexBackendOpenResult open(const uburu::index::IndexBackendOpenRequest&) const override
    {
      uburu::index::IndexBackendHandle handle;
      handle.indexService = std::make_shared<FakeIndexService>();
      handle.capabilities = capabilities();

      return handle;
    }
  };

  class FakeFileWatcher final : public uburu::filesystem::FileWatcher
  {
  public:
    [[nodiscard]]
    uburu::filesystem::FileChangeBatch poll(std::stop_token = {}) override
    {
      uburu::filesystem::FileChangeBatch batch;
      batch.requiresRescan = true;

      return batch;
    }
  };

  class FakeFileWatcherFactory final : public uburu::filesystem::FileWatcherFactory
  {
  public:
    [[nodiscard]]
    uburu::filesystem::FileWatcherBackendCapabilities capabilities() const override
    {
      uburu::filesystem::FileWatcherBackendCapabilities capabilities;
      capabilities.name = "fake-watcher";
      capabilities.kind = uburu::filesystem::FileWatcherBackendKind::polling;
      capabilities.recursive = true;
      capabilities.overflowDetection = true;

      return capabilities;
    }

    [[nodiscard]]
    uburu::filesystem::FileWatcherOpenResult open(const uburu::filesystem::FileWatcherOpenRequest&) const override
    {
      return std::make_unique<FakeFileWatcher>();
    }
  };

} // namespace

TEST_CASE("index backend factory exposes capabilities and an index service")
{
  FakeIndexBackendFactory factory;

  const auto capabilities = factory.capabilities();

  CHECK(capabilities.name == "fake-index");
  CHECK(capabilities.contract.name == "uburu.index.backend");
  CHECK(capabilities.persistent);
  CHECK(capabilities.contentAddressedDocuments);

  uburu::index::IndexBackendOpenRequest request;
  request.storagePath = "C:/tmp/index.db";

  auto result = factory.open(request);

  REQUIRE(std::holds_alternative<uburu::index::IndexBackendHandle>(result));

  auto& handle = std::get<uburu::index::IndexBackendHandle>(result);

  REQUIRE(handle.indexService != nullptr);
  CHECK(handle.capabilities.name == "fake-index");
}

TEST_CASE("file watcher factory returns a watcher through a typed backend result")
{
  FakeFileWatcherFactory factory;

  const auto capabilities = factory.capabilities();

  CHECK(capabilities.name == "fake-watcher");
  CHECK(capabilities.contract.name == "uburu.filesystem.file-watcher.backend");
  CHECK(capabilities.recursive);
  CHECK(capabilities.overflowDetection);

  uburu::filesystem::FileWatcherOpenRequest request;
  request.root = "C:/tmp/repo";

  auto result = factory.open(request);

  REQUIRE(std::holds_alternative<std::unique_ptr<uburu::filesystem::FileWatcher>>(result));

  auto watcher = std::move(std::get<std::unique_ptr<uburu::filesystem::FileWatcher>>(result));

  REQUIRE(watcher != nullptr);

  const auto batch = watcher->poll();

  CHECK(batch.requiresRescan);
}
