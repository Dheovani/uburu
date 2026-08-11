#pragma once

#include "core/index/index-service.hpp"

#include <cstdint>
#include <span>

namespace uburu::index
{

  /**
   * Estimates memory owned by filesystem metadata retained for indexing.
   */
  [[nodiscard]]
  std::uint64_t approximateFileEntryMemoryBytes(const FileEntry& file);

  /**
   * Estimates memory owned by an indexing-candidate collection.
   */
  [[nodiscard]]
  std::uint64_t approximateIndexCandidatesMemoryBytes(std::span<const IndexFileCandidate> candidates);

  /**
   * Estimates memory owned by scanned files and Git overlay inputs.
   */
  [[nodiscard]]
  std::uint64_t approximateIndexInputsMemoryBytes(
    std::span<const FileEntry> files,
    std::span<const GitOverlayEntry> overlay);

  /**
   * Estimates memory owned by one document waiting for generation publication.
   */
  [[nodiscard]]
  std::uint64_t approximateIndexDocumentMemoryBytes(const IndexDocument& document);

  /**
   * Checks a zero-as-unlimited indexing budget without unsigned overflow.
   */
  [[nodiscard]]
  bool indexWorkingMemoryFitsBudget(
    std::uint64_t retainedMemoryBytes,
    std::uint64_t additionalMemoryBytes,
    std::uintmax_t memoryBudgetBytes);

} // namespace uburu::index
