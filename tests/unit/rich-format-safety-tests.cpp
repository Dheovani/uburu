#include "core/text/rich-format-safety.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("rich format safety accepts bounded archive metadata")
{
  const uburu::text::RichFormatSafetyLimits limits{.maximumExpandedArchiveBytes = 1'000,
                                                   .maximumSingleExpandedEntryBytes = 500,
                                                   .maximumArchiveEntries = 10,
                                                   .maximumArchiveNestingDepth = 2,
                                                   .maximumCompressionRatio = 20};

  CHECK(uburu::text::validateArchiveSafety({.totalExpandedBytes = 900, .entryCount = 10, .nestingDepth = 2}, limits) ==
        uburu::text::RichFormatSafetyStatus::accepted);
  CHECK(uburu::text::validateArchiveEntrySafety({.compressedBytes = 25, .expandedBytes = 500}, limits) ==
        uburu::text::RichFormatSafetyStatus::accepted);
}

TEST_CASE("rich format safety rejects archive-wide decompression limits")
{
  const uburu::text::RichFormatSafetyLimits limits{.maximumExpandedArchiveBytes = 1'000,
                                                   .maximumSingleExpandedEntryBytes = 500,
                                                   .maximumArchiveEntries = 10,
                                                   .maximumArchiveNestingDepth = 2,
                                                   .maximumCompressionRatio = 20};

  CHECK(uburu::text::validateArchiveSafety({.totalExpandedBytes = 1'001, .entryCount = 1, .nestingDepth = 1}, limits) ==
        uburu::text::RichFormatSafetyStatus::totalExpandedBytesExceeded);
  CHECK(uburu::text::validateArchiveSafety({.totalExpandedBytes = 100, .entryCount = 11, .nestingDepth = 1}, limits) ==
        uburu::text::RichFormatSafetyStatus::entryCountExceeded);
  CHECK(uburu::text::validateArchiveSafety({.totalExpandedBytes = 100, .entryCount = 1, .nestingDepth = 3}, limits) ==
        uburu::text::RichFormatSafetyStatus::nestingDepthExceeded);
}

TEST_CASE("rich format safety rejects suspicious archive entries")
{
  const uburu::text::RichFormatSafetyLimits limits{.maximumExpandedArchiveBytes = 1'000,
                                                   .maximumSingleExpandedEntryBytes = 500,
                                                   .maximumArchiveEntries = 10,
                                                   .maximumArchiveNestingDepth = 2,
                                                   .maximumCompressionRatio = 20};

  CHECK(uburu::text::validateArchiveEntrySafety({.compressedBytes = 50, .expandedBytes = 501}, limits) ==
        uburu::text::RichFormatSafetyStatus::singleEntryBytesExceeded);
  CHECK(uburu::text::validateArchiveEntrySafety({.compressedBytes = 10, .expandedBytes = 201}, limits) ==
        uburu::text::RichFormatSafetyStatus::compressionRatioExceeded);
  CHECK(uburu::text::validateArchiveEntrySafety({.compressedBytes = 0, .expandedBytes = 1}, limits) ==
        uburu::text::RichFormatSafetyStatus::compressionRatioExceeded);
}

TEST_CASE("rich format safety exposes stable status names")
{
  CHECK(uburu::text::richFormatSafetyStatusName(uburu::text::RichFormatSafetyStatus::accepted) == "accepted");
  CHECK(uburu::text::richFormatSafetyStatusName(uburu::text::RichFormatSafetyStatus::compressionRatioExceeded) ==
        "compressionRatioExceeded");
}
