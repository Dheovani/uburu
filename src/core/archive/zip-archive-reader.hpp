#pragma once

#include "core/text/rich-format-safety.hpp"

#include <cstdint>
#include <filesystem>
#include <stop_token>
#include <string>
#include <system_error>
#include <vector>

namespace uburu::archive
{

  enum class ZipArchiveReadStatus
  {
    completed,
    cancelled,
    openFailed,
    readFailed,
    invalidArchive,
    unsupportedZip64,
    unsupportedCompressionMethod,
    entryNotFound,
    unsafeEntryName,
    safetyLimitExceeded,
    decompressionFailed
  };

  /**
   * Describes one safe central-directory entry without owning its payload.
   */
  struct ZipArchiveEntry
  {
    std::string rawName;
    std::filesystem::path normalizedPath;
    std::uint16_t compressionMethod{0};
    std::uint64_t localHeaderOffset{0};
    std::uint64_t compressedBytes{0};
    std::uint64_t expandedBytes{0};
    bool directory{false};
  };

  /**
   * Contains the validated central-directory catalog for a ZIP archive.
   */
  struct ZipArchiveCatalog
  {
    ZipArchiveReadStatus status{ZipArchiveReadStatus::completed};
    text::RichFormatSafetyStatus safetyStatus{text::RichFormatSafetyStatus::accepted};
    std::error_code error;
    std::vector<ZipArchiveEntry> entries;
  };

  /**
   * Contains the bounded payload bytes read for one ZIP entry.
   */
  struct ZipEntryReadResult
  {
    ZipArchiveReadStatus status{ZipArchiveReadStatus::completed};
    text::RichFormatSafetyStatus safetyStatus{text::RichFormatSafetyStatus::accepted};
    std::error_code error;
    std::vector<unsigned char> bytes;
  };

  class ZipArchiveReader
  {
  public:
    /**
     * Reads and validates the archive catalog without extracting entry payloads.
     */
    [[nodiscard]]
    ZipArchiveCatalog readCatalog(
      const std::filesystem::path& path,
      text::RichFormatSafetyLimits limits = {},
      std::stop_token stopToken = {}) const;

    /**
     * Reads one previously cataloged entry using the same safety boundaries.
     */
    [[nodiscard]]
    ZipEntryReadResult readEntry(
      const std::filesystem::path& path,
      const ZipArchiveEntry& entry,
      text::RichFormatSafetyLimits limits = {},
      std::stop_token stopToken = {}) const;
  };

  /**
   * Returns a stable diagnostic name for a ZIP archive read status.
   */
  [[nodiscard]]
  std::string_view zipArchiveReadStatusName(ZipArchiveReadStatus status);

} // namespace uburu::archive
