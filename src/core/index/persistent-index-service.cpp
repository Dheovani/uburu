#include "core/index/persistent-index-service.hpp"

#include "core/index/content-hash.hpp"
#include "core/index/index-overlay.hpp"
#include "core/text/regex-matcher.hpp"
#include "core/text/text-file-reader.hpp"
#include "core/text/text-matcher.hpp"

#include <chrono>
#include <deque>
#include <exception>
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

    [[nodiscard]] std::string reusableDocumentKey(ContentHashAlgorithm algorithm, std::string_view hash)
    {
      return std::to_string(static_cast<int>(algorithm)) + ":" + std::string{hash};
    }

    [[nodiscard]] bool shouldIndexFile(const FileEntry& file)
    {
      return !file.binary;
    }

    [[nodiscard]] bool isDeletedOverlay(const IndexFileMetadata& metadata)
    {
      return metadata.status == GitFileStatus::deleted;
    }

    [[nodiscard]] bool canReuseCatalogDocument(const IndexDocument& document, const IndexFileCandidate& candidate)
    {
      const auto& file = candidate.file;

      return !document.deleted && !document.contentHash.empty() && document.size == file.size &&
             document.modifiedAt == file.modifiedAt && document.status == candidate.metadata.status &&
             document.status == GitFileStatus::clean && document.contentHashAlgorithm != ContentHashAlgorithm::unknown;
    }

    [[nodiscard]] bool canReuseBlobDocument(const IndexFileMetadata& metadata)
    {
      return metadata.status == GitFileStatus::clean && metadata.gitBlob.has_value() &&
             metadata.gitBlob->algorithm != GitObjectHashAlgorithm::unknown && !metadata.gitBlob->value.empty();
    }

    [[nodiscard]] std::optional<std::string> gitBlobHash(const IndexFileMetadata& metadata)
    {
      if (!metadata.gitBlob)
        return std::nullopt;

      return metadata.gitBlob->value;
    }

    [[nodiscard]] GitObjectHashAlgorithm gitBlobHashAlgorithm(const IndexFileMetadata& metadata)
    {
      if (!metadata.gitBlob)
        return GitObjectHashAlgorithm::unknown;

      return metadata.gitBlob->algorithm;
    }

    [[nodiscard]] IndexDocument makeIndexDocument(const WorktreeInfo& worktree,
                                                  const FileEntry& file,
                                                  const IndexFileMetadata& metadata,
                                                  const ContentHash& contentHash,
                                                  std::string indexedText)
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

    [[nodiscard]] IndexDocument makeDeletedIndexDocument(const WorktreeInfo& worktree,
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

    [[nodiscard]] IndexDocument makeReusedIndexDocument(const WorktreeInfo& worktree,
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

    [[nodiscard]] IndexedDocumentIdentity reusableIdentity(const IndexDocument& document)
    {
      return IndexedDocumentIdentity{.formatVersion = document.formatVersion,
                                     .contentHash = document.contentHash,
                                     .contentHashAlgorithm = document.contentHashAlgorithm,
                                     .gitBlobHash = document.gitBlobHash,
                                     .gitBlobHashAlgorithm = document.gitBlobHashAlgorithm,
                                     .size = document.size,
                                     .indexedAt = document.indexedAt};
    }

    [[nodiscard]] IndexFileCandidate defaultCandidate(const FileEntry& file)
    {
      return IndexFileCandidate{.file = file, .metadata = IndexFileMetadata{}};
    }

    void publishProgress(const IndexProgressCallback& onProgress, const IndexUpdateProgress& progress)
    {
      if (!onProgress)
        return;

      onProgress(progress);
    }

    [[nodiscard]] bool searchesFileName(const SearchQuery& query)
    {
      return query.options.target == SearchTarget::fileName || query.options.target == SearchTarget::contentAndFileName;
    }

    [[nodiscard]] bool searchesContent(const SearchQuery& query)
    {
      return query.options.target == SearchTarget::content || query.options.target == SearchTarget::contentAndFileName;
    }

    [[nodiscard]] std::optional<std::string>
    readIndexedText(const FileEntry& file, const SearchOptions& options, std::stop_token stopToken)
    {
      std::string indexedText;
      bool firstLine = true;

      const auto summary = text::readTextFileLines(
        file.absolutePath,
        options,
        [&](const text::TextLine& line) {
          if (!firstLine)
            indexedText.push_back('\n');

          indexedText += line.text;
          firstLine = false;

          return true;
        },
        stopToken);

      if (summary.status != text::TextReadStatus::completed)
        return std::nullopt;

      return indexedText;
    }

    [[nodiscard]] std::vector<text::MatchPosition> indexedPathMatches(std::string_view pathText,
                                                                      const SearchQuery& query,
                                                                      const std::optional<text::RegexMatcher>& regex)
    {
      if (regex)
        return regex->findAll(pathText).matches;

      return text::findAllLiterals(pathText, query.expression, query.options);
    }

    [[nodiscard]] std::optional<std::vector<text::MatchPosition>>
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

    [[nodiscard]] std::vector<MatchSpan> makeHighlights(std::string_view lineText,
                                                        const std::vector<text::MatchPosition>& matches)
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

    [[nodiscard]] SearchResult makeIndexedResult(const IndexDocument& document,
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

    [[nodiscard]] bool appendIndexedResults(const IndexDocument& document,
                                            SearchResultKind kind,
                                            std::size_t line,
                                            std::string_view lineText,
                                            const std::vector<text::MatchPosition>& matches,
                                            const SearchQuery& query,
                                            std::size_t& fileMatches,
                                            const std::deque<std::string>& contextBefore,
                                            std::vector<SearchResult>& results)
    {
      for (const auto& match : matches) {
        if (results.size() >= query.options.resultLimit)
          return false;

        if (fileMatches >= query.options.perFileResultLimit)
          return true;

        ++fileMatches;
        results.push_back(makeIndexedResult(document, kind, line, lineText, match, matches, query, contextBefore));
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
                                     std::vector<SearchResult>& results)
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
                                                       results))
          return false;

        rememberContextLine(contextBefore, lineText, query);

        if (lineEnd == std::string_view::npos)
          break;

        lineStart = lineEnd + 1;
      }

      return true;
    }

    [[nodiscard]] std::optional<text::RegexMatcher> compileIndexedRegex(const SearchQuery& query)
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

  IndexUpdateSummary PersistentIndexService::update(const WorktreeInfo& worktree,
                                                    std::span<const FileEntry> files,
                                                    const IndexProgressCallback& onProgress,
                                                    std::stop_token stopToken)
  {
    std::vector<IndexFileCandidate> candidates;
    candidates.reserve(files.size());

    for (const auto& file : files) {
      candidates.push_back(defaultCandidate(file));
    }

    return update(worktree, candidates, onProgress, stopToken);
  }

  IndexUpdateSummary PersistentIndexService::update(const WorktreeInfo& worktree,
                                                    std::span<const IndexFileCandidate> files,
                                                    const IndexProgressCallback& onProgress,
                                                    std::stop_token stopToken)
  {
    IndexUpdateSummary summary;
    IndexUpdateProgress progress;
    std::unordered_set<std::string> hashesSeenInUpdate;
    std::vector<IndexDocument> documents;

    progress.total = files.size();
    documents.reserve(files.size());

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
        ++summary.removed;
        ++progress.removed;

        if (reusableCatalogDocument && !reusableCatalogDocument->contentHash.empty())
          documents.push_back(makeDeletedIndexDocument(worktree, file, candidate.metadata, *reusableCatalogDocument));

        publishProgress(onProgress, progress);

        continue;
      }

      if (!shouldIndexFile(file)) {
        publishProgress(onProgress, progress);

        continue;
      }

      if (reusableCatalogDocument && canReuseCatalogDocument(*reusableCatalogDocument, candidate)) {
        ++summary.reusedByCatalog;
        ++progress.reusedByCatalog;
        documents.push_back(
          makeReusedIndexDocument(worktree, file, candidate.metadata, reusableIdentity(*reusableCatalogDocument)));
        publishProgress(onProgress, progress);

        continue;
      }

      if (canReuseBlobDocument(candidate.metadata)) {
        const auto reusableBlobDocument = storageService->findReusableDocumentByGitBlobHash(
          candidate.metadata.gitBlob->algorithm, candidate.metadata.gitBlob->value);

        if (reusableBlobDocument) {
          ++summary.reusedByBlob;
          ++progress.reusedByBlob;
          documents.push_back(makeReusedIndexDocument(worktree, file, candidate.metadata, *reusableBlobDocument));
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

      const auto indexedText = readIndexedText(file, SearchOptions{}, stopToken);

      if (!indexedText) {
        if (stopToken.stop_requested()) {
          summary.cancelled = true;

          break;
        }

        ++summary.failed;
        ++progress.failed;
        publishProgress(onProgress, progress);

        continue;
      }

      const auto hashKey = reusableDocumentKey(contentHash->algorithm, contentHash->value);
      const auto alreadySeenInUpdate = hashesSeenInUpdate.contains(hashKey);
      const auto alreadyStored =
        storageService->findReusableDocumentByContentHash(contentHash->algorithm, contentHash->value).has_value();

      if (alreadySeenInUpdate || alreadyStored) {
        ++summary.reusedByHash;
        ++progress.reusedByHash;
      } else {
        ++summary.indexed;
        ++progress.indexed;
      }

      hashesSeenInUpdate.insert(hashKey);
      documents.push_back(makeIndexDocument(worktree, file, candidate.metadata, *contentHash, *indexedText));
      publishProgress(onProgress, progress);
    }

    if (summary.cancelled)
      return summary;

    storageService->publishGeneration(IndexGeneration{.repositoryId = worktree.repositoryId,
                                                      .worktreeId = worktree.id,
                                                      .headOid = worktree.headOid,
                                                      .branch = worktree.branch,
                                                      .createdAt = std::chrono::system_clock::now(),
                                                      .documents = std::move(documents)});

    return summary;
  }

  IndexUpdateSummary PersistentIndexService::update(const WorktreeInfo& worktree,
                                                    std::span<const FileEntry> files,
                                                    std::span<const GitOverlayEntry> overlay,
                                                    const IndexProgressCallback& onProgress,
                                                    std::stop_token stopToken)
  {
    auto plan = buildOverlayIndexCandidates(worktree, files, overlay);

    return update(worktree, plan.candidates, onProgress, stopToken);
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

  std::vector<SearchResult> PersistentIndexService::search(const SearchQuery& query, std::stop_token stopToken) const
  {
    if (query.expression.empty() || (!searchesFileName(query) && !searchesContent(query)))
      return {};

    auto regex = compileIndexedRegex(query);

    if (query.options.mode == SearchMode::regex && !regex)
      return {};

    std::vector<SearchResult> results;
    const auto documents = storageService->visibleDocumentsForRoot(query.root);

    for (const auto& document : documents) {
      if (stopToken.stop_requested())
        break;

      std::size_t fileMatches = 0;

      if (searchesFileName(query)) {
        const auto pathText = document.relativePath.generic_string();
        const auto matches = indexedPathMatches(pathText, query, regex);

        if (!appendIndexedResults(
              document, SearchResultKind::fileName, 0, pathText, matches, query, fileMatches, {}, results))
          return results;
      }

      if (searchesContent(query) && document.indexedText &&
          !appendIndexedContentResults(document, *document.indexedText, query, regex, fileMatches, results))
        return results;
    }

    return results;
  }

} // namespace uburu::index
