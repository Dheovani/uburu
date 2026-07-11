#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace uburu::text
{

  inline constexpr std::uint64_t defaultMaximumExpandedArchiveBytes = 512ULL * 1024ULL * 1024ULL;
  inline constexpr std::uint64_t defaultMaximumSingleExpandedEntryBytes = 128ULL * 1024ULL * 1024ULL;
  inline constexpr std::size_t defaultMaximumArchiveEntries = 10'000;
  inline constexpr std::size_t defaultMaximumArchiveNestingDepth = 16;
  inline constexpr std::uint64_t defaultMaximumCompressionRatio = 100;

  enum class RichFormatSafetyStatus
  {
    accepted,
    totalExpandedBytesExceeded,
    singleEntryBytesExceeded,
    entryCountExceeded,
    nestingDepthExceeded,
    compressionRatioExceeded
  };

  /**
   * Defines archive expansion limits used before rich document parsing.
   */
  struct RichFormatSafetyLimits
  {
    std::uint64_t maximumExpandedArchiveBytes{defaultMaximumExpandedArchiveBytes};
    std::uint64_t maximumSingleExpandedEntryBytes{defaultMaximumSingleExpandedEntryBytes};
    std::size_t maximumArchiveEntries{defaultMaximumArchiveEntries};
    std::size_t maximumArchiveNestingDepth{defaultMaximumArchiveNestingDepth};
    std::uint64_t maximumCompressionRatio{defaultMaximumCompressionRatio};
  };

  /**
   * Provides compressed and expanded sizes for a single archive entry.
   */
  struct ArchiveEntrySafetyInput
  {
    std::uint64_t compressedBytes{0};
    std::uint64_t expandedBytes{0};
  };

  /**
   * Provides aggregate archive metadata for whole-archive validation.
   */
  struct ArchiveSafetyInput
  {
    std::uint64_t totalExpandedBytes{0};
    std::size_t entryCount{0};
    std::size_t nestingDepth{0};
  };

  /**
   * Validates one archive entry before its payload can be inflated.
   */
  [[nodiscard]]
  RichFormatSafetyStatus validateArchiveEntrySafety(ArchiveEntrySafetyInput entry, RichFormatSafetyLimits limits = {});

  /**
   * Validates aggregate archive metadata before any format-specific parsing.
   */
  [[nodiscard]]
  RichFormatSafetyStatus validateArchiveSafety(ArchiveSafetyInput archive, RichFormatSafetyLimits limits = {});

  /**
   * Returns a stable diagnostic name for a rich-format safety status.
   */
  [[nodiscard]]
  std::string_view richFormatSafetyStatusName(RichFormatSafetyStatus status);

} // namespace uburu::text
