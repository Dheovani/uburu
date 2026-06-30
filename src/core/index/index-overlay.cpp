#include "core/index/index-overlay.hpp"

#include "core/filesystem/path-normalization.hpp"

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace uburu::index
{
  namespace
  {

    [[nodiscard]] std::string relativePathKey(const WorktreeInfo& worktree, const std::filesystem::path& relativePath)
    {
      return filesystem::normalizedPathKey(worktree.root / relativePath);
    }

    [[nodiscard]] FileEntry deletedFileEntry(const WorktreeInfo& worktree, const std::filesystem::path& relativePath)
    {
      return FileEntry{.absolutePath = worktree.root / relativePath,
                       .relativePath = relativePath,
                       .size = 0,
                       .modifiedAt = {},
                       .searchRoot = worktree.root};
    }

    [[nodiscard]] IndexFileMetadata overlayMetadata(const GitOverlayEntry& entry)
    {
      return IndexFileMetadata{.status = entry.status, .gitBlob = entry.reusableBlob};
    }

    [[nodiscard]] IndexFileCandidate deletedCandidate(const WorktreeInfo& worktree,
                                                      const std::filesystem::path& relativePath,
                                                      const GitOverlayEntry& entry)
    {
      return IndexFileCandidate{
        .file = deletedFileEntry(worktree, relativePath),
        .metadata = IndexFileMetadata{.status = GitFileStatus::deleted, .gitBlob = entry.reusableBlob},
      };
    }

    [[nodiscard]] bool shouldUseWorkingTreeFile(GitOverlayDisposition disposition)
    {
      switch (disposition) {
      case GitOverlayDisposition::useIndexedContent:
      case GitOverlayDisposition::replaceWithWorkingTree:
      case GitOverlayDisposition::addWorkingTreeFile:
      case GitOverlayDisposition::conflict:
        return true;
      case GitOverlayDisposition::hideIndexedContent:
        return false;
      }

      return false;
    }

    void hideIndexedPath(OverlayCandidatePlan& plan,
                         std::unordered_set<std::string>& hiddenPaths,
                         const WorktreeInfo& worktree,
                         const std::filesystem::path& relativePath,
                         const GitOverlayEntry& entry)
    {
      const auto key = relativePathKey(worktree, relativePath);

      if (!hiddenPaths.insert(key).second)
        return;

      plan.candidates.push_back(deletedCandidate(worktree, relativePath, entry));
      ++plan.hiddenIndexedPaths;
    }

  } // namespace

  OverlayCandidatePlan buildOverlayIndexCandidates(const WorktreeInfo& worktree,
                                                   std::span<const FileEntry> scannedFiles,
                                                   std::span<const GitOverlayEntry> overlay)
  {
    OverlayCandidatePlan plan;
    std::unordered_map<std::string, std::size_t> candidateIndexes;
    std::unordered_set<std::string> hiddenPaths;

    plan.candidates.reserve(scannedFiles.size() + overlay.size());

    for (const auto& file : scannedFiles) {
      const auto key = relativePathKey(worktree, file.relativePath);
      candidateIndexes.emplace(key, plan.candidates.size());
      plan.candidates.push_back(IndexFileCandidate{.file = file, .metadata = IndexFileMetadata{}});
    }

    for (const auto& entry : overlay) {
      if (entry.previousRelativePath && *entry.previousRelativePath != entry.relativePath)
        hideIndexedPath(plan, hiddenPaths, worktree, *entry.previousRelativePath, entry);

      if (entry.disposition == GitOverlayDisposition::hideIndexedContent) {
        hideIndexedPath(plan, hiddenPaths, worktree, entry.relativePath, entry);

        continue;
      }

      if (!shouldUseWorkingTreeFile(entry.disposition))
        continue;

      const auto key = relativePathKey(worktree, entry.relativePath);
      const auto candidate = candidateIndexes.find(key);

      if (candidate == candidateIndexes.end()) {
        ++plan.missingWorkingTreeFiles;

        continue;
      }

      plan.candidates[candidate->second].metadata = overlayMetadata(entry);
    }

    return plan;
  }

} // namespace uburu::index
