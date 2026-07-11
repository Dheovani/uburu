#include "core/archive/zip-archive-reader.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string_view>

namespace
{

  constexpr std::size_t maximumInputBytes = 65536;
  constexpr std::size_t maximumEntriesToRead = 8;
  constexpr std::uint64_t maximumExpandedArchiveBytes = 128ULL * 1024ULL;
  constexpr std::uint64_t maximumSingleExpandedEntryBytes = 32ULL * 1024ULL;
  constexpr std::size_t maximumArchiveEntries = 256;
  constexpr std::size_t maximumArchiveNestingDepth = 8;
  constexpr std::uint64_t maximumCompressionRatio = 64;

  std::filesystem::path fuzzZipPath()
  {
    return std::filesystem::temp_directory_path() / "uburu-zip-archive-reader-fuzzer.zip";
  }

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
  if (data == nullptr || size == 0)
    return 0;

  const auto contentSize = std::min(size, maximumInputBytes);
  const std::string_view content(reinterpret_cast<const char*>(data), contentSize);
  const auto path = fuzzZipPath();

  {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
  }

  const uburu::text::RichFormatSafetyLimits limits{
    .maximumExpandedArchiveBytes = maximumExpandedArchiveBytes,
    .maximumSingleExpandedEntryBytes = maximumSingleExpandedEntryBytes,
    .maximumArchiveEntries = maximumArchiveEntries,
    .maximumArchiveNestingDepth = maximumArchiveNestingDepth,
    .maximumCompressionRatio = maximumCompressionRatio,
  };
  const uburu::archive::ZipArchiveReader reader;
  const auto catalog = reader.readCatalog(path, limits);

  if (catalog.status == uburu::archive::ZipArchiveReadStatus::completed) {
    const auto entriesToRead = std::min(catalog.entries.size(), maximumEntriesToRead);

    for (std::size_t index = 0; index < entriesToRead; ++index) {
      const auto result = reader.readEntry(path, catalog.entries[index], limits);
      static_cast<void>(result.bytes.size());
    }
  }

  std::filesystem::remove(path);

  return 0;
}
