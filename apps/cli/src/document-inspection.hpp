#pragma once

#include "core/document/document-extractor.hpp"
#include "shared/types/domain-types.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <stop_token>

namespace uburu::cli
{

  struct DocumentInspectionSummary
  {
    std::size_t filesScanned{0};
    std::size_t supportedFiles{0};
    std::uintmax_t sourceBytes{0};
    std::map<document::DocumentExtractionStatus, std::size_t> statuses;
    std::map<document::DocumentExtractionIssue, std::size_t> issues;
    bool cancelled{false};
  };

  /**
   * Inspects extraction outcomes without retaining document text or private paths.
   */
  [[nodiscard]]
  DocumentInspectionSummary inspectDocuments(
    const std::filesystem::path& root,
    const SearchOptions& options,
    std::stop_token stopToken = {});

} // namespace uburu::cli
