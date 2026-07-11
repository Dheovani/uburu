#include "core/archive/zip-archive-reader.hpp"

#include "helpers/temporary-paths.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string_view>
#include <vector>

namespace
{

  constexpr std::uint32_t localFileHeaderSignature = 0x0403'4B50U;
  constexpr std::uint32_t centralDirectoryFileHeaderSignature = 0x0201'4B50U;
  constexpr std::uint32_t endOfCentralDirectorySignature = 0x0605'4B50U;
  constexpr std::uint16_t zipVersionNeeded = 20;
  constexpr std::uint16_t storeCompressionMethod = 0;
  constexpr std::uint16_t deflateCompressionMethod = 8;
  constexpr std::uint16_t unsupportedCompressionMethod = 99;
  constexpr std::uint16_t noFlags = 0;
  constexpr std::uint16_t noTimestamp = 0;
  constexpr std::uint32_t noCrc = 0;
  constexpr std::uint16_t noDisk = 0;
  constexpr std::uint16_t noAttributes = 0;
  constexpr std::uint32_t noExternalAttributes = 0;
  constexpr std::size_t endOfCentralDirectoryMinimumBytes = 22;
  constexpr std::size_t eocdEntryCountOffset = 10;
  constexpr std::uint16_t zip64Marker16 = 0xFFFFU;

  struct TestZipEntry
  {
    std::string_view name;
    std::uint32_t compressedBytes{0};
    std::uint32_t expandedBytes{0};
    std::uint16_t compressionMethod{deflateCompressionMethod};
    std::vector<unsigned char> payload;
  };

  void appendLittleEndian16(std::vector<unsigned char>& bytes, std::uint16_t value)
  {
    bytes.push_back(static_cast<unsigned char>(value & 0xFFU));
    bytes.push_back(static_cast<unsigned char>((value >> 8U) & 0xFFU));
  }

  void appendLittleEndian32(std::vector<unsigned char>& bytes, std::uint32_t value)
  {
    bytes.push_back(static_cast<unsigned char>(value & 0xFFU));
    bytes.push_back(static_cast<unsigned char>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<unsigned char>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<unsigned char>((value >> 24U) & 0xFFU));
  }

  void appendText(std::vector<unsigned char>& bytes, std::string_view text)
  {
    for (const auto character : text)
      bytes.push_back(static_cast<unsigned char>(character));
  }

  void overwriteLittleEndian16(std::vector<unsigned char>& bytes, std::size_t offset, std::uint16_t value)
  {
    bytes[offset] = static_cast<unsigned char>(value & 0xFFU);
    bytes[offset + 1] = static_cast<unsigned char>((value >> 8U) & 0xFFU);
  }

  [[nodiscard]]
  std::uint32_t effectiveCompressedBytes(const TestZipEntry& entry)
  {
    if (!entry.payload.empty())
      return static_cast<std::uint32_t>(entry.payload.size());

    return entry.compressedBytes;
  }

  void appendPayload(std::vector<unsigned char>& bytes, const TestZipEntry& entry)
  {
    if (!entry.payload.empty()) {
      bytes.insert(bytes.end(), entry.payload.begin(), entry.payload.end());

      return;
    }

    bytes.insert(bytes.end(), static_cast<std::size_t>(entry.compressedBytes), 0);
  }

  [[nodiscard]]
  std::vector<unsigned char> makeZip(std::vector<TestZipEntry> entries)
  {
    std::vector<unsigned char> bytes;
    std::vector<std::uint32_t> localOffsets;

    for (const auto& entry : entries) {
      localOffsets.push_back(static_cast<std::uint32_t>(bytes.size()));
      appendLittleEndian32(bytes, localFileHeaderSignature);
      appendLittleEndian16(bytes, zipVersionNeeded);
      appendLittleEndian16(bytes, noFlags);
      appendLittleEndian16(bytes, entry.compressionMethod);
      appendLittleEndian16(bytes, noTimestamp);
      appendLittleEndian16(bytes, noTimestamp);
      appendLittleEndian32(bytes, noCrc);
      appendLittleEndian32(bytes, effectiveCompressedBytes(entry));
      appendLittleEndian32(bytes, entry.expandedBytes);
      appendLittleEndian16(bytes, static_cast<std::uint16_t>(entry.name.size()));
      appendLittleEndian16(bytes, 0);
      appendText(bytes, entry.name);
      appendPayload(bytes, entry);
    }

    const auto centralDirectoryOffset = static_cast<std::uint32_t>(bytes.size());

    for (std::size_t index = 0; index < entries.size(); ++index) {
      const auto& entry = entries[index];
      appendLittleEndian32(bytes, centralDirectoryFileHeaderSignature);
      appendLittleEndian16(bytes, zipVersionNeeded);
      appendLittleEndian16(bytes, zipVersionNeeded);
      appendLittleEndian16(bytes, noFlags);
      appendLittleEndian16(bytes, entry.compressionMethod);
      appendLittleEndian16(bytes, noTimestamp);
      appendLittleEndian16(bytes, noTimestamp);
      appendLittleEndian32(bytes, noCrc);
      appendLittleEndian32(bytes, effectiveCompressedBytes(entry));
      appendLittleEndian32(bytes, entry.expandedBytes);
      appendLittleEndian16(bytes, static_cast<std::uint16_t>(entry.name.size()));
      appendLittleEndian16(bytes, 0);
      appendLittleEndian16(bytes, 0);
      appendLittleEndian16(bytes, noDisk);
      appendLittleEndian16(bytes, noAttributes);
      appendLittleEndian32(bytes, noExternalAttributes);
      appendLittleEndian32(bytes, localOffsets[index]);
      appendText(bytes, entry.name);
    }

    const auto centralDirectorySize = static_cast<std::uint32_t>(bytes.size() - centralDirectoryOffset);
    appendLittleEndian32(bytes, endOfCentralDirectorySignature);
    appendLittleEndian16(bytes, noDisk);
    appendLittleEndian16(bytes, noDisk);
    appendLittleEndian16(bytes, static_cast<std::uint16_t>(entries.size()));
    appendLittleEndian16(bytes, static_cast<std::uint16_t>(entries.size()));
    appendLittleEndian32(bytes, centralDirectorySize);
    appendLittleEndian32(bytes, centralDirectoryOffset);
    appendLittleEndian16(bytes, 0);

    return bytes;
  }

} // namespace

TEST_CASE("zip archive reader lists safe entries without extracting payloads")
{
  uburu::tests::TemporaryFile file("uburu-zip-catalog.zip");
  uburu::tests::writeBytes(
    file.path(),
    makeZip(
      {{.name = "word/document.xml", .compressedBytes = 12, .expandedBytes = 24},
       {.name = "word/_rels/document.xml.rels", .compressedBytes = 6, .expandedBytes = 8},
       {.name = "empty/", .compressedBytes = 0, .expandedBytes = 0, .compressionMethod = storeCompressionMethod}}));

  const uburu::archive::ZipArchiveReader reader;
  const auto catalog = reader.readCatalog(file.path());

  REQUIRE(catalog.status == uburu::archive::ZipArchiveReadStatus::completed);
  REQUIRE(catalog.entries.size() == 3);
  CHECK(catalog.entries[0].rawName == "word/document.xml");
  CHECK(catalog.entries[0].normalizedPath.generic_string() == "word/document.xml");
  CHECK(catalog.entries[0].compressedBytes == 12);
  CHECK(catalog.entries[0].expandedBytes == 24);
  CHECK_FALSE(catalog.entries[0].directory);
  CHECK(catalog.entries[2].directory);
}

TEST_CASE("zip archive reader reads stored entry payloads")
{
  uburu::tests::TemporaryFile file("uburu-zip-stored-entry.zip");
  uburu::tests::writeBytes(
    file.path(),
    makeZip({{.name = "word/document.xml",
              .compressedBytes = 5,
              .expandedBytes = 5,
              .compressionMethod = storeCompressionMethod,
              .payload = {'h', 'e', 'l', 'l', 'o'}}}));

  const uburu::archive::ZipArchiveReader reader;
  const auto catalog = reader.readCatalog(file.path());

  REQUIRE(catalog.status == uburu::archive::ZipArchiveReadStatus::completed);
  REQUIRE(catalog.entries.size() == 1);

  const auto result = reader.readEntry(file.path(), catalog.entries.front());

  REQUIRE(result.status == uburu::archive::ZipArchiveReadStatus::completed);
  CHECK(result.bytes == std::vector<unsigned char>{'h', 'e', 'l', 'l', 'o'});
}

TEST_CASE("zip archive reader inflates raw deflate entry payloads")
{
  uburu::tests::TemporaryFile file("uburu-zip-deflate-entry.zip");
  uburu::tests::writeBytes(
    file.path(),
    makeZip({{.name = "word/document.xml",
              .expandedBytes = 5,
              .payload = {0xCBU, 0x48U, 0xCDU, 0xC9U, 0xC9U, 0x07U, 0x00U}}}));

  const uburu::archive::ZipArchiveReader reader;
  const auto catalog = reader.readCatalog(file.path());

  REQUIRE(catalog.status == uburu::archive::ZipArchiveReadStatus::completed);
  REQUIRE(catalog.entries.size() == 1);

  const auto result = reader.readEntry(file.path(), catalog.entries.front());

  REQUIRE(result.status == uburu::archive::ZipArchiveReadStatus::completed);
  CHECK(result.bytes == std::vector<unsigned char>{'h', 'e', 'l', 'l', 'o'});
}

TEST_CASE("zip archive reader rejects unsafe entry names")
{
  uburu::tests::TemporaryFile file("uburu-zip-unsafe-name.zip");
  uburu::tests::writeBytes(file.path(), makeZip({{.name = "../evil.xml", .compressedBytes = 1, .expandedBytes = 1}}));

  const uburu::archive::ZipArchiveReader reader;
  const auto catalog = reader.readCatalog(file.path());

  CHECK(catalog.status == uburu::archive::ZipArchiveReadStatus::unsafeEntryName);
}

TEST_CASE("zip archive reader rejects zip64 metadata until it is explicitly supported")
{
  uburu::tests::TemporaryFile file("uburu-zip64-marker.zip");
  auto bytes = makeZip({{.name = "word/document.xml", .compressedBytes = 1, .expandedBytes = 1}});
  const auto eocdOffset = bytes.size() - endOfCentralDirectoryMinimumBytes;
  overwriteLittleEndian16(bytes, eocdOffset + eocdEntryCountOffset, zip64Marker16);
  uburu::tests::writeBytes(file.path(), bytes);

  const uburu::archive::ZipArchiveReader reader;
  const auto catalog = reader.readCatalog(file.path());

  CHECK(catalog.status == uburu::archive::ZipArchiveReadStatus::unsupportedZip64);
}

TEST_CASE("zip archive reader rejects unsupported compression methods")
{
  uburu::tests::TemporaryFile file("uburu-zip-unsupported-compression.zip");
  uburu::tests::writeBytes(
    file.path(),
    makeZip({{.name = "word/document.xml",
              .compressedBytes = 1,
              .expandedBytes = 1,
              .compressionMethod = unsupportedCompressionMethod}}));

  const uburu::archive::ZipArchiveReader reader;
  const auto catalog = reader.readCatalog(file.path());

  CHECK(catalog.status == uburu::archive::ZipArchiveReadStatus::unsupportedCompressionMethod);
}

TEST_CASE("zip archive reader rejects archive safety limit violations")
{
  uburu::tests::TemporaryFile file("uburu-zip-safety.zip");
  uburu::tests::writeBytes(file.path(),
                           makeZip({{.name = "word/document.xml", .compressedBytes = 1, .expandedBytes = 200}}));

  const uburu::archive::ZipArchiveReader reader;
  const uburu::text::RichFormatSafetyLimits limits{.maximumExpandedArchiveBytes = 1'000,
                                                   .maximumSingleExpandedEntryBytes = 500,
                                                   .maximumArchiveEntries = 10,
                                                   .maximumArchiveNestingDepth = 4,
                                                   .maximumCompressionRatio = 100};
  const auto catalog = reader.readCatalog(file.path(), limits);

  CHECK(catalog.status == uburu::archive::ZipArchiveReadStatus::safetyLimitExceeded);
  CHECK(catalog.safetyStatus == uburu::text::RichFormatSafetyStatus::compressionRatioExceeded);
}

TEST_CASE("zip archive reader rejects excessive nested package paths")
{
  uburu::tests::TemporaryFile file("uburu-zip-nested-paths.zip");
  uburu::tests::writeBytes(
    file.path(),
    makeZip({{.name = "a/b/c/d/document.xml", .compressedBytes = 1, .expandedBytes = 1}}));

  const uburu::archive::ZipArchiveReader reader;
  const uburu::text::RichFormatSafetyLimits limits{
    .maximumExpandedArchiveBytes = 1'000,
    .maximumSingleExpandedEntryBytes = 500,
    .maximumArchiveEntries = 10,
    .maximumArchiveNestingDepth = 3,
    .maximumCompressionRatio = 100,
  };
  const auto catalog = reader.readCatalog(file.path(), limits);

  CHECK(catalog.status == uburu::archive::ZipArchiveReadStatus::safetyLimitExceeded);
  CHECK(catalog.safetyStatus == uburu::text::RichFormatSafetyStatus::nestingDepthExceeded);
}

TEST_CASE("zip archive reader reports invalid archives")
{
  uburu::tests::TemporaryFile file("uburu-invalid-zip.zip");
  uburu::tests::writeBytes(file.path(), {0x55U, 0x62U, 0x75U, 0x72U, 0x75U});

  const uburu::archive::ZipArchiveReader reader;
  const auto catalog = reader.readCatalog(file.path());

  CHECK(catalog.status == uburu::archive::ZipArchiveReadStatus::invalidArchive);
}

TEST_CASE("zip archive reader exposes stable status names")
{
  CHECK(uburu::archive::zipArchiveReadStatusName(uburu::archive::ZipArchiveReadStatus::completed) == "completed");
  CHECK(uburu::archive::zipArchiveReadStatusName(uburu::archive::ZipArchiveReadStatus::decompressionFailed) ==
        "decompressionFailed");
  CHECK(uburu::archive::zipArchiveReadStatusName(uburu::archive::ZipArchiveReadStatus::unsupportedCompressionMethod) ==
        "unsupportedCompressionMethod");
  CHECK(uburu::archive::zipArchiveReadStatusName(uburu::archive::ZipArchiveReadStatus::unsafeEntryName) ==
        "unsafeEntryName");
}
