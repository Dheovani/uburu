#pragma once

#include "core/diagnostics/metrics.hpp"
#include "shared/types/domain-types.hpp"

#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace uburu::search
{

  using ResultSink = std::function<bool(SearchResult)>;

  enum class SearchErrorCode
  {
    emptyRoot,
    rootNotFound,
    rootNotDirectory,
    rootUnavailable,
    emptyExpression,
    unsupportedSearchMode,
    regexCompileFailed,
    regexResourceLimitExceeded,
    regexTimeout,
    invalidRegexLimit,
    invalidResultLimit,
    invalidPerFileResultLimit,
    invalidMaximumFileSize,
    fileOpenFailed,
    fileReadFailed
  };

  /**
   * Describes one typed search error with optional backend-specific context.
   */
  struct SearchError
  {
    SearchErrorCode code{SearchErrorCode::emptyExpression};
    std::string translationKey;
    std::string context;
    std::optional<std::size_t> offset;
  };

  enum class RegexExecutionMode
  {
    notUsed,
    jit,
    interpretedFallback
  };

  /**
   * Summarizes a search run for UI status, diagnostics, and tests.
   */
  struct SearchSummary
  {
    std::size_t filesScanned{0};
    std::size_t matches{0};
    std::size_t filesWithMatchLimitReached{0};
    std::size_t filesWithReadErrors{0};
    bool cancelled{false};
    bool limitReached{false};
    bool partialFailure{false};
    std::vector<SearchError> errors;
    RegexExecutionMode regexExecutionMode{RegexExecutionMode::notUsed};
    diagnostics::SearchMetrics metrics;
  };

  /**
   * Streams search results without coupling callers to a concrete search backend.
   */
  class SearchEngine
  {
  public:
    virtual ~SearchEngine() = default;

    [[nodiscard]]
    virtual SearchSummary search(
      const SearchQuery& query,
      ResultSink sink,
      std::stop_token stopToken = {}) const = 0;
  };

} // namespace uburu::search
