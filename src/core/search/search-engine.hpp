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

  class SearchEngine
  {
  public:
    virtual ~SearchEngine() = default;
    [[nodiscard]] virtual SearchSummary
    search(const SearchQuery& query, ResultSink sink, std::stop_token stop_token = {}) const = 0;
  };

} // namespace uburu::search
