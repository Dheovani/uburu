#include "core/text/rich-format-safety.hpp"

#include <limits>

namespace uburu::text
{
  namespace
  {

    [[nodiscard]]
    bool exceedsCompressionRatio(std::uint64_t compressedBytes,
                                 std::uint64_t expandedBytes,
                                 std::uint64_t maximumCompressionRatio)
    {
      if (expandedBytes == 0)
        return false;

      if (compressedBytes == 0)
        return true;

      if (maximumCompressionRatio == 0)
        return true;

      const auto maximumMultiplier = std::numeric_limits<std::uint64_t>::max() / maximumCompressionRatio;

      if (compressedBytes > maximumMultiplier)
        return false;

      return expandedBytes > compressedBytes * maximumCompressionRatio;
    }

  } // namespace

  RichFormatSafetyStatus validateArchiveEntrySafety(ArchiveEntrySafetyInput entry, RichFormatSafetyLimits limits)
  {
    if (entry.expandedBytes > limits.maximumSingleExpandedEntryBytes)
      return RichFormatSafetyStatus::singleEntryBytesExceeded;

    if (exceedsCompressionRatio(entry.compressedBytes, entry.expandedBytes, limits.maximumCompressionRatio))
      return RichFormatSafetyStatus::compressionRatioExceeded;

    return RichFormatSafetyStatus::accepted;
  }

  RichFormatSafetyStatus validateArchiveSafety(ArchiveSafetyInput archive, RichFormatSafetyLimits limits)
  {
    if (archive.totalExpandedBytes > limits.maximumExpandedArchiveBytes)
      return RichFormatSafetyStatus::totalExpandedBytesExceeded;

    if (archive.entryCount > limits.maximumArchiveEntries)
      return RichFormatSafetyStatus::entryCountExceeded;

    if (archive.nestingDepth > limits.maximumArchiveNestingDepth)
      return RichFormatSafetyStatus::nestingDepthExceeded;

    return RichFormatSafetyStatus::accepted;
  }

  std::string_view richFormatSafetyStatusName(RichFormatSafetyStatus status)
  {
    switch (status) {
    case RichFormatSafetyStatus::accepted:
      return "accepted";
    case RichFormatSafetyStatus::totalExpandedBytesExceeded:
      return "totalExpandedBytesExceeded";
    case RichFormatSafetyStatus::singleEntryBytesExceeded:
      return "singleEntryBytesExceeded";
    case RichFormatSafetyStatus::entryCountExceeded:
      return "entryCountExceeded";
    case RichFormatSafetyStatus::nestingDepthExceeded:
      return "nestingDepthExceeded";
    case RichFormatSafetyStatus::compressionRatioExceeded:
      return "compressionRatioExceeded";
    }

    return "unknown";
  }

} // namespace uburu::text
