#include "core/index/persistent-index-service.hpp"

#include "core/document/document-extractor.hpp"
#include "core/document/docx-document-extractor.hpp"
#include "core/document/html-document-extractor.hpp"
#include "core/document/open-document-extractor.hpp"
#include "core/document/pdf-document-extractor.hpp"
#include "core/document/plain-text-extractor.hpp"
#include "core/document/pptx-document-extractor.hpp"
#include "core/document/rtf-document-extractor.hpp"
#include "core/document/subtitle-document-extractor.hpp"
#include "core/document/xlsx-document-extractor.hpp"
#include "core/index/content-hash.hpp"
#include "core/index/index-overlay.hpp"
#include "core/index/index-working-memory.hpp"
#include "core/search/search-result-memory.hpp"
#include "core/text/regex-matcher.hpp"
#include "core/text/text-file-reader.hpp"
#include "core/text/text-matcher.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <deque>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace uburu::index
{
  namespace
  {

    constexpr std::uint64_t approximateHashSetNodeOverheadBytes = sizeof(std::string) + 3U * sizeof(void*);

    [[nodiscard]]
    std::uint64_t saturatedAdd(std::uint64_t left, std::uint64_t right)
    {
      if (right > std::numeric_limits<std::uint64_t>::max() - left)
        return std::numeric_limits<std::uint64_t>::max();

      return left + right;
    }

    [[nodiscard]]
    std::uint64_t reservedDocumentStorageBytes(std::size_t documentCount)
    {
      if (documentCount > std::numeric_limits<std::uint64_t>::max() / sizeof(IndexDocument))
        return std::numeric_limits<std::uint64_t>::max();

      return static_cast<std::uint64_t>(documentCount) * sizeof(IndexDocument);
    }

    [[nodiscard]]
    IndexUpdateOptions withRetainedMemory(IndexUpdateOptions options, std::uint64_t retainedMemoryBytes)
    {
      options.retainedMemoryBytes = saturatedAdd(options.retainedMemoryBytes, retainedMemoryBytes);

      return options;
    }

    [[nodiscard]]
    std::string reusableDocumentKey(ContentHashAlgorithm algorithm, std::string_view hash)
    {
      return std::to_string(static_cast<int>(algorithm)) + ":" + std::string{hash};
    }

    [[nodiscard]]
    bool shouldIndexFile(const FileEntry& file)
    {
      return !file.binary;
    }

    enum class IndexSkipReason
    {
      none,
      unsupportedFormat,
      binary,
      temporaryLimitation
    };

    struct IndexedTextReadResult
    {
      std::optional<std::string> text;
      std::string extractorName;
      document::DocumentExtractionSummary extractionSummary;
      std::chrono::nanoseconds extractionTime{};
      std::uintmax_t indexedTextBytes{0};
      IndexSkipReason skipReason{IndexSkipReason::none};
      bool cancelled{false};
      bool memoryLimitReached{false};
      bool failed{false};
    };

    [[nodiscard]]
    const document::DocumentExtractorRegistry& defaultDocumentExtractorRegistry()
    {
      static const auto registry = [] {
        document::DocumentExtractorRegistry configuredRegistry;

        configuredRegistry.add(std::make_shared<document::DocxDocumentExtractor>());
        configuredRegistry.add(std::make_shared<document::HtmlDocumentExtractor>());
        configuredRegistry.add(std::make_shared<document::OpenDocumentExtractor>());
        configuredRegistry.add(std::make_shared<document::PdfDocumentExtractor>());
        configuredRegistry.add(std::make_shared<document::PptxDocumentExtractor>());
        configuredRegistry.add(std::make_shared<document::RtfDocumentExtractor>());
        configuredRegistry.add(std::make_shared<document::SubtitleDocumentExtractor>());
        configuredRegistry.add(std::make_shared<document::XlsxDocumentExtractor>());
        configuredRegistry.add(std::make_shared<document::PlainTextExtractor>());

        return configuredRegistry;
      }();

      return registry;
    }

    [[nodiscard]]
    std::string lowerAscii(std::string value)
    {
      for (auto& character : value) {
        if (character >= 'A' && character <= 'Z')
          character = static_cast<char>(character - 'A' + 'a');
      }

      return value;
    }

    [[nodiscard]]
    bool hasUnsupportedDocumentExtension(const std::filesystem::path& path)
    {
      constexpr std::array unsupportedExtensions{".doc", ".epub", ".zip"};
      const auto extension = lowerAscii(path.extension().string());

      for (const auto unsupportedExtension : unsupportedExtensions) {
        if (extension == unsupportedExtension)
          return true;
      }

      return false;
    }

    void recordSkip(IndexSkipReason reason, IndexUpdateSummary& summary, IndexUpdateProgress& progress)
    {
      switch (reason) {
      case IndexSkipReason::unsupportedFormat:
        ++summary.skippedUnsupportedFormat;
        ++progress.skippedUnsupportedFormat;
        return;
      case IndexSkipReason::binary:
        ++summary.skippedBinary;
        ++progress.skippedBinary;
        return;
      case IndexSkipReason::temporaryLimitation:
        ++summary.skippedTemporaryLimitation;
        ++progress.skippedTemporaryLimitation;
        return;
      case IndexSkipReason::none:
        return;
      }
    }

    [[nodiscard]]
    IndexExtractorMetrics& extractorMetrics(std::vector<IndexExtractorMetrics>& metrics, std::string_view extractorName)
    {
      for (auto& item : metrics) {
        if (item.extractorName == extractorName)
          return item;
      }

      auto& item = metrics.emplace_back();

      item.extractorName = std::string{extractorName};

      return item;
    }

    void recordExtractorMetrics(IndexUpdateSummary& summary,
                                IndexUpdateProgress& progress,
                                std::string_view extractorName,
                                std::uintmax_t fileBytes,
                                const document::DocumentExtractionSummary& extractionSummary,
                                std::chrono::nanoseconds extractionTime,
                                std::uintmax_t indexedTextBytes)
    {
      auto record = [&](std::vector<IndexExtractorMetrics>& metrics) {
        auto& item = extractorMetrics(metrics, extractorName);
        const auto availability = document::documentContentAvailability(extractionSummary.status);

        ++item.filesProcessed;
        item.bytesProcessed += fileBytes;
        item.extractionTime += extractionTime;
        item.indexedTextBytes += indexedTextBytes;

        if (availability == document::DocumentContentAvailability::nameOnlyUnsupported)
          ++item.skippedUnsupportedFormat;

        if (availability == document::DocumentContentAvailability::nameOnlyBinary)
          ++item.skippedBinary;

        if (availability == document::DocumentContentAvailability::nameOnlySafetyLimited)
          ++item.skippedSafetyLimited;

        if (availability == document::DocumentContentAvailability::nameOnlyProtected)
          ++item.skippedProtected;

        if (availability == document::DocumentContentAvailability::extractionFailed)
          ++item.parserFailures;
      };

      record(summary.extractorMetrics);
      record(progress.extractorMetrics);
    }

    [[nodiscard]]
    bool isDeletedOverlay(const IndexFileMetadata& metadata)
    {
      return metadata.status == GitFileStatus::deleted;
    }

    [[nodiscard]]
    bool canReuseCatalogDocument(const IndexDocument& document, const IndexFileCandidate& candidate)
    {
      const auto& file = candidate.file;

      return !document.deleted && !document.contentHash.empty() && document.size == file.size &&
             document.modifiedAt == file.modifiedAt && document.status == candidate.metadata.status &&
             document.status == GitFileStatus::clean && document.contentHashAlgorithm != ContentHashAlgorithm::unknown;
    }

    [[nodiscard]]
    bool canReuseBlobDocument(const IndexFileMetadata& metadata)
    {
      return metadata.status == GitFileStatus::clean && metadata.gitBlob.has_value() &&
             metadata.gitBlob->algorithm != GitObjectHashAlgorithm::unknown && !metadata.gitBlob->value.empty();
    }

    [[nodiscard]]
    std::optional<std::string> gitBlobHash(const IndexFileMetadata& metadata)
    {
      if (!metadata.gitBlob)
        return std::nullopt;

      return metadata.gitBlob->value;
    }

    [[nodiscard]]
    GitObjectHashAlgorithm gitBlobHashAlgorithm(const IndexFileMetadata& metadata)
    {
      if (!metadata.gitBlob)
        return GitObjectHashAlgorithm::unknown;

      return metadata.gitBlob->algorithm;
    }

    [[nodiscard]]
    IndexDocument makeIndexDocument(const WorktreeInfo& worktree,
                                    const FileEntry& file,
                                    const IndexFileMetadata& metadata,
                                    const ContentHash& contentHash,
                                    std::optional<std::string> indexedText)
    {
      return IndexDocument{.formatVersion = latestIndexDocumentFormatVersion,
                           .repositoryId = worktree.repositoryId,
                           .worktreeId = worktree.id,
                           .relativePath = file.relativePath,
                           .contentHash = contentHash.value,
                           .contentHashAlgorithm = contentHash.algorithm,
                           .gitBlobHash = gitBlobHash(metadata),
                           .gitBlobHashAlgorithm = gitBlobHashAlgorithm(metadata),
                           .status = metadata.status,
                           .size = file.size,
                           .modifiedAt = file.modifiedAt,
                           .indexedAt = std::chrono::system_clock::now(),
                           .deleted = false,
                           .indexedText = std::move(indexedText)};
    }

    [[nodiscard]]
    IndexDocument makeDeletedIndexDocument(const WorktreeInfo& worktree,
                                           const FileEntry& file,
                                           const IndexFileMetadata& metadata,
                                           const IndexDocument& previousDocument)
    {
      return IndexDocument{.formatVersion = latestIndexDocumentFormatVersion,
                           .repositoryId = worktree.repositoryId,
                           .worktreeId = worktree.id,
                           .relativePath = file.relativePath,
                           .contentHash = previousDocument.contentHash,
                           .contentHashAlgorithm = previousDocument.contentHashAlgorithm,
                           .gitBlobHash = metadata.gitBlob ? std::optional<std::string>{metadata.gitBlob->value}
                                                           : previousDocument.gitBlobHash,
                           .gitBlobHashAlgorithm =
                             metadata.gitBlob ? metadata.gitBlob->algorithm : previousDocument.gitBlobHashAlgorithm,
                           .status = GitFileStatus::deleted,
                           .size = previousDocument.size,
                           .modifiedAt = previousDocument.modifiedAt,
                           .indexedAt = std::chrono::system_clock::now(),
                           .deleted = true,
                           .indexedText = std::nullopt};
    }

    [[nodiscard]]
    IndexDocument makeReusedIndexDocument(const WorktreeInfo& worktree,
                                          const FileEntry& file,
                                          const IndexFileMetadata& metadata,
                                          const IndexedDocumentIdentity& reusableDocument)
    {
      return IndexDocument{.formatVersion = latestIndexDocumentFormatVersion,
                           .repositoryId = worktree.repositoryId,
                           .worktreeId = worktree.id,
                           .relativePath = file.relativePath,
                           .contentHash = reusableDocument.contentHash,
                           .contentHashAlgorithm = reusableDocument.contentHashAlgorithm,
                           .gitBlobHash = metadata.gitBlob ? std::optional<std::string>{metadata.gitBlob->value}
                                                           : reusableDocument.gitBlobHash,
                           .gitBlobHashAlgorithm =
                             metadata.gitBlob ? metadata.gitBlob->algorithm : reusableDocument.gitBlobHashAlgorithm,
                           .status = metadata.status,
                           .size = file.size,
                           .modifiedAt = file.modifiedAt,
                           .indexedAt = std::chrono::system_clock::now(),
                           .deleted = false,
                           .indexedText = std::nullopt};
    }

    [[nodiscard]]
    IndexedDocumentIdentity reusableIdentity(const IndexDocument& document)
    {
      return IndexedDocumentIdentity{.formatVersion = document.formatVersion,
                                     .contentHash = document.contentHash,
                                     .contentHashAlgorithm = document.contentHashAlgorithm,
                                     .gitBlobHash = document.gitBlobHash,
                                     .gitBlobHashAlgorithm = document.gitBlobHashAlgorithm,
                                     .size = document.size,
                                     .indexedAt = document.indexedAt};
    }

    [[nodiscard]]
    IndexFileCandidate defaultCandidate(const FileEntry& file)
    {
      return IndexFileCandidate{.file = file, .metadata = IndexFileMetadata{}};
    }

    void publishProgress(const IndexProgressCallback& onProgress, const IndexUpdateProgress& progress)
    {
      if (!onProgress)
        return;

      onProgress(progress);
    }

    [[nodiscard]]
    bool searchesFileName(const SearchQuery& query)
    {
      return query.options.target == SearchTarget::fileName || query.options.target == SearchTarget::contentAndFileName;
    }

    [[nodiscard]]
    bool searchesContent(const SearchQuery& query)
    {
      return query.options.target == SearchTarget::content || query.options.target == SearchTarget::contentAndFileName;
    }

    [[nodiscard]]
    IndexedTextReadResult readIndexedText(
      const FileEntry& file,
      const SearchOptions& options,
      std::uintmax_t maximumTextBytes,
      std::stop_token stopToken)
    {
      std::string indexedText;
      bool firstSegment = true;
      bool memoryLimitReached = false;
      const auto* extractor = defaultDocumentExtractorRegistry().findExtractor(file.absolutePath);
      const auto extractorName =
        extractor == nullptr ? std::string{"unsupported-format"} : std::string{extractor->name()};
      const auto extractionStart = std::chrono::steady_clock::now();

      document::DocumentExtractionOptions extractionOptions;

      extractionOptions.textOptions = options;

      auto extractionSummary = document::DocumentExtractionSummary{};

      if (extractor == nullptr) {
        extractionSummary.status = document::DocumentExtractionStatus::unsupportedFormat;
      } else {
        extractionSummary = extractor->extract(
          file.absolutePath,
          extractionOptions,
          [&](const document::ExtractedTextSegment& segment) {
            const auto separatorBytes = firstSegment ? 0U : 1U;
            const auto segmentBytes = static_cast<std::uintmax_t>(segment.text.size());
            const auto additionalBytes = segmentBytes + separatorBytes;
            const auto indexedTextBytes = static_cast<std::uintmax_t>(indexedText.size());

            if (maximumTextBytes > 0 &&
                (indexedTextBytes > maximumTextBytes || additionalBytes > maximumTextBytes - indexedTextBytes)) {
              memoryLimitReached = true;

              return false;
            }

            if (!firstSegment)
              indexedText.push_back('\n');

            indexedText += segment.text;
            firstSegment = false;

            return true;
          },
          stopToken);
      }

      IndexedTextReadResult result;
      result.extractorName = extractorName;
      result.extractionSummary = extractionSummary;
      result.extractionTime = std::chrono::steady_clock::now() - extractionStart;
      result.indexedTextBytes = static_cast<std::uintmax_t>(indexedText.size());
      result.memoryLimitReached = memoryLimitReached;

      if (memoryLimitReached)
        return result;

      const auto availability = document::documentContentAvailability(extractionSummary.status);

      if (availability == document::DocumentContentAvailability::contentAvailable) {
        result.text = std::move(indexedText);

        return result;
      }

      if (availability == document::DocumentContentAvailability::cancelled) {
        result.cancelled = true;

        return result;
      }

      if (availability == document::DocumentContentAvailability::nameOnlyBinary) {
        result.skipReason = IndexSkipReason::binary;

        return result;
      }

      if (availability == document::DocumentContentAvailability::nameOnlyUnsupported) {
        result.skipReason = IndexSkipReason::unsupportedFormat;

        return result;
      }

      if (document::isNameOnlySearchable(availability)) {
        result.skipReason = IndexSkipReason::temporaryLimitation;

        return result;
      }

      result.failed = true;

      return result;
    }

    [[nodiscard]]
    std::vector<text::MatchPosition> indexedPathMatches(std::string_view pathText,
                                                        const SearchQuery& query,
                                                        const std::optional<text::RegexMatcher>& regex)
    {
      if (regex)
        return regex->findAll(pathText).matches;

      return text::findAllLiterals(pathText, query.expression, query.options);
    }

    [[nodiscard]]
    std::optional<std::vector<text::MatchPosition>>
    indexedTextMatches(std::string_view text, const SearchQuery& query, const std::optional<text::RegexMatcher>& regex)
    {
      if (regex) {
        const auto matchResult = regex->findAll(text);

        if (matchResult.status != text::RegexMatchStatus::completed)
          return std::nullopt;

        return matchResult.matches;
      }

      return text::findAllLiterals(text, query.expression, query.options);
    }

    [[nodiscard]]
    std::vector<MatchSpan> makeHighlights(std::string_view lineText, const std::vector<text::MatchPosition>& matches)
    {
      std::vector<MatchSpan> highlights;
      highlights.reserve(matches.size());

      for (const auto& match : matches) {
        highlights.push_back(MatchSpan{.column = text::visualColumnForByteOffset(lineText, match.offset),
                                       .byteOffset = match.offset,
                                       .byteLength = match.length});
      }

      return highlights;
    }

    [[nodiscard]]
    SearchResult makeIndexedResult(const IndexDocument& document,
                                   SearchResultKind kind,
                                   std::size_t line,
                                   std::string_view lineText,
                                   const text::MatchPosition& match,
                                   const std::vector<text::MatchPosition>& matches,
                                   const SearchQuery& query,
                                   const std::deque<std::string>& contextBefore)
    {
      return SearchResult{.kind = kind,
                          .path = document.relativePath,
                          .line = line,
                          .column = text::visualColumnForByteOffset(lineText, match.offset),
                          .matchLength = match.length,
                          .lineText = std::string{lineText},
                          .highlights = makeHighlights(lineText, matches),
                          .contextBefore = {contextBefore.begin(), contextBefore.end()},
                          .contextAfter = {},
                          .searchRoot = query.root};
    }

    [[nodiscard]]
    bool appendIndexedResults(const IndexDocument& document,
                              SearchResultKind kind,
                              std::size_t line,
                              std::string_view lineText,
                              const std::vector<text::MatchPosition>& matches,
                              const SearchQuery& query,
                              std::size_t& fileMatches,
                              const std::deque<std::string>& contextBefore,
                              IndexSearchResult& searchResult)
    {
      for (const auto& match : matches) {
        if (searchResult.results.size() >= query.options.resultLimit) {
          searchResult.resultLimitReached = true;

          return false;
        }

        if (fileMatches >= query.options.perFileResultLimit)
          return true;

        auto result = makeIndexedResult(document, kind, line, lineText, match, matches, query, contextBefore);
        const auto resultMemoryBytes = search::approximateSearchResultMemoryBytes(result);

        if (!search::searchResultFitsMemoryBudget(
              searchResult.resultMemoryBytes, resultMemoryBytes, query.options.resultMemoryBudgetBytes)) {
          searchResult.memoryLimitReached = true;

          return false;
        }

        ++fileMatches;
        searchResult.resultMemoryBytes += resultMemoryBytes;
        searchResult.results.push_back(std::move(result));
      }

      return true;
    }

    void rememberContextLine(std::deque<std::string>& context, std::string_view lineText, const SearchQuery& query)
    {
      context.push_back(std::string{lineText});

      while (context.size() > query.options.contextBeforeLines) {
        context.pop_front();
      }
    }

    bool appendIndexedContentResults(const IndexDocument& document,
                                     std::string_view indexedText,
                                     const SearchQuery& query,
                                     const std::optional<text::RegexMatcher>& regex,
                                     std::size_t& fileMatches,
                                     IndexSearchResult& searchResult)
    {
      std::deque<std::string> contextBefore;
      std::size_t lineNumber = 0;
      std::size_t lineStart = 0;

      while (lineStart < indexedText.size()) {
        const auto lineEnd = indexedText.find('\n', lineStart);
        const auto lineSize = lineEnd == std::string_view::npos ? indexedText.size() - lineStart : lineEnd - lineStart;
        const auto lineText = indexedText.substr(lineStart, lineSize);
        ++lineNumber;

        const auto matches = indexedTextMatches(lineText, query, regex);

        if (!matches)
          return false;

        if (!matches->empty() && !appendIndexedResults(document,
                                                       SearchResultKind::content,
                                                       lineNumber,
                                                       lineText,
                                                       *matches,
                                                       query,
                                                       fileMatches,
                                                       contextBefore,
                                                       searchResult))
          return false;

        rememberContextLine(contextBefore, lineText, query);

        if (lineEnd == std::string_view::npos)
          break;

        lineStart = lineEnd + 1;
      }

      return true;
    }

    [[nodiscard]]
    std::optional<text::RegexMatcher> compileIndexedRegex(const SearchQuery& query)
    {
      if (query.options.mode != SearchMode::regex)
        return std::nullopt;

      auto compiled = text::compileRegex(query.expression, query.options);
      if (!compiled.matcher)
        return std::nullopt;

      return std::move(compiled.matcher);
    }

  } // namespace

  PersistentIndexService::PersistentIndexService(storage::StorageService& storage) : storageService(&storage) {}

  IndexUpdateSummary PersistentIndexService::update(
    const WorktreeInfo& worktree,
    std::span<const FileEntry> files,
    const IndexProgressCallback& onProgress,
    std::stop_token stopToken,
    const IndexUpdateOptions& options)
  {
    std::vector<IndexFileCandidate> candidates;
    candidates.reserve(files.size());

    for (const auto& file : files) {
      candidates.push_back(defaultCandidate(file));
    }

    const auto retainedInputBytes = approximateIndexInputsMemoryBytes(files, {});

    return update(worktree, candidates, onProgress, stopToken, withRetainedMemory(options, retainedInputBytes));
  }

  IndexUpdateSummary PersistentIndexService::update(
    const WorktreeInfo& worktree,
    std::span<const IndexFileCandidate> files,
    const IndexProgressCallback& onProgress,
    std::stop_token stopToken,
    const IndexUpdateOptions& options)
  {
    IndexUpdateSummary summary;
    IndexUpdateProgress progress;
    std::unordered_set<std::string> hashesSeenInUpdate;
    std::vector<IndexDocument> documents;
    auto retainedMemoryBytes = saturatedAdd(options.retainedMemoryBytes, approximateIndexCandidatesMemoryBytes(files));
    retainedMemoryBytes = saturatedAdd(retainedMemoryBytes, reservedDocumentStorageBytes(files.size()));

    progress.total = files.size();
    progress.workingMemoryBytes = retainedMemoryBytes;
    progress.workingMemoryPeakBytes = retainedMemoryBytes;
    summary.workingMemoryPeakBytes = retainedMemoryBytes;

    auto reachMemoryLimit = [&] {
      summary.memoryLimitReached = true;
      progress.memoryLimitReached = true;
      publishProgress(onProgress, progress);
    };

    if (!indexWorkingMemoryFitsBudget(0, retainedMemoryBytes, options.memoryBudgetBytes)) {
      reachMemoryLimit();

      return summary;
    }

    documents.reserve(files.size());

    auto retainDocument = [&](IndexDocument document, std::uint64_t additionalHashMemoryBytes = 0) {
      const auto documentMemoryBytes = approximateIndexDocumentMemoryBytes(document) - sizeof(IndexDocument);
      const auto additionalMemoryBytes = saturatedAdd(documentMemoryBytes, additionalHashMemoryBytes);

      if (!indexWorkingMemoryFitsBudget(retainedMemoryBytes, additionalMemoryBytes, options.memoryBudgetBytes)) {
        reachMemoryLimit();

        return false;
      }

      retainedMemoryBytes += additionalMemoryBytes;
      progress.workingMemoryBytes = retainedMemoryBytes;
      progress.workingMemoryPeakBytes = std::max(progress.workingMemoryPeakBytes, retainedMemoryBytes);
      summary.workingMemoryPeakBytes = std::max(summary.workingMemoryPeakBytes, retainedMemoryBytes);
      documents.push_back(std::move(document));

      return true;
    };

    for (const auto& candidate : files) {
      if (stopToken.stop_requested()) {
        summary.cancelled = true;

        break;
      }

      const auto& file = candidate.file;
      ++progress.processed;
      progress.currentPath = file.relativePath;

      const auto reusableCatalogDocument = storageService->findDocument(worktree.id, file.relativePath);

      if (isDeletedOverlay(candidate.metadata)) {
        if (reusableCatalogDocument && !reusableCatalogDocument->contentHash.empty() &&
            !retainDocument(makeDeletedIndexDocument(worktree, file, candidate.metadata, *reusableCatalogDocument)))
          break;

        ++summary.removed;
        ++progress.removed;
        publishProgress(onProgress, progress);

        continue;
      }

      if (reusableCatalogDocument && canReuseCatalogDocument(*reusableCatalogDocument, candidate)) {
        if (!retainDocument(
              makeReusedIndexDocument(worktree, file, candidate.metadata, reusableIdentity(*reusableCatalogDocument))))
          break;

        ++summary.reusedByCatalog;
        ++progress.reusedByCatalog;
        publishProgress(onProgress, progress);

        continue;
      }

      if (canReuseBlobDocument(candidate.metadata)) {
        const auto reusableBlobDocument = storageService->findReusableDocumentByGitBlobHash(
          candidate.metadata.gitBlob->algorithm, candidate.metadata.gitBlob->value);

        if (reusableBlobDocument) {
          if (!retainDocument(makeReusedIndexDocument(worktree, file, candidate.metadata, *reusableBlobDocument)))
            break;

          ++summary.reusedByBlob;
          ++progress.reusedByBlob;
          publishProgress(onProgress, progress);

          continue;
        }
      }

      std::optional<ContentHash> contentHash;

      try {
        contentHash = computeFileContentHash(file.absolutePath, stopToken);
      } catch (const std::exception&) {
        ++summary.failed;
        ++progress.failed;
        publishProgress(onProgress, progress);

        continue;
      }

      if (!contentHash) {
        summary.cancelled = true;

        break;
      }

      if (!shouldIndexFile(file)) {
        document::DocumentExtractionSummary extractionSummary;

        extractionSummary.status = document::DocumentExtractionStatus::binarySkipped;
        recordExtractorMetrics(
          summary, progress, "binary-sample", file.size, extractionSummary, std::chrono::nanoseconds{}, 0);
        recordSkip(IndexSkipReason::binary, summary, progress);
        if (!retainDocument(makeIndexDocument(worktree, file, candidate.metadata, *contentHash, std::nullopt)))
          break;

        ++summary.indexed;
        ++progress.indexed;
        publishProgress(onProgress, progress);

        continue;
      }

      if (hasUnsupportedDocumentExtension(file.relativePath)) {
        document::DocumentExtractionSummary extractionSummary;

        extractionSummary.status = document::DocumentExtractionStatus::unsupportedFormat;
        recordExtractorMetrics(
          summary, progress, "unsupported-format", file.size, extractionSummary, std::chrono::nanoseconds{}, 0);
        recordSkip(IndexSkipReason::unsupportedFormat, summary, progress);
        if (!retainDocument(makeIndexDocument(worktree, file, candidate.metadata, *contentHash, std::nullopt)))
          break;

        ++summary.indexed;
        ++progress.indexed;
        publishProgress(onProgress, progress);

        continue;
      }

      const auto remainingMemoryBytes = options.memoryBudgetBytes == 0
                                          ? 0
                                          : options.memoryBudgetBytes - retainedMemoryBytes;
      auto indexedText = readIndexedText(file, SearchOptions{}, remainingMemoryBytes, stopToken);

      recordExtractorMetrics(
        summary,
        progress,
        indexedText.extractorName,
        file.size,
        indexedText.extractionSummary,
        indexedText.extractionTime,
        indexedText.indexedTextBytes);

      if (!indexedText.text) {
        if (indexedText.memoryLimitReached) {
          reachMemoryLimit();

          break;
        }

        if (stopToken.stop_requested() || indexedText.cancelled) {
          summary.cancelled = true;

          break;
        }

        if (indexedText.skipReason != IndexSkipReason::none) {
          recordSkip(indexedText.skipReason, summary, progress);
          if (!retainDocument(makeIndexDocument(worktree, file, candidate.metadata, *contentHash, std::nullopt)))
            break;

          ++summary.indexed;
          ++progress.indexed;
        } else {
          if (!retainDocument(makeIndexDocument(worktree, file, candidate.metadata, *contentHash, std::nullopt)))
            break;

          ++summary.failed;
          ++progress.failed;
        }

        publishProgress(onProgress, progress);

        continue;
      }

      const auto hashKey = reusableDocumentKey(contentHash->algorithm, contentHash->value);
      const auto alreadySeenInUpdate = hashesSeenInUpdate.contains(hashKey);
      const auto alreadyStored =
        storageService->findReusableDocumentByContentHash(contentHash->algorithm, contentHash->value).has_value();

      const auto hashMemoryBytes = alreadySeenInUpdate
                                     ? 0
                                     : approximateHashSetNodeOverheadBytes +
                                         static_cast<std::uint64_t>(hashKey.capacity());
      auto document = makeIndexDocument(worktree, file, candidate.metadata, *contentHash, std::move(indexedText.text));

      if (!retainDocument(std::move(document), hashMemoryBytes))
        break;

      if (alreadySeenInUpdate || alreadyStored) {
        ++summary.reusedByHash;
        ++progress.reusedByHash;
      } else {
        ++summary.indexed;
        ++progress.indexed;
      }

      hashesSeenInUpdate.insert(hashKey);
      publishProgress(onProgress, progress);
    }

    if (summary.cancelled || summary.memoryLimitReached)
      return summary;

    storageService->publishGeneration(IndexGeneration{.repositoryId = worktree.repositoryId,
                                                      .worktreeId = worktree.id,
                                                      .headOid = worktree.headOid,
                                                      .branch = worktree.branch,
                                                      .createdAt = std::chrono::system_clock::now(),
                                                      .documents = std::move(documents)});

    return summary;
  }

  IndexUpdateSummary PersistentIndexService::update(
    const WorktreeInfo& worktree,
    std::span<const FileEntry> files,
    std::span<const GitOverlayEntry> overlay,
    const IndexProgressCallback& onProgress,
    std::stop_token stopToken,
    const IndexUpdateOptions& options)
  {
    auto plan = buildOverlayIndexCandidates(worktree, files, overlay);
    const auto retainedInputBytes = approximateIndexInputsMemoryBytes(files, overlay);

    return update(worktree, plan.candidates, onProgress, stopToken, withRetainedMemory(options, retainedInputBytes));
  }

  IndexStalenessReport PersistentIndexService::staleness(const WorktreeInfo& worktree) const
  {
    auto latestGeneration = storageService->latestGenerationForRoot(worktree.root);

    if (!latestGeneration)
      return IndexStalenessReport{.state = IndexStalenessState::missing,
                                  .headChanged = false,
                                  .branchChanged = false,
                                  .latestGeneration = std::nullopt};

    const auto headChanged = latestGeneration->headOid != worktree.headOid;
    const auto branchChanged = latestGeneration->branch != worktree.branch;
    const auto state = headChanged || branchChanged ? IndexStalenessState::stale : IndexStalenessState::fresh;

    return IndexStalenessReport{.state = state,
                                .headChanged = headChanged,
                                .branchChanged = branchChanged,
                                .latestGeneration = std::move(latestGeneration)};
  }

  IndexSearchResult PersistentIndexService::search(
    const SearchQuery& query,
    std::stop_token stopToken) const
  {
    if (query.expression.empty() || (!searchesFileName(query) && !searchesContent(query)))
      return {};

    auto regex = compileIndexedRegex(query);

    if (query.options.mode == SearchMode::regex && !regex)
      return {};

    IndexSearchResult searchResult;
    const auto documents = storageService->visibleDocumentsForRoot(query.root);

    for (const auto& document : documents) {
      if (stopToken.stop_requested())
        break;

      std::size_t fileMatches = 0;

      if (searchesFileName(query)) {
        const auto pathText = document.relativePath.generic_string();
        const auto matches = indexedPathMatches(pathText, query, regex);

        if (!appendIndexedResults(
              document, SearchResultKind::fileName, 0, pathText, matches, query, fileMatches, {}, searchResult))
          return searchResult;
      }

      if (searchesContent(query) && document.indexedText &&
          !appendIndexedContentResults(document, *document.indexedText, query, regex, fileMatches, searchResult))
        return searchResult;
    }

    return searchResult;
  }

} // namespace uburu::index
