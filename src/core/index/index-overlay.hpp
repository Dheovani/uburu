#pragma once

#include "core/index/index-service.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace uburu::index
{

  /**
   * Converts Git overlay state into indexing candidates and hidden indexed paths.
   */
  struct OverlayCandidatePlan
  {
    std::vector<IndexFileCandidate> candidates;
    std::size_t hiddenIndexedPaths{0};
    std::size_t missingWorkingTreeFiles{0};
  };

  [[nodiscard]]
  OverlayCandidatePlan buildOverlayIndexCandidates(
    const WorktreeInfo& worktree,
    std::span<const FileEntry> scannedFiles,
    std::span<const GitOverlayEntry> overlay);

} // namespace uburu::index
