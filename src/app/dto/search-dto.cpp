#include "app/dto/search-dto.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace uburu::app
{
  namespace
  {

    [[nodiscard]] std::string searchErrorCodeName(search::SearchErrorCode code)
    {
      switch (code) {
      case search::SearchErrorCode::emptyRoot:
        return "emptyRoot";
      case search::SearchErrorCode::rootNotFound:
        return "rootNotFound";
      case search::SearchErrorCode::rootNotDirectory:
        return "rootNotDirectory";
      case search::SearchErrorCode::emptyExpression:
        return "emptyExpression";
      case search::SearchErrorCode::unsupportedSearchMode:
        return "unsupportedSearchMode";
      case search::SearchErrorCode::regexCompileFailed:
        return "regexCompileFailed";
      case search::SearchErrorCode::regexResourceLimitExceeded:
        return "regexResourceLimitExceeded";
      case search::SearchErrorCode::regexTimeout:
        return "regexTimeout";
      case search::SearchErrorCode::invalidRegexLimit:
        return "invalidRegexLimit";
      case search::SearchErrorCode::invalidResultLimit:
        return "invalidResultLimit";
      case search::SearchErrorCode::invalidPerFileResultLimit:
        return "invalidPerFileResultLimit";
      case search::SearchErrorCode::invalidMaximumFileSize:
        return "invalidMaximumFileSize";
      case search::SearchErrorCode::fileOpenFailed:
        return "fileOpenFailed";
      case search::SearchErrorCode::fileReadFailed:
        return "fileReadFailed";
      }

      return "unknown";
    }

    [[nodiscard]] std::string regexExecutionModeName(search::RegexExecutionMode mode)
    {
      switch (mode) {
      case search::RegexExecutionMode::notUsed:
        return "notUsed";
      case search::RegexExecutionMode::jit:
        return "jit";
      case search::RegexExecutionMode::interpretedFallback:
        return "interpretedFallback";
      }

      return "unknown";
    }

  } // namespace

  SearchResultKindDto toSearchResultKindDto(SearchResultKind kind)
  {
    switch (kind) {
    case SearchResultKind::content:
      return SearchResultKindDto::content;
    case SearchResultKind::fileName:
      return SearchResultKindDto::fileName;
    }

    return SearchResultKindDto::content;
  }

  SearchResultKind toCoreSearchResultKind(SearchResultKindDto kind)
  {
    switch (kind) {
    case SearchResultKindDto::content:
      return SearchResultKind::content;
    case SearchResultKindDto::fileName:
      return SearchResultKind::fileName;
    }

    return SearchResultKind::content;
  }

  SearchMatchDto toSearchMatchDto(const MatchSpan& match)
  {
    SearchMatchDto dto;

    dto.column = match.column;
    dto.byteOffset = match.byteOffset;
    dto.byteLength = match.byteLength;

    return dto;
  }

  SearchResultDto toSearchResultDto(const SearchResult& result)
  {
    std::vector<SearchMatchDto> highlights;
    highlights.reserve(result.highlights.size());
    std::ranges::transform(result.highlights, std::back_inserter(highlights), toSearchMatchDto);

    SearchResultDto dto;

    dto.kind = toSearchResultKindDto(result.kind);
    dto.path = result.path;
    dto.line = result.line;
    dto.column = result.column;
    dto.matchLength = result.matchLength;
    dto.lineText = result.lineText;
    dto.highlights = std::move(highlights);
    dto.contextBefore = result.contextBefore;
    dto.contextAfter = result.contextAfter;
    dto.searchRoot = result.searchRoot;

    return dto;
  }

  SearchMetricsDto toSearchMetricsDto(const diagnostics::SearchMetrics& metrics)
  {
    SearchMetricsDto dto;

    dto.timeToFirstResult = metrics.timeToFirstResult;
    dto.totalTime = metrics.totalTime;
    dto.filesProcessed = metrics.filesProcessed;
    dto.bytesProcessed = metrics.bytesProcessed;
    dto.filesPerSecond = metrics.filesPerSecond;
    dto.bytesPerSecond = metrics.bytesPerSecond;
    dto.resultsEmitted = metrics.resultsEmitted;
    dto.ignoredFiles = metrics.ignoredFiles;
    dto.hiddenFiles = metrics.hiddenFiles;
    dto.binaryFiles = metrics.binaryFiles;
    dto.binaryFilesSkipped = metrics.binaryFilesSkipped;
    dto.queueProducerWaits = metrics.queueProducerWaits;
    dto.queueConsumerWaits = metrics.queueConsumerWaits;
    dto.cacheHits = metrics.cacheHits;
    dto.cacheMisses = metrics.cacheMisses;
    dto.reusedByCatalog = metrics.reusedByCatalog;
    dto.reusedByBlob = metrics.reusedByBlob;
    dto.reusedByHash = metrics.reusedByHash;
    dto.approximateMemoryBytes = metrics.approximateMemoryBytes;
    dto.memoryGrowthBytes = metrics.memoryGrowthBytes;
    dto.memoryIncreased = metrics.memoryIncreased;

    return dto;
  }

  SearchErrorDto toSearchErrorDto(const search::SearchError& error)
  {
    SearchErrorDto dto;

    dto.code = searchErrorCodeName(error.code);
    dto.translationKey = error.translationKey;
    dto.context = error.context;
    dto.hasOffset = error.offset.has_value();
    dto.offset = error.offset.value_or(0);

    return dto;
  }

  SearchSummaryDto toSearchSummaryDto(const search::SearchSummary& summary)
  {
    std::vector<SearchErrorDto> errors;
    errors.reserve(summary.errors.size());
    std::ranges::transform(summary.errors, std::back_inserter(errors), toSearchErrorDto);

    SearchSummaryDto dto;

    dto.filesScanned = summary.filesScanned;
    dto.matches = summary.matches;
    dto.filesWithMatchLimitReached = summary.filesWithMatchLimitReached;
    dto.filesWithReadErrors = summary.filesWithReadErrors;
    dto.cancelled = summary.cancelled;
    dto.limitReached = summary.limitReached;
    dto.partialFailure = summary.partialFailure;
    dto.errors = std::move(errors);
    dto.regexExecutionMode = regexExecutionModeName(summary.regexExecutionMode);
    dto.metrics = toSearchMetricsDto(summary.metrics);

    return dto;
  }

} // namespace uburu::app
