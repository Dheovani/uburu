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
    unsafeEntryName,
    safetyLimitExceeded
  };

  struct ZipArchiveEntry
  {
    std::string rawName;
    std::filesystem::path normalizedPath;
    std::uint16_t compressionMethod{0};
    std::uint64_t compressedBytes{0};
    std::uint64_t expandedBytes{0};
    bool directory{false};
  };

  struct ZipArchiveCatalog
  {
    ZipArchiveReadStatus status{ZipArchiveReadStatus::completed};
    text::RichFormatSafetyStatus safetyStatus{text::RichFormatSafetyStatus::accepted};
    std::error_code error;
    std::vector<ZipArchiveEntry> entries;
  };

  class ZipArchiveReader
  {
  public:
    [[nodiscard]]
    ZipArchiveCatalog readCatalog(const std::filesystem::path& path,
                                  text::RichFormatSafetyLimits limits = {},
                                  std::stop_token stopToken = {}) const;
  };

  [[nodiscard]]
  std::string_view zipArchiveReadStatusName(ZipArchiveReadStatus status);

} // namespace uburu::archive
