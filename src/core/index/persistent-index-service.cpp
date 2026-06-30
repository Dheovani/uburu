#include "core/index/persistent-index-service.hpp"

#include "core/index/content-hash.hpp"

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

    [[nodiscard]] IndexDocument makeIndexDocument(const WorktreeInfo& worktree, const FileEntry& file,
                                                  const ContentHash& contentHash)
    {
      return IndexDocument{.formatVersion = latestIndexDocumentFormatVersion,
                           .repositoryId = worktree.repositoryId,
                           .worktreeId = worktree.id,
                           .relativePath = file.relativePath,
                           .contentHash = contentHash.value,
                           .contentHashAlgorithm = contentHash.algorithm,
                           .gitBlobHash = std::nullopt,
                           .gitBlobHashAlgorithm = GitObjectHashAlgorithm::unknown,
                           .status = GitFileStatus::clean,
                           .size = file.size,
                           .indexedAt = std::chrono::system_clock::now(),
                           .deleted = false};
    }

    void publishProgress(const IndexProgressCallback& onProgress, const IndexUpdateProgress& progress)
    {
      if (!onProgress)
        return;

      onProgress(progress);
    }

  } // namespace

  PersistentIndexService::PersistentIndexService(storage::StorageService& storage) : storageService(&storage) {}

  IndexUpdateSummary PersistentIndexService::update(const WorktreeInfo& worktree, std::span<const FileEntry> files,
                                                    const IndexProgressCallback& onProgress, std::stop_token stopToken)
  {
    IndexUpdateSummary summary;
    IndexUpdateProgress progress;
    std::unordered_set<std::string> hashesSeenInUpdate;
    std::vector<IndexDocument> documents;

    progress.total = files.size();
    documents.reserve(files.size());

    for (const auto& file : files) {
      if (stopToken.stop_requested()) {
        summary.cancelled = true;

        break;
      }

      ++progress.processed;
      progress.currentPath = file.relativePath;

      if (!shouldIndexFile(file)) {
        publishProgress(onProgress, progress);

        continue;
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
      documents.push_back(makeIndexDocument(worktree, file, *contentHash));
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

  std::vector<SearchResult> PersistentIndexService::search(const SearchQuery& query, std::stop_token stopToken) const
  {
    static_cast<void>(query);
    static_cast<void>(stopToken);

    return {};
  }

} // namespace uburu::index
