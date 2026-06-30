#include "core/index/persistent-index-service.hpp"

#include "core/index/content-hash.hpp"
#include "core/index/index-overlay.hpp"

#include <chrono>
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
                                                  const ContentHash& contentHash)
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
                           .deleted = false};
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
                           .deleted = true};
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
                           .deleted = false};
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
      documents.push_back(makeIndexDocument(worktree, file, candidate.metadata, *contentHash));
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

  std::vector<SearchResult> PersistentIndexService::search(const SearchQuery& query, std::stop_token stopToken) const
  {
    static_cast<void>(query);
    static_cast<void>(stopToken);

    return {};
  }

} // namespace uburu::index
