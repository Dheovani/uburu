#include "core/archive/zip-archive-reader.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include <zlib.h>

namespace uburu::archive
{
  namespace
  {

    constexpr std::uint32_t localFileHeaderSignature = 0x0403'4B50U;
    constexpr std::uint32_t centralDirectoryFileHeaderSignature = 0x0201'4B50U;
    constexpr std::uint32_t endOfCentralDirectorySignature = 0x0605'4B50U;
    constexpr std::uint16_t zip64Marker16 = 0xFFFFU;
    constexpr std::uint32_t zip64Marker32 = 0xFFFF'FFFFU;
    constexpr std::size_t endOfCentralDirectoryMinimumBytes = 22;
    constexpr std::size_t maximumEndOfCentralDirectoryCommentBytes = 65'535;
    constexpr std::size_t centralDirectoryFixedHeaderBytes = 46;
    constexpr std::size_t centralDirectoryNameLengthOffset = 28;
    constexpr std::size_t centralDirectoryExtraLengthOffset = 30;
    constexpr std::size_t centralDirectoryCommentLengthOffset = 32;
    constexpr std::size_t centralDirectoryCompressedSizeOffset = 20;
    constexpr std::size_t centralDirectoryExpandedSizeOffset = 24;
    constexpr std::size_t centralDirectoryCompressionMethodOffset = 10;
    constexpr std::size_t centralDirectoryLocalHeaderOffset = 42;
    constexpr std::size_t localFileHeaderFixedBytes = 30;
    constexpr std::size_t localFileHeaderNameLengthOffset = 26;
    constexpr std::size_t localFileHeaderExtraLengthOffset = 28;
    constexpr std::size_t eocdEntryCountOffset = 10;
    constexpr std::size_t eocdCentralDirectorySizeOffset = 12;
    constexpr std::size_t eocdCentralDirectoryOffsetOffset = 16;
    constexpr char zipDirectorySeparator = '/';
    constexpr char windowsDirectorySeparator = '\\';
    constexpr char driveLetterSeparator = ':';
    constexpr char nulCharacter = '\0';
    constexpr std::uint16_t storeCompressionMethod = 0;
    constexpr std::uint16_t deflateCompressionMethod = 8;
    constexpr int zlibRawDeflateWindowBits = -MAX_WBITS;

    struct CentralDirectoryLocation
    {
      std::uint64_t offset{0};
      std::uint64_t size{0};
      std::size_t entryCount{0};
    };

    class InflateStream
    {
    public:
      InflateStream()
      {
        initialized = inflateInit2(&stream, zlibRawDeflateWindowBits) == Z_OK;
      }

      ~InflateStream()
      {
        if (initialized)
          inflateEnd(&stream);
      }

      InflateStream(const InflateStream&) = delete;
      InflateStream& operator=(const InflateStream&) = delete;
      InflateStream(InflateStream&&) noexcept = delete;
      InflateStream& operator=(InflateStream&&) noexcept = delete;

      [[nodiscard]]
      bool isInitialized() const
      {
        return initialized;
      }

      [[nodiscard]]
      z_stream& value()
      {
        return stream;
      }

    private:
      z_stream stream{};
      bool initialized{false};
    };

    [[nodiscard]]
    std::uint16_t littleEndian16(std::span<const unsigned char> bytes, std::size_t offset)
    {
      return static_cast<std::uint16_t>(bytes[offset]) |
             static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
    }

    [[nodiscard]]
    std::uint32_t littleEndian32(std::span<const unsigned char> bytes, std::size_t offset)
    {
      return (static_cast<std::uint32_t>(bytes[offset])) |
             (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
             (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
             (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
    }

    [[nodiscard]]
    bool isZip64Marker(const CentralDirectoryLocation& location)
    {
      return location.entryCount == zip64Marker16
          || location.offset == zip64Marker32
          || location.size == zip64Marker32;
    }

    [[nodiscard]]
    bool isSupportedCompressionMethod(std::uint16_t method)
    {
      return method == storeCompressionMethod
          || method == deflateCompressionMethod;
    }

    [[nodiscard]]
    bool hasUnsafeSegment(std::string_view name)
    {
      std::size_t segmentStart = 0;

      while (segmentStart <= name.size()) {
        const auto segmentEnd = name.find(zipDirectorySeparator, segmentStart);
        const auto segment =
          name.substr(segmentStart, segmentEnd == std::string_view::npos ? name.size() : segmentEnd);

        if (segment == "." || segment == "..")
          return true;

        if (segmentEnd == std::string_view::npos)
          break;

        segmentStart = segmentEnd + 1;
      }

      return false;
    }

    [[nodiscard]]
    bool isUnsafeEntryName(std::string_view name)
    {
      if (name.empty())
        return true;

      if (name.front() == zipDirectorySeparator)
        return true;

      if (name.find(windowsDirectorySeparator) != std::string_view::npos)
        return true;

      if (name.find(nulCharacter) != std::string_view::npos)
        return true;

      if (name.size() >= 2 && name[1] == driveLetterSeparator)
        return true;

      return hasUnsafeSegment(name);
    }

    [[nodiscard]]
    std::filesystem::path normalizeZipEntryPath(std::string_view name)
    {
      std::filesystem::path normalized;
      std::size_t segmentStart = 0;

      while (segmentStart < name.size()) {
        const auto segmentEnd = name.find(zipDirectorySeparator, segmentStart);
        const auto segment =
          name.substr(segmentStart, segmentEnd == std::string_view::npos ? name.size() : segmentEnd);

        if (!segment.empty())
          normalized /= std::string{segment};

        if (segmentEnd == std::string_view::npos)
          break;

        segmentStart = segmentEnd + 1;
      }

      return normalized;
    }

    [[nodiscard]]
    bool checkedAdd(std::uint64_t left, std::uint64_t right, std::uint64_t& result)
    {
      if (left > std::numeric_limits<std::uint64_t>::max() - right)
        return false;

      result = left + right;

      return true;
    }

    [[nodiscard]]
    ZipArchiveCatalog failedCatalog(
      ZipArchiveReadStatus status,
      std::error_code error = {},
      text::RichFormatSafetyStatus safetyStatus = text::RichFormatSafetyStatus::accepted)
    {
      return ZipArchiveCatalog{.status = status, .safetyStatus = safetyStatus, .error = error, .entries = {}};
    }

    [[nodiscard]]
    ZipEntryReadResult failedEntryRead(
      ZipArchiveReadStatus status,
      std::error_code error = {},
      text::RichFormatSafetyStatus safetyStatus = text::RichFormatSafetyStatus::accepted)
    {
      return ZipEntryReadResult{.status = status, .safetyStatus = safetyStatus, .error = error, .bytes = {}};
    }

    [[nodiscard]]
    std::vector<unsigned char> readTail(std::ifstream& stream, std::uint64_t fileSize, std::error_code& error)
    {
      const auto tailBytes =
        std::min<std::uint64_t>(fileSize, maximumEndOfCentralDirectoryCommentBytes + endOfCentralDirectoryMinimumBytes);

      std::vector<unsigned char> tail(static_cast<std::size_t>(tailBytes));

      stream.seekg(static_cast<std::streamoff>(fileSize - tailBytes), std::ios::beg);

      if (!stream) {
        error = std::make_error_code(std::errc::io_error);

        return {};
      }

      stream.read(reinterpret_cast<char*>(tail.data()), static_cast<std::streamsize>(tail.size()));

      if (!stream) {
        error = std::make_error_code(std::errc::io_error);

        return {};
      }

      return tail;
    }

    [[nodiscard]]
    std::optional<std::size_t> findEndOfCentralDirectory(std::span<const unsigned char> tail)
    {
      if (tail.size() < endOfCentralDirectoryMinimumBytes)
        return std::nullopt;

      for (auto offset = tail.size() - endOfCentralDirectoryMinimumBytes;; --offset) {
        if (littleEndian32(tail, offset) == endOfCentralDirectorySignature)
          return offset;

        if (offset == 0)
          break;
      }

      return std::nullopt;
    }

    [[nodiscard]]
    std::optional<CentralDirectoryLocation> readCentralDirectoryLocation(
      std::span<const unsigned char> tail,
      std::size_t eocdOffset)
    {
      if (tail.size() - eocdOffset < endOfCentralDirectoryMinimumBytes)
        return std::nullopt;

      return CentralDirectoryLocation{.offset = littleEndian32(tail, eocdOffset + eocdCentralDirectoryOffsetOffset),
                                      .size = littleEndian32(tail, eocdOffset + eocdCentralDirectorySizeOffset),
                                      .entryCount = littleEndian16(tail, eocdOffset + eocdEntryCountOffset)};
    }

    [[nodiscard]]
    std::vector<unsigned char> readCentralDirectory(
      std::ifstream& stream,
      CentralDirectoryLocation location,
      std::uint64_t fileSize,
      std::error_code& error)
    {
      std::uint64_t centralDirectoryEnd = 0;

      if (!checkedAdd(location.offset, location.size, centralDirectoryEnd) || centralDirectoryEnd > fileSize) {
        error = std::make_error_code(std::errc::invalid_argument);

        return {};
      }

      if (location.size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        error = std::make_error_code(std::errc::file_too_large);

        return {};
      }

      std::vector<unsigned char> centralDirectory(static_cast<std::size_t>(location.size));

      stream.seekg(static_cast<std::streamoff>(location.offset), std::ios::beg);

      if (!stream) {
        error = std::make_error_code(std::errc::io_error);

        return {};
      }

      stream.read(
        reinterpret_cast<char*>(centralDirectory.data()),
        static_cast<std::streamsize>(centralDirectory.size()));

      if (!stream) {
        error = std::make_error_code(std::errc::io_error);

        return {};
      }

      return centralDirectory;
    }

    [[nodiscard]]
    std::vector<unsigned char> readBytes(
      std::ifstream& stream,
      std::uint64_t offset,
      std::uint64_t size,
      std::error_code& error)
    {
      if (size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        error = std::make_error_code(std::errc::file_too_large);

        return {};
      }

      std::vector<unsigned char> bytes(static_cast<std::size_t>(size));

      stream.seekg(static_cast<std::streamoff>(offset), std::ios::beg);

      if (!stream) {
        error = std::make_error_code(std::errc::io_error);

        return {};
      }

      stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

      if (!stream) {
        error = std::make_error_code(std::errc::io_error);

        return {};
      }

      return bytes;
    }

    [[nodiscard]]
    std::optional<std::uint64_t> readEntryPayloadOffset(
      std::ifstream& stream,
      const ZipArchiveEntry& entry,
      std::error_code& error)
    {
      const auto localHeader = readBytes(stream, entry.localHeaderOffset, localFileHeaderFixedBytes, error);

      if (error)
        return std::nullopt;

      if (littleEndian32(localHeader, 0) != localFileHeaderSignature) {
        error = std::make_error_code(std::errc::invalid_argument);

        return std::nullopt;
      }

      const auto nameLength = littleEndian16(localHeader, localFileHeaderNameLengthOffset);
      const auto extraLength = littleEndian16(localHeader, localFileHeaderExtraLengthOffset);
      const auto payloadOffset = entry.localHeaderOffset + localFileHeaderFixedBytes + nameLength + extraLength;
      const auto localName = readBytes(stream, entry.localHeaderOffset + localFileHeaderFixedBytes, nameLength, error);

      if (error)
        return std::nullopt;

      const std::string rawLocalName(
        reinterpret_cast<const char*>(localName.data()),
        localName.size());

      if (rawLocalName != entry.rawName) {
        error = std::make_error_code(std::errc::invalid_argument);

        return std::nullopt;
      }

      return payloadOffset;
    }

    [[nodiscard]]
    ZipEntryReadResult inflateRawDeflatePayload(std::span<const unsigned char> compressed, std::uint64_t expandedBytes)
    {
      if (expandedBytes == 0)
        return ZipEntryReadResult{.bytes = {}};

      if (expandedBytes > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
        return failedEntryRead(ZipArchiveReadStatus::safetyLimitExceeded);

      if (expandedBytes > static_cast<std::uint64_t>(std::numeric_limits<uInt>::max()))
        return failedEntryRead(ZipArchiveReadStatus::safetyLimitExceeded);

      if (compressed.size() > static_cast<std::size_t>(std::numeric_limits<uInt>::max()))
        return failedEntryRead(ZipArchiveReadStatus::safetyLimitExceeded);

      std::vector<unsigned char> output(static_cast<std::size_t>(expandedBytes));
      InflateStream inflateStream;

      if (!inflateStream.isInitialized())
        return failedEntryRead(ZipArchiveReadStatus::decompressionFailed);

      auto& stream = inflateStream.value();
      stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(compressed.data()));
      stream.avail_in = static_cast<uInt>(compressed.size());
      stream.next_out = reinterpret_cast<Bytef*>(output.data());
      stream.avail_out = static_cast<uInt>(output.size());

      const auto result = inflate(&stream, Z_FINISH);

      if (result != Z_STREAM_END || stream.total_out != output.size())
        return failedEntryRead(ZipArchiveReadStatus::decompressionFailed);

      return ZipEntryReadResult{.bytes = std::move(output)};
    }

    [[nodiscard]]
    ZipArchiveCatalog parseCentralDirectory(
      std::span<const unsigned char> centralDirectory,
      CentralDirectoryLocation location,
      text::RichFormatSafetyLimits limits,
      std::stop_token stopToken)
    {
      ZipArchiveCatalog catalog;
      catalog.entries.reserve(location.entryCount);
      std::uint64_t totalExpandedBytes = 0;
      std::size_t offset = 0;

      for (std::size_t entryIndex = 0; entryIndex < location.entryCount; ++entryIndex) {
        if (stopToken.stop_requested())
          return failedCatalog(ZipArchiveReadStatus::cancelled);

        if (offset > centralDirectory.size() || centralDirectory.size() - offset < centralDirectoryFixedHeaderBytes)
          return failedCatalog(ZipArchiveReadStatus::invalidArchive);

        if (littleEndian32(centralDirectory, offset) != centralDirectoryFileHeaderSignature)
          return failedCatalog(ZipArchiveReadStatus::invalidArchive);

        const auto compressedBytes = littleEndian32(centralDirectory, offset + centralDirectoryCompressedSizeOffset);
        const auto expandedBytes = littleEndian32(centralDirectory, offset + centralDirectoryExpandedSizeOffset);
        const auto nameLength = littleEndian16(centralDirectory, offset + centralDirectoryNameLengthOffset);
        const auto extraLength = littleEndian16(centralDirectory, offset + centralDirectoryExtraLengthOffset);
        const auto commentLength = littleEndian16(centralDirectory, offset + centralDirectoryCommentLengthOffset);
        const auto compressionMethod =
          littleEndian16(centralDirectory, offset + centralDirectoryCompressionMethodOffset);
        const auto localHeaderOffset = littleEndian32(centralDirectory, offset + centralDirectoryLocalHeaderOffset);
        const auto variableBytes = static_cast<std::size_t>(nameLength) + extraLength + commentLength;
        const auto entryBytes = centralDirectoryFixedHeaderBytes + variableBytes;

        if (compressedBytes == zip64Marker32 ||
            expandedBytes == zip64Marker32 ||
            localHeaderOffset == zip64Marker32)
          return failedCatalog(ZipArchiveReadStatus::unsupportedZip64);

        if (offset > centralDirectory.size() || centralDirectory.size() - offset < entryBytes)
          return failedCatalog(ZipArchiveReadStatus::invalidArchive);

        const auto rawNameOffset = offset + centralDirectoryFixedHeaderBytes;
        const std::string rawName(
          reinterpret_cast<const char*>(centralDirectory.data() + rawNameOffset),
          static_cast<std::size_t>(nameLength));

        if (isUnsafeEntryName(rawName))
          return failedCatalog(ZipArchiveReadStatus::unsafeEntryName);

        const auto directory = rawName.back() == zipDirectorySeparator;

        if (!directory && !isSupportedCompressionMethod(compressionMethod))
          return failedCatalog(ZipArchiveReadStatus::unsupportedCompressionMethod);

        const auto entrySafety = text::validateArchiveEntrySafety(
          {.compressedBytes = compressedBytes, .expandedBytes = expandedBytes},
          limits);

        if (entrySafety != text::RichFormatSafetyStatus::accepted)
          return failedCatalog(ZipArchiveReadStatus::safetyLimitExceeded, {}, entrySafety);

        if (!checkedAdd(totalExpandedBytes, expandedBytes, totalExpandedBytes))
          return failedCatalog(
            ZipArchiveReadStatus::safetyLimitExceeded,
            {},
            text::RichFormatSafetyStatus::totalExpandedBytesExceeded);

        catalog.entries.push_back(ZipArchiveEntry{.rawName = rawName,
                                                  .normalizedPath = normalizeZipEntryPath(rawName),
                                                  .compressionMethod = compressionMethod,
                                                  .localHeaderOffset = localHeaderOffset,
                                                  .compressedBytes = compressedBytes,
                                                  .expandedBytes = expandedBytes,
                                                  .directory = directory});

        offset += entryBytes;
      }

      if (offset != centralDirectory.size())
        return failedCatalog(ZipArchiveReadStatus::invalidArchive);

      const auto archiveSafety = text::validateArchiveSafety(
        {.totalExpandedBytes = totalExpandedBytes, .entryCount = location.entryCount, .nestingDepth = 1},
        limits);

      if (archiveSafety != text::RichFormatSafetyStatus::accepted)
        return failedCatalog(ZipArchiveReadStatus::safetyLimitExceeded, {}, archiveSafety);

      return catalog;
    }

  } // namespace

  ZipArchiveCatalog ZipArchiveReader::readCatalog(
    const std::filesystem::path& path,
    text::RichFormatSafetyLimits limits,
    std::stop_token stopToken) const
  {
    if (stopToken.stop_requested())
      return failedCatalog(ZipArchiveReadStatus::cancelled);

    std::ifstream stream(path, std::ios::binary | std::ios::ate);

    if (!stream)
      return failedCatalog(ZipArchiveReadStatus::openFailed,
                           std::make_error_code(std::errc::no_such_file_or_directory));

    const auto fileSizePosition = stream.tellg();

    if (fileSizePosition < static_cast<std::streampos>(endOfCentralDirectoryMinimumBytes))
      return failedCatalog(ZipArchiveReadStatus::invalidArchive);

    const auto fileSize = static_cast<std::uint64_t>(fileSizePosition);
    std::error_code readError;
    const auto tail = readTail(stream, fileSize, readError);

    if (readError)
      return failedCatalog(ZipArchiveReadStatus::readFailed, readError);

    const auto eocdOffset = findEndOfCentralDirectory(tail);

    if (!eocdOffset)
      return failedCatalog(ZipArchiveReadStatus::invalidArchive);

    const auto location = readCentralDirectoryLocation(tail, *eocdOffset);

    if (!location)
      return failedCatalog(ZipArchiveReadStatus::invalidArchive);

    if (isZip64Marker(*location))
      return failedCatalog(ZipArchiveReadStatus::unsupportedZip64);

    const auto centralDirectory = readCentralDirectory(stream, *location, fileSize, readError);

    if (readError) {
      if (readError == std::make_error_code(std::errc::invalid_argument))
        return failedCatalog(ZipArchiveReadStatus::invalidArchive, readError);

      return failedCatalog(ZipArchiveReadStatus::readFailed, readError);
    }

    return parseCentralDirectory(centralDirectory, *location, limits, stopToken);
  }

  ZipEntryReadResult ZipArchiveReader::readEntry(
    const std::filesystem::path& path,
    const ZipArchiveEntry& entry,
    text::RichFormatSafetyLimits limits,
    std::stop_token stopToken) const
  {
    if (stopToken.stop_requested())
      return failedEntryRead(ZipArchiveReadStatus::cancelled);

    if (entry.directory)
      return failedEntryRead(ZipArchiveReadStatus::entryNotFound);

    if (!isSupportedCompressionMethod(entry.compressionMethod))
      return failedEntryRead(ZipArchiveReadStatus::unsupportedCompressionMethod);

    const auto entrySafety = text::validateArchiveEntrySafety(
      {.compressedBytes = entry.compressedBytes, .expandedBytes = entry.expandedBytes},
      limits);

    if (entrySafety != text::RichFormatSafetyStatus::accepted)
      return failedEntryRead(ZipArchiveReadStatus::safetyLimitExceeded, {}, entrySafety);

    std::ifstream stream(path, std::ios::binary);

    if (!stream)
      return failedEntryRead(
        ZipArchiveReadStatus::openFailed,
        std::make_error_code(std::errc::no_such_file_or_directory));

    std::error_code readError;
    const auto payloadOffset = readEntryPayloadOffset(stream, entry, readError);

    if (readError) {
      if (readError == std::make_error_code(std::errc::invalid_argument))
        return failedEntryRead(ZipArchiveReadStatus::invalidArchive, readError);

      return failedEntryRead(ZipArchiveReadStatus::readFailed, readError);
    }

    if (!payloadOffset)
      return failedEntryRead(ZipArchiveReadStatus::invalidArchive);

    if (stopToken.stop_requested())
      return failedEntryRead(ZipArchiveReadStatus::cancelled);

    auto payload = readBytes(stream, *payloadOffset, entry.compressedBytes, readError);

    if (readError)
      return failedEntryRead(ZipArchiveReadStatus::readFailed, readError);

    if (entry.compressionMethod == storeCompressionMethod) {
      if (entry.compressedBytes != entry.expandedBytes)
        return failedEntryRead(ZipArchiveReadStatus::invalidArchive);

      return ZipEntryReadResult{.bytes = std::move(payload)};
    }

    return inflateRawDeflatePayload(payload, entry.expandedBytes);
  }

  std::string_view zipArchiveReadStatusName(ZipArchiveReadStatus status)
  {
    switch (status) {
    case ZipArchiveReadStatus::completed:
      return "completed";
    case ZipArchiveReadStatus::cancelled:
      return "cancelled";
    case ZipArchiveReadStatus::openFailed:
      return "openFailed";
    case ZipArchiveReadStatus::readFailed:
      return "readFailed";
    case ZipArchiveReadStatus::invalidArchive:
      return "invalidArchive";
    case ZipArchiveReadStatus::unsupportedZip64:
      return "unsupportedZip64";
    case ZipArchiveReadStatus::unsupportedCompressionMethod:
      return "unsupportedCompressionMethod";
    case ZipArchiveReadStatus::entryNotFound:
      return "entryNotFound";
    case ZipArchiveReadStatus::unsafeEntryName:
      return "unsafeEntryName";
    case ZipArchiveReadStatus::safetyLimitExceeded:
      return "safetyLimitExceeded";
    case ZipArchiveReadStatus::decompressionFailed:
      return "decompressionFailed";
    }

    return "unknown";
  }

} // namespace uburu::archive
