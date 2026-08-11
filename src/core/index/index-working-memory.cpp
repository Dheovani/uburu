#include "core/index/index-working-memory.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace uburu::index
{
  namespace
  {

    [[nodiscard]]
    std::uint64_t stringMemoryBytes(const std::string& value)
    {
      return static_cast<std::uint64_t>(value.capacity());
    }

    [[nodiscard]]
    std::uint64_t optionalStringMemoryBytes(const std::optional<std::string>& value)
    {
      return value ? stringMemoryBytes(*value) : 0;
    }

    [[nodiscard]]
    std::uint64_t pathMemoryBytes(const std::filesystem::path& path)
    {
      return static_cast<std::uint64_t>(path.native().size() * sizeof(std::filesystem::path::value_type));
    }

    [[nodiscard]]
    std::uint64_t optionalPathMemoryBytes(const std::optional<std::filesystem::path>& path)
    {
      return path ? pathMemoryBytes(*path) : 0;
    }

    [[nodiscard]]
    std::uint64_t gitObjectMemoryBytes(const std::optional<GitObjectId>& object)
    {
      return object ? stringMemoryBytes(object->value) : 0;
    }

    [[nodiscard]]
    std::uint64_t indexCandidateMemoryBytes(const IndexFileCandidate& candidate)
    {
      return sizeof(IndexFileCandidate) + approximateFileEntryMemoryBytes(candidate.file) - sizeof(FileEntry) +
             gitObjectMemoryBytes(candidate.metadata.gitBlob);
    }

    [[nodiscard]]
    std::uint64_t overlayEntryMemoryBytes(const GitOverlayEntry& entry)
    {
      return sizeof(GitOverlayEntry) + pathMemoryBytes(entry.relativePath) +
             optionalPathMemoryBytes(entry.previousRelativePath) + gitObjectMemoryBytes(entry.reusableBlob);
    }

  } // namespace

  std::uint64_t approximateFileEntryMemoryBytes(const FileEntry& file)
  {
    return sizeof(FileEntry) + pathMemoryBytes(file.absolutePath) + pathMemoryBytes(file.relativePath) +
           pathMemoryBytes(file.searchRoot);
  }

  std::uint64_t approximateIndexCandidatesMemoryBytes(std::span<const IndexFileCandidate> candidates)
  {
    std::uint64_t memoryBytes = 0;

    for (const auto& candidate : candidates)
      memoryBytes += indexCandidateMemoryBytes(candidate);

    return memoryBytes;
  }

  std::uint64_t approximateIndexInputsMemoryBytes(
    std::span<const FileEntry> files,
    std::span<const GitOverlayEntry> overlay)
  {
    std::uint64_t memoryBytes = 0;

    for (const auto& file : files)
      memoryBytes += approximateFileEntryMemoryBytes(file);

    for (const auto& entry : overlay)
      memoryBytes += overlayEntryMemoryBytes(entry);

    return memoryBytes;
  }

  std::uint64_t approximateIndexDocumentMemoryBytes(const IndexDocument& document)
  {
    auto memoryBytes = static_cast<std::uint64_t>(sizeof(IndexDocument));

    memoryBytes += stringMemoryBytes(document.repositoryId);
    memoryBytes += stringMemoryBytes(document.worktreeId);
    memoryBytes += pathMemoryBytes(document.relativePath);
    memoryBytes += stringMemoryBytes(document.contentHash);
    memoryBytes += optionalStringMemoryBytes(document.gitBlobHash);
    memoryBytes += optionalStringMemoryBytes(document.indexedText);

    return memoryBytes;
  }

  bool indexWorkingMemoryFitsBudget(
    std::uint64_t retainedMemoryBytes,
    std::uint64_t additionalMemoryBytes,
    std::uintmax_t memoryBudgetBytes)
  {
    if (memoryBudgetBytes == 0)
      return true;

    return retainedMemoryBytes <= memoryBudgetBytes && additionalMemoryBytes <= memoryBudgetBytes - retainedMemoryBytes;
  }

} // namespace uburu::index
