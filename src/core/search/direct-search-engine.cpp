#include "core/search/direct-search-engine.hpp"

#include "core/search/search-errors.hpp"
#include "core/search/search-query-validation.hpp"
#include "core/search/search-scope.hpp"
#include "core/text/regex-matcher.hpp"
#include "core/text/text-file-reader.hpp"
#include "core/text/text-matcher.hpp"

#include <deque>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace uburu::search
{
  namespace
  {

    enum class PublishDecision
    {
      continueSearch,
      stopCurrentFile,
      stopSearch
    };

    struct PendingResult
    {
      SearchResult result;
      std::size_t remainingContextLines{0};
    };

    SearchErrorCode errorCodeFromRegexStatus(text::RegexMatchStatus status)
    {
      if (status == text::RegexMatchStatus::timedOut)
        return SearchErrorCode::regexTimeout;

      return SearchErrorCode::regexResourceLimitExceeded;
    }

    bool searchesContent(SearchTarget target)
    {
      return target == SearchTarget::content || target == SearchTarget::contentAndFileName;
    }

    bool searchesFileName(SearchTarget target)
    {
      return target == SearchTarget::fileName || target == SearchTarget::contentAndFileName;
    }

    std::vector<text::MatchPosition> findLiteralMatches(std::string_view text, const SearchQuery& query)
    {
      return text::findAllLiterals(text, query.expression, query.options);
    }

    std::optional<std::vector<text::MatchPosition>>
    findRegexMatches(std::string_view text, const text::RegexMatcher& regexMatcher, SearchSummary& summary)
    {
      const auto regexResult = regexMatcher.findAll(text);

      if (regexResult.status != text::RegexMatchStatus::completed) {
        summary.errors.push_back(
          makeSearchError(errorCodeFromRegexStatus(regexResult.status), std::to_string(regexResult.backendErrorCode)));

        return std::nullopt;
      }

      return std::move(regexResult.matches);
    }

    std::optional<std::vector<text::MatchPosition>> findMatches(std::string_view text, const SearchQuery& query,
                                                                const std::optional<text::RegexMatcher>& regexMatcher,
                                                                SearchSummary& summary)
    {
      if (regexMatcher)
        return findRegexMatches(text, *regexMatcher, summary);

      return findLiteralMatches(text, query);
    }

    void reportPartialFailure(SearchSummary& summary, SearchErrorCode code, const FileEntry& entry)
    {
      ++summary.filesWithReadErrors;
      summary.partialFailure = true;
      summary.errors.push_back(makeSearchError(code, entry.relativePath.generic_string()));
    }

    bool reportTextReadSummary(SearchSummary& summary, const FileEntry& entry, text::TextReadSummary readSummary)
    {
      if (readSummary.status == text::TextReadStatus::completed)
        return true;

      if (readSummary.status == text::TextReadStatus::binarySkipped) {
        ++summary.metrics.binaryFiles;
        ++summary.metrics.binaryFilesSkipped;

        return true;
      }

      if (readSummary.status == text::TextReadStatus::cancelled)
        return false;

      const auto code = readSummary.status == text::TextReadStatus::openFailed ? SearchErrorCode::fileOpenFailed
                                                                               : SearchErrorCode::fileReadFailed;
      reportPartialFailure(summary, code, entry);

      return true;
    }

    std::vector<MatchSpan> makeHighlights(std::string_view lineText, const std::vector<text::MatchPosition>& matches)
    {
      std::vector<MatchSpan> highlights;
      highlights.reserve(matches.size());
      for (const auto& highlightMatch : matches) {
        highlights.push_back(MatchSpan{.column = text::visualColumnForByteOffset(lineText, highlightMatch.offset),
                                       .byteOffset = highlightMatch.offset,
                                       .byteLength = highlightMatch.length});
      }

      return highlights;
    }

    std::vector<std::string> copyContext(const std::deque<std::string>& context)
    {
      return {context.begin(), context.end()};
    }

    bool publishReadyPending(std::vector<PendingResult>& pending, const ResultSink& sink)
    {
      for (auto iterator = pending.begin(); iterator != pending.end();) {
        if (iterator->remainingContextLines > 0) {
          ++iterator;

          continue;
        }

        if (!sink(std::move(iterator->result)))
          return false;

        iterator = pending.erase(iterator);
      }

      return true;
    }

    bool addContextAfter(std::vector<PendingResult>& pending, std::string_view lineText, const ResultSink& sink)
    {
      for (auto& pendingResult : pending) {
        if (pendingResult.remainingContextLines == 0)
          continue;

        pendingResult.result.contextAfter.push_back(std::string{lineText});
        --pendingResult.remainingContextLines;
      }

      return publishReadyPending(pending, sink);
    }

    bool flushPending(std::vector<PendingResult>& pending, const ResultSink& sink)
    {
      for (auto& pendingResult : pending)
        pendingResult.remainingContextLines = 0;

      return publishReadyPending(pending, sink);
    }

    PublishDecision publishMatches(const FileEntry& entry, SearchResultKind kind, std::size_t lineNumber,
                                   std::string_view lineText, const std::vector<text::MatchPosition>& matches,
                                   const SearchQuery& query, SearchSummary& summary, std::size_t& fileMatches,
                                   const std::deque<std::string>& contextBefore, std::vector<PendingResult>& pending,
                                   const ResultSink& sink)
    {
      for (const auto& match : matches) {
        if (summary.matches >= query.options.resultLimit) {
          summary.limitReached = true;

          return PublishDecision::stopSearch;
        }

        if (fileMatches >= query.options.perFileResultLimit) {
          ++summary.filesWithMatchLimitReached;

          return PublishDecision::stopCurrentFile;
        }

        ++summary.matches;
        ++fileMatches;

        SearchResult result{.kind = kind,
                            .path = entry.relativePath,
                            .line = lineNumber,
                            .column = text::visualColumnForByteOffset(lineText, match.offset),
                            .matchLength = match.length,
                            .lineText = std::string{lineText},
                            .highlights = makeHighlights(lineText, matches),
                            .contextBefore = copyContext(contextBefore),
                            .contextAfter = {},
                            .searchRoot = entry.searchRoot};

        if (query.options.contextAfterLines == 0) {
          if (!sink(std::move(result)))
            return PublishDecision::stopSearch;

          continue;
        }

        pending.push_back(
          PendingResult{.result = std::move(result), .remainingContextLines = query.options.contextAfterLines});
      }

      return PublishDecision::continueSearch;
    }

  } // namespace

  DirectSearchEngine::DirectSearchEngine(std::shared_ptr<const filesystem::FileScanner> scanner)
      : scanner(std::move(scanner))
  {
    if (!this->scanner)
      throw std::invalid_argument("DirectSearchEngine requires a file scanner");
  }

  SearchSummary DirectSearchEngine::search(const SearchQuery& query, ResultSink sink, std::stop_token stop_token) const
  {
    SearchSummary summary;
    summary.errors = validateSearchQuery(query);
    if (!summary.errors.empty())
      return summary;

    std::optional<text::RegexMatcher> regexMatcher;
    if (query.options.mode == SearchMode::regex) {
      auto compiled = text::compileRegex(query.expression, query.options);
      if (!compiled.matcher) {
        const auto& error = compiled.error;
        summary.errors.push_back(makeSearchError(SearchErrorCode::regexCompileFailed,
                                                 error ? error->message : std::string{},
                                                 error ? error->offset : std::optional<std::size_t>{}));

        return summary;
      }
      summary.regexExecutionMode =
        compiled.matcher->jitEnabled() ? RegexExecutionMode::jit : RegexExecutionMode::interpretedFallback;
      regexMatcher = std::move(compiled.matcher);
    }

    const auto roots = effectiveSearchRoots(query);

    for (const auto& root : roots) {
      if (stop_token.stop_requested() || summary.limitReached)
        break;

      auto rootOptions = optionsForRoot(query.options, root);

      scanner->scan(
        root.path, rootOptions,
        [&](FileEntry entry) {
          if (stop_token.stop_requested())
            return false;

          ++summary.filesScanned;
          ++summary.metrics.filesProcessed;
          summary.metrics.bytesProcessed += entry.size;

          std::size_t fileMatches = 0;
          std::deque<std::string> previousContext;
          std::vector<PendingResult> pendingResults;

          if (searchesFileName(query.options.target)) {
            const auto pathText = entry.relativePath.generic_string();
            const auto pathMatches = findMatches(pathText, query, regexMatcher, summary);

            if (!pathMatches)
              return false;

            const auto decision = publishMatches(entry, SearchResultKind::fileName, 0, pathText, *pathMatches, query,
                                                 summary, fileMatches, previousContext, pendingResults, sink);

            if (!flushPending(pendingResults, sink))
              return false;

            if (decision == PublishDecision::stopSearch)
              return false;

            if (decision == PublishDecision::stopCurrentFile)
              return true;
          }

          if (!searchesContent(query.options.target))
            return true;

          bool stopCurrentFile = false;
          bool stopSearch = false;
          const auto readSummary = text::readTextFileLines(
            entry.absolutePath, query.options,
            [&](const text::TextLine& line) {
              if (!addContextAfter(pendingResults, line.text, sink)) {
                stopSearch = true;

                return false;
              }

              const auto matches = findMatches(line.text, query, regexMatcher, summary);

              if (!matches)
                return false;

              if (matches->empty()) {
                previousContext.push_back(line.text);
                while (previousContext.size() > query.options.contextBeforeLines)
                  previousContext.pop_front();

                return true;
              }

              const auto decision =
                publishMatches(entry, SearchResultKind::content, line.lineNumber, line.text, *matches, query, summary,
                               fileMatches, previousContext, pendingResults, sink);

              previousContext.push_back(line.text);
              while (previousContext.size() > query.options.contextBeforeLines)
                previousContext.pop_front();

              if (decision == PublishDecision::stopSearch) {
                stopSearch = true;

                return false;
              }

              if (decision == PublishDecision::stopCurrentFile) {
                stopCurrentFile = true;

                return false;
              }

              return true;
            },
            stop_token);

          if (!flushPending(pendingResults, sink))
            return false;

          if (stopSearch)
            return false;

          if (stopCurrentFile)
            return true;

          return reportTextReadSummary(summary, entry, readSummary);
        },
        stop_token, &summary.metrics);
    }

    summary.cancelled = stop_token.stop_requested();
    summary.metrics.resultsEmitted = summary.matches;

    return summary;
  }

} // namespace uburu::search
