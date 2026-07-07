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

  struct RichFormatSafetyLimits
  {
    std::uint64_t maximumExpandedArchiveBytes{defaultMaximumExpandedArchiveBytes};
    std::uint64_t maximumSingleExpandedEntryBytes{defaultMaximumSingleExpandedEntryBytes};
    std::size_t maximumArchiveEntries{defaultMaximumArchiveEntries};
    std::size_t maximumArchiveNestingDepth{defaultMaximumArchiveNestingDepth};
    std::uint64_t maximumCompressionRatio{defaultMaximumCompressionRatio};
  };

  struct ArchiveEntrySafetyInput
  {
    std::uint64_t compressedBytes{0};
    std::uint64_t expandedBytes{0};
  };

  struct ArchiveSafetyInput
  {
    std::uint64_t totalExpandedBytes{0};
    std::size_t entryCount{0};
    std::size_t nestingDepth{0};
  };

  [[nodiscard]] RichFormatSafetyStatus validateArchiveEntrySafety(ArchiveEntrySafetyInput entry,
                                                                  RichFormatSafetyLimits limits = {});
  [[nodiscard]] RichFormatSafetyStatus validateArchiveSafety(ArchiveSafetyInput archive,
                                                             RichFormatSafetyLimits limits = {});
  [[nodiscard]] std::string_view richFormatSafetyStatusName(RichFormatSafetyStatus status);

} // namespace uburu::text
