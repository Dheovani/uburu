#pragma once

#include "shared/types/domain-types.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <vector>

namespace uburu::index
{

  struct IndexExtractorMetrics
  {
    std::string extractorName;
    std::size_t filesProcessed{0};
    std::uintmax_t bytesProcessed{0};
    std::chrono::nanoseconds extractionTime{};
    std::size_t skippedUnsupportedFormat{0};
    std::size_t skippedBinary{0};
    std::size_t skippedSafetyLimited{0};
    std::size_t skippedProtected{0};
    std::size_t parserFailures{0};
    std::uintmax_t indexedTextBytes{0};
  };

  struct IndexUpdateProgress
  {
    std::size_t processed{0};
    std::size_t total{0};
    std::size_t indexed{0};
    std::size_t reusedByCatalog{0};
    std::size_t reusedByBlob{0};
    std::size_t reusedByHash{0};
    std::size_t removed{0};
    std::size_t failed{0};
    std::size_t skippedUnsupportedFormat{0};
    std::size_t skippedBinary{0};
    std::size_t skippedBySize{0};
    std::size_t skippedByFilter{0};
    std::size_t skippedTemporaryLimitation{0};
    std::vector<IndexExtractorMetrics> extractorMetrics;
    std::filesystem::path currentPath;
  };

  struct IndexUpdateSummary
  {
    std::size_t indexed{0};
    std::size_t reusedByCatalog{0};
    std::size_t reusedByBlob{0};
    std::size_t reusedByHash{0};
    std::size_t removed{0};
    std::size_t failed{0};
    std::size_t skippedUnsupportedFormat{0};
    std::size_t skippedBinary{0};
    std::size_t skippedBySize{0};
    std::size_t skippedByFilter{0};
    std::size_t skippedTemporaryLimitation{0};
    std::vector<IndexExtractorMetrics> extractorMetrics;
    bool cancelled{false};
  };

  struct IndexFileMetadata
  {
    GitFileStatus status{GitFileStatus::clean};
    std::optional<GitObjectId> gitBlob;
  };

  struct IndexFileCandidate
  {
    FileEntry file;
    IndexFileMetadata metadata;
  };

  enum class IndexStalenessState
  {
    missing,
    fresh,
    stale
  };

  struct IndexStalenessReport
  {
    IndexStalenessState state{IndexStalenessState::missing};
    bool headChanged{false};
    bool branchChanged{false};
    std::optional<IndexGenerationMetadata> latestGeneration;
  };

  using IndexProgressCallback = std::function<void(const IndexUpdateProgress&)>;

  class IndexService
  {
  public:
    virtual ~IndexService() = default;
    [[nodiscard]] virtual IndexUpdateSummary update(const WorktreeInfo& worktree,
                                                    std::span<const FileEntry> files,
                                                    const IndexProgressCallback& onProgress = {},
                                                    std::stop_token stopToken = {}) = 0;
    [[nodiscard]] virtual IndexUpdateSummary update(const WorktreeInfo& worktree,
                                                    std::span<const IndexFileCandidate> files,
                                                    const IndexProgressCallback& onProgress = {},
                                                    std::stop_token stopToken = {}) = 0;
    [[nodiscard]] virtual IndexUpdateSummary update(const WorktreeInfo& worktree,
                                                    std::span<const FileEntry> files,
                                                    std::span<const GitOverlayEntry> overlay,
                                                    const IndexProgressCallback& onProgress = {},
                                                    std::stop_token stopToken = {}) = 0;
    [[nodiscard]] virtual IndexStalenessReport staleness(const WorktreeInfo& worktree) const = 0;
    [[nodiscard]] virtual std::vector<SearchResult> search(const SearchQuery& query,
                                                           std::stop_token stopToken = {}) const = 0;
  };

} // namespace uburu::index
