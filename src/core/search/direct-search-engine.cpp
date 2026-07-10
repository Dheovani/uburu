#include "core/search/direct-search-engine.hpp"

#include "core/document/docx-document-extractor.hpp"
#include "core/document/html-document-extractor.hpp"
#include "core/document/rtf-document-extractor.hpp"
#include "core/document/subtitle-document-extractor.hpp"
#include "core/search/search-errors.hpp"
#include "core/search/search-query-validation.hpp"
#include "core/search/search-scope.hpp"
#include "core/text/regex-matcher.hpp"
#include "core/text/text-file-reader.hpp"
#include "core/text/text-matcher.hpp"

#include <cstdlib>
#include <deque>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
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

    std::string pathToUtf8(const std::filesystem::path& path)
    {
      const auto text = path.generic_u8string();

      return {reinterpret_cast<const char*>(text.data()), text.size()};
    }

    std::optional<std::string> environmentVariable(const char* name)
    {
#ifdef _WIN32
      char* value = nullptr;
      std::size_t size = 0;

      if (_dupenv_s(&value, &size, name) != 0 || value == nullptr)
        return std::nullopt;

      std::string text(value);
      std::free(value);

      return text;
#else
      const auto* value = std::getenv(name);

      if (value == nullptr)
        return std::nullopt;

      return std::string(value);
#endif
    }

    bool searchDebugEnabled()
    {
      const auto value = environmentVariable("UBURU_SEARCH_DEBUG");

      return value.has_value() && *value != "0";
    }

    std::filesystem::path searchDebugLogPath()
    {
      const auto value = environmentVariable("UBURU_SEARCH_DEBUG_FILE");

      if (!value || value->empty())
        return "uburu-search-debug.log";

      return *value;
    }

    void appendSearchDebugLog(std::string_view source, std::string_view message)
    {
      if (!searchDebugEnabled())
        return;

      static std::mutex mutex;
      std::lock_guard lock(mutex);
      std::ofstream file(searchDebugLogPath(), std::ios::app);

      if (!file)
        return;

      file << "[search][" << source << "] " << message << '\n';
    }

    std::string searchTargetName(SearchTarget target)
    {
      switch (target) {
      case SearchTarget::content:
        return "content";
      case SearchTarget::fileName:
        return "fileName";
      case SearchTarget::contentAndFileName:
        return "contentAndFileName";
      }

      return "unknown";
    }

    std::string searchModeName(SearchMode mode)
    {
      switch (mode) {
      case SearchMode::literal:
        return "literal";
      case SearchMode::regex:
        return "regex";
      }

      return "unknown";
    }

    std::string textReadStatusName(text::TextReadStatus status)
    {
      switch (status) {
      case text::TextReadStatus::completed:
        return "completed";
      case text::TextReadStatus::openFailed:
        return "openFailed";
      case text::TextReadStatus::readFailed:
        return "readFailed";
      case text::TextReadStatus::binarySkipped:
        return "binarySkipped";
      case text::TextReadStatus::invalidEncoding:
        return "invalidEncoding";
      case text::TextReadStatus::lineTooLong:
        return "lineTooLong";
      case text::TextReadStatus::cancelled:
        return "cancelled";
      }

      return "unknown";
    }

    std::string joinExtensions(const std::vector<std::string>& extensions)
    {
      std::string text;

      for (const auto& extension : extensions) {
        if (!text.empty())
          text += ",";

        text += extension;
      }

      return text.empty() ? "<none>" : text;
    }

    void logSearchStart(const SearchQuery& query)
    {
      std::ostringstream message;
      message << "start root=" << pathToUtf8(query.root) << " expression=\"" << query.expression << "\""
              << " mode=" << searchModeName(query.options.mode) << " target=" << searchTargetName(query.options.target)
              << " extensions=" << joinExtensions(query.options.extensions)
              << " includeSubdirectories=" << query.options.includeSubdirectories
              << " respectGitignore=" << query.options.respectGitignore
              << " includeHidden=" << query.options.includeHidden
              << " maximumFileSize=" << query.options.maximumFileSize;

      appendSearchDebugLog("engine", message.str());
    }

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

    [[nodiscard]]
    const document::DocumentExtractor* structuredDocumentExtractor(const std::filesystem::path& path)
    {
      static const document::DocxDocumentExtractor docxExtractor;
      static const document::HtmlDocumentExtractor htmlExtractor;
      static const document::RtfDocumentExtractor rtfExtractor;
      static const document::SubtitleDocumentExtractor subtitleExtractor;

      if (docxExtractor.supports(path))
        return &docxExtractor;

      if (htmlExtractor.supports(path))
        return &htmlExtractor;

      if (rtfExtractor.supports(path))
        return &rtfExtractor;

      if (subtitleExtractor.supports(path))
        return &subtitleExtractor;

      return nullptr;
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

    std::optional<std::vector<text::MatchPosition>> findMatches(std::string_view text,
                                                                const SearchQuery& query,
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
      summary.errors.push_back(makeSearchError(code, pathToUtf8(entry.relativePath)));
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

    bool reportDocumentExtractionSummary(SearchSummary& summary,
                                         const FileEntry& entry,
                                         document::DocumentExtractionSummary extractionSummary)
    {
      if (extractionSummary.status == document::DocumentExtractionStatus::completed)
        return true;

      if (extractionSummary.status == document::DocumentExtractionStatus::binarySkipped) {
        ++summary.metrics.binaryFiles;
        ++summary.metrics.binaryFilesSkipped;

        return true;
      }

      if (extractionSummary.status == document::DocumentExtractionStatus::cancelled)
        return false;

      const auto code = extractionSummary.status == document::DocumentExtractionStatus::openFailed
                          ? SearchErrorCode::fileOpenFailed
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

    PublishDecision publishMatches(const FileEntry& entry,
                                   SearchResultKind kind,
                                   std::size_t lineNumber,
                                   std::string_view lineText,
                                   const std::vector<text::MatchPosition>& matches,
                                   const SearchQuery& query,
                                   SearchSummary& summary,
                                   std::size_t& fileMatches,
                                   const std::deque<std::string>& contextBefore,
                                   std::vector<PendingResult>& pending,
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
    logSearchStart(query);

    SearchSummary summary;
    summary.errors = validateSearchQuery(query);
    if (!summary.errors.empty()) {
      appendSearchDebugLog("engine", "validation failed errors=" + std::to_string(summary.errors.size()));

      return summary;
    }

    std::optional<text::RegexMatcher> regexMatcher;
    if (query.options.mode == SearchMode::regex) {
      auto compiled = text::compileRegex(query.expression, query.options);
      if (!compiled.matcher) {
        const auto& error = compiled.error;
        summary.errors.push_back(makeSearchError(SearchErrorCode::regexCompileFailed,
                                                 error ? error->message : std::string{},
                                                 error ? error->offset : std::optional<std::size_t>{}));

        appendSearchDebugLog("engine", "regex compile failed");

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
      appendSearchDebugLog("engine", "scan-root path=" + pathToUtf8(root.path));

      scanner->scan(
        root.path,
        rootOptions,
        [&](FileEntry entry) {
          if (stop_token.stop_requested())
            return false;

          ++summary.filesScanned;
          ++summary.metrics.filesProcessed;
          summary.metrics.bytesProcessed += entry.size;
          appendSearchDebugLog(
            "engine", "read-candidate path=" + pathToUtf8(entry.relativePath) + " size=" + std::to_string(entry.size));

          std::size_t fileMatches = 0;
          std::deque<std::string> previousContext;
          std::vector<PendingResult> pendingResults;

          if (searchesFileName(query.options.target)) {
            const auto pathText = pathToUtf8(entry.relativePath);
            const auto pathMatches = findMatches(pathText, query, regexMatcher, summary);

            if (!pathMatches)
              return false;

            appendSearchDebugLog(
              "engine", "file-name-check path=" + pathText + " matches=" + std::to_string(pathMatches->size()));

            const auto decision = publishMatches(entry,
                                                 SearchResultKind::fileName,
                                                 0,
                                                 pathText,
                                                 *pathMatches,
                                                 query,
                                                 summary,
                                                 fileMatches,
                                                 previousContext,
                                                 pendingResults,
                                                 sink);

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
          auto processLine = [&](std::string_view lineText, std::size_t lineNumber) {
            if (!addContextAfter(pendingResults, lineText, sink)) {
              stopSearch = true;

              return false;
            }

            const auto matches = findMatches(lineText, query, regexMatcher, summary);

            if (!matches)
              return false;

            if (matches->empty()) {
              previousContext.push_back(std::string{lineText});
              while (previousContext.size() > query.options.contextBeforeLines)
                previousContext.pop_front();

              return true;
            }

            appendSearchDebugLog("engine",
                                 "content-match path=" + pathToUtf8(entry.relativePath) + " line=" +
                                   std::to_string(lineNumber) + " matches=" + std::to_string(matches->size()));

            const auto decision = publishMatches(entry,
                                                 SearchResultKind::content,
                                                 lineNumber,
                                                 lineText,
                                                 *matches,
                                                 query,
                                                 summary,
                                                 fileMatches,
                                                 previousContext,
                                                 pendingResults,
                                                 sink);

            previousContext.push_back(std::string{lineText});
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
          };

          const auto* structuredExtractor = structuredDocumentExtractor(entry.absolutePath);
          if (structuredExtractor != nullptr) {
            document::DocumentExtractionOptions extractionOptions;

            extractionOptions.textOptions = query.options;

            const auto extractionSummary = structuredExtractor->extract(
              entry.absolutePath,
              extractionOptions,
              [&](const document::ExtractedTextSegment& segment) {
                std::size_t lineNumber = 1;
                std::size_t lineStart = 0;

                while (lineStart <= segment.text.size()) {
                  const auto lineEnd = segment.text.find('\n', lineStart);
                  const auto lineSize =
                    lineEnd == std::string::npos ? segment.text.size() - lineStart : lineEnd - lineStart;
                  const auto lineText = std::string_view{segment.text}.substr(lineStart, lineSize);

                  if (!processLine(lineText, lineNumber)) {
                    if (!stopSearch && !stopCurrentFile)
                      stopSearch = true;

                    return false;
                  }

                  if (lineEnd == std::string::npos)
                    break;

                  lineStart = lineEnd + 1;
                  ++lineNumber;
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

            return reportDocumentExtractionSummary(summary, entry, extractionSummary);
          }

          const auto readSummary = text::readTextFileLines(
            entry.absolutePath,
            query.options,
            [&](const text::TextLine& line) {
              if (!processLine(line.text, line.lineNumber)) {
                if (stopSearch || stopCurrentFile)
                  return false;

                stopSearch = true;

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

          appendSearchDebugLog("engine",
                               "read-summary path=" + pathToUtf8(entry.relativePath) +
                                 " status=" + textReadStatusName(readSummary.status) +
                                 " lines=" + std::to_string(readSummary.linesRead));

          return reportTextReadSummary(summary, entry, readSummary);
        },
        stop_token,
        &summary.metrics);
    }

    summary.cancelled = stop_token.stop_requested();
    summary.metrics.resultsEmitted = summary.matches;
    appendSearchDebugLog("engine",
                         "finish filesScanned=" + std::to_string(summary.filesScanned) + " matches=" +
                           std::to_string(summary.matches) + " errors=" + std::to_string(summary.errors.size()));

    return summary;
  }

} // namespace uburu::search
