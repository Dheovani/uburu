#include "core/document/pdf-document-extractor.hpp"

#include "core/document/xml-document-utils.hpp"

#include <zlib.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cctype>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace uburu::document
{
  namespace
  {

    constexpr std::string_view extractorName = "pdf";
    constexpr std::string_view pdfHeaderMarker = "%PDF-";
    constexpr std::string_view encryptedDocumentMarker = "/Encrypt";
    constexpr std::string_view pageTypeMarker = "/Type";
    constexpr std::string_view pageMarker = "/Page";
    constexpr std::string_view pagesMarker = "/Pages";
    constexpr std::string_view contentsMarker = "/Contents";
    constexpr std::string_view fontMarker = "/Font";
    constexpr std::string_view toUnicodeMarker = "/ToUnicode";
    constexpr std::string_view streamMarker = "stream";
    constexpr std::string_view endStreamMarker = "endstream";
    constexpr std::string_view flateDecodeMarker = "/FlateDecode";
    constexpr std::string_view beginBfCharMarker = "beginbfchar";
    constexpr std::string_view endBfCharMarker = "endbfchar";
    constexpr std::string_view beginBfRangeMarker = "beginbfrange";
    constexpr std::string_view endBfRangeMarker = "endbfrange";
    constexpr std::uintmax_t defaultMaximumPdfBytes = 128ULL * 1024ULL * 1024ULL;
    constexpr std::uintmax_t pdfSafetyMultiplier = 16;
    constexpr std::size_t maximumReferenceScanBytes = 2048;
    constexpr std::size_t maximumFontResourceScanBytes = 4096;
    constexpr std::size_t maximumPdfHeaderProbeBytes = 1024;
    constexpr std::size_t maximumOctalDigits = 3;
    constexpr std::size_t utf16CodeUnitBytes = 2;
    constexpr std::size_t pdfReadBufferBytes = 8192;
    constexpr std::size_t zlibOutputBufferBytes = 8192;
    constexpr unsigned char utf16BigEndianBomFirst = 0xFEU;
    constexpr unsigned char utf16BigEndianBomSecond = 0xFFU;
    constexpr char32_t replacementScalar = 0xFFFDU;
    constexpr char32_t highSurrogateFirst = 0xD800U;
    constexpr char32_t highSurrogateLast = 0xDBFFU;
    constexpr char32_t lowSurrogateFirst = 0xDC00U;
    constexpr char32_t lowSurrogateLast = 0xDFFFU;
    constexpr char32_t surrogateBase = 0x10000U;
    constexpr char32_t surrogatePayloadBits = 10U;
    constexpr char32_t surrogatePayloadMask = 0x3FFU;
    constexpr char32_t undefinedSingleByteScalar = replacementScalar;
    constexpr int zlibWindowBits = 15;

    constexpr std::array<char32_t, 32> windows1252ControlScalars{
      0x20ACU,                    // 0x80
      undefinedSingleByteScalar,  // 0x81
      0x201AU,                    // 0x82
      0x0192U,                    // 0x83
      0x201EU,                    // 0x84
      0x2026U,                    // 0x85
      0x2020U,                    // 0x86
      0x2021U,                    // 0x87
      0x02C6U,                    // 0x88
      0x2030U,                    // 0x89
      0x0160U,                    // 0x8A
      0x2039U,                    // 0x8B
      0x0152U,                    // 0x8C
      undefinedSingleByteScalar,  // 0x8D
      0x017DU,                    // 0x8E
      undefinedSingleByteScalar,  // 0x8F
      undefinedSingleByteScalar,  // 0x90
      0x2018U,                    // 0x91
      0x2019U,                    // 0x92
      0x201CU,                    // 0x93
      0x201DU,                    // 0x94
      0x2022U,                    // 0x95
      0x2013U,                    // 0x96
      0x2014U,                    // 0x97
      0x02DCU,                    // 0x98
      0x2122U,                    // 0x99
      0x0161U,                    // 0x9A
      0x203AU,                    // 0x9B
      0x0153U,                    // 0x9C
      undefinedSingleByteScalar,  // 0x9D
      0x017EU,                    // 0x9E
      0x0178U,                    // 0x9F
    };

    struct PdfObject
    {
      int number{0};
      std::string_view body;
    };

    struct PdfStream
    {
      std::string_view dictionary;
      std::string_view bytes;
    };

    struct ToUnicodeMap
    {
      std::map<std::string, std::string> entries;
      std::size_t maximumCodeBytes{0};
    };

    using FontUnicodeMaps = std::map<std::string, ToUnicodeMap>;

    [[nodiscard]]
    bool wouldExceedByteLimit(const DocumentExtractionOptions& options, std::uintmax_t bytes)
    {
      return options.maximumExtractedBytes > 0 && bytes > options.maximumExtractedBytes;
    }

    [[nodiscard]]
    bool wouldExceedSegmentLimit(const DocumentExtractionOptions& options, std::size_t segments)
    {
      return options.maximumSegments > 0 && segments > options.maximumSegments;
    }

    [[nodiscard]]
    std::uintmax_t maximumSourceBytes(const DocumentExtractionOptions& options)
    {
      if (options.maximumExtractedBytes == 0)
        return defaultMaximumPdfBytes;

      if (options.maximumExtractedBytes > std::numeric_limits<std::uintmax_t>::max() / pdfSafetyMultiplier)
        return defaultMaximumPdfBytes;

      return std::max(options.maximumExtractedBytes * pdfSafetyMultiplier, options.maximumExtractedBytes);
    }

    [[nodiscard]]
    bool isAsciiWhitespace(char character)
    {
      return character == '\0' ||
             character == '\t' ||
             character == '\n' ||
             character == '\f' ||
             character == '\r' ||
             character == ' ';
    }

    [[nodiscard]]
    bool isPdfDelimiter(char character)
    {
      return character == '(' ||
             character == ')' ||
             character == '<' ||
             character == '>' ||
             character == '[' ||
             character == ']' ||
             character == '{' ||
             character == '}' ||
             character == '/' ||
             character == '%';
    }

    [[nodiscard]]
    bool hasUtf16BigEndianBom(std::string_view bytes)
    {
      return bytes.size() >= utf16CodeUnitBytes &&
             static_cast<unsigned char>(bytes[0]) == utf16BigEndianBomFirst &&
             static_cast<unsigned char>(bytes[1]) == utf16BigEndianBomSecond;
    }

    void appendUtf8(std::string& output, char32_t scalar)
    {
      if (scalar <= 0x7FU) {
        output.push_back(static_cast<char>(scalar));

        return;
      }

      if (scalar <= 0x7FFU) {
        output.push_back(static_cast<char>(0xC0U | (scalar >> 6U)));
        output.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));

        return;
      }

      if (scalar <= 0xFFFFU) {
        output.push_back(static_cast<char>(0xE0U | (scalar >> 12U)));
        output.push_back(static_cast<char>(0x80U | ((scalar >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));

        return;
      }

      output.push_back(static_cast<char>(0xF0U | (scalar >> 18U)));
      output.push_back(static_cast<char>(0x80U | ((scalar >> 12U) & 0x3FU)));
      output.push_back(static_cast<char>(0x80U | ((scalar >> 6U) & 0x3FU)));
      output.push_back(static_cast<char>(0x80U | (scalar & 0x3FU)));
    }

    [[nodiscard]]
    char32_t utf16Scalar(std::string_view bytes, std::size_t& offset)
    {
      if (offset + 1 >= bytes.size()) {
        offset = bytes.size();

        return replacementScalar;
      }

      auto scalar = (static_cast<char32_t>(static_cast<unsigned char>(bytes[offset])) << 8U) |
                    static_cast<unsigned char>(bytes[offset + 1]);
      offset += utf16CodeUnitBytes;

      if (scalar < highSurrogateFirst || scalar > highSurrogateLast)
        return scalar;

      if (offset + 1 >= bytes.size()) {
        offset = bytes.size();

        return replacementScalar;
      }

      const auto low = (static_cast<char32_t>(static_cast<unsigned char>(bytes[offset])) << 8U) |
                       static_cast<unsigned char>(bytes[offset + 1]);
      offset += utf16CodeUnitBytes;

      if (low < lowSurrogateFirst || low > lowSurrogateLast)
        return replacementScalar;

      return surrogateBase + (((scalar - highSurrogateFirst) & surrogatePayloadMask) << surrogatePayloadBits) +
             ((low - lowSurrogateFirst) & surrogatePayloadMask);
    }

    [[nodiscard]]
    std::string utf16BigEndianToUtf8(std::string_view bytes)
    {
      std::string output;
      std::size_t offset = hasUtf16BigEndianBom(bytes) ? utf16CodeUnitBytes : 0;

      while (offset < bytes.size()) {
        appendUtf8(output, utf16Scalar(bytes, offset));
      }

      return output;
    }

    [[nodiscard]]
    char32_t windows1252Scalar(unsigned char byte)
    {
      if (byte < 0x80U || byte >= 0xA0U)
        return byte;

      return windows1252ControlScalars[byte - 0x80U];
    }

    [[nodiscard]]
    std::string singleBytePdfTextToUtf8(std::string_view bytes)
    {
      std::string output;

      for (const auto byte : bytes) {
        appendUtf8(output, windows1252Scalar(static_cast<unsigned char>(byte)));
      }

      return output;
    }

    void skipWhitespace(std::string_view text, std::size_t& offset)
    {
      while (offset < text.size() && isAsciiWhitespace(text[offset])) {
        ++offset;
      }
    }

    void trimAsciiWhitespace(std::string_view& text)
    {
      while (!text.empty() && isAsciiWhitespace(text.front())) {
        text.remove_prefix(1);
      }

      while (!text.empty() && isAsciiWhitespace(text.back())) {
        text.remove_suffix(1);
      }
    }

    [[nodiscard]]
    bool tokenBoundary(std::string_view text, std::size_t offset)
    {
      if (offset >= text.size())
        return true;

      return isAsciiWhitespace(text[offset]) || isPdfDelimiter(text[offset]);
    }

    [[nodiscard]]
    bool hasTokenAt(std::string_view text, std::size_t offset, std::string_view token)
    {
      if (offset + token.size() > text.size())
        return false;

      if (text.substr(offset, token.size()) != token)
        return false;

      return tokenBoundary(text, offset + token.size());
    }

    [[nodiscard]]
    std::optional<int> parseInteger(std::string_view text, std::size_t& offset)
    {
      skipWhitespace(text, offset);

      if (offset >= text.size() || !std::isdigit(static_cast<unsigned char>(text[offset])))
        return std::nullopt;

      const auto begin = text.data() + offset;
      const auto end = text.data() + text.size();
      int value = 0;
      const auto result = std::from_chars(begin, end, value);

      if (result.ec != std::errc{})
        return std::nullopt;

      offset = static_cast<std::size_t>(result.ptr - text.data());

      return value;
    }

    [[nodiscard]]
    std::optional<std::string> parseName(std::string_view text, std::size_t& offset)
    {
      skipWhitespace(text, offset);

      if (offset >= text.size() || text[offset] != '/')
        return std::nullopt;

      ++offset;
      const auto begin = offset;

      while (offset < text.size() && !isAsciiWhitespace(text[offset]) && !isPdfDelimiter(text[offset])) {
        ++offset;
      }

      if (offset == begin)
        return std::nullopt;

      return std::string{text.substr(begin, offset - begin)};
    }

    [[nodiscard]]
    std::optional<std::string> readPdfBytes(
      const std::filesystem::path& path,
      const DocumentExtractionOptions& options,
      DocumentExtractionSummary& summary,
      std::stop_token stopToken)
    {
      std::ifstream file(path, std::ios::binary);

      if (!file) {
        summary.status = DocumentExtractionStatus::openFailed;

        return std::nullopt;
      }

      const auto maximumBytes = maximumSourceBytes(options);
      std::string bytes;
      std::array<char, pdfReadBufferBytes> buffer{};

      while (file) {
        if (stopToken.stop_requested()) {
          summary.status = DocumentExtractionStatus::cancelled;

          return std::nullopt;
        }

        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto readBytes = file.gcount();

        if (readBytes <= 0)
          break;

        if (bytes.size() + static_cast<std::size_t>(readBytes) > maximumBytes) {
          summary.status = DocumentExtractionStatus::safetyLimitExceeded;

          return std::nullopt;
        }

        bytes.append(buffer.data(), static_cast<std::size_t>(readBytes));
      }

      if (file.bad()) {
        summary.status = DocumentExtractionStatus::readFailed;

        return std::nullopt;
      }

      return bytes;
    }

    [[nodiscard]]
    bool hasPdfHeader(std::string_view bytes)
    {
      return bytes
        .substr(0, std::min(bytes.size(), maximumPdfHeaderProbeBytes))
        .find(pdfHeaderMarker) != std::string_view::npos;
    }

    [[nodiscard]]
    std::vector<PdfObject> pdfObjects(std::string_view bytes)
    {
      std::vector<PdfObject> objects;
      std::size_t offset = 0;

      while (offset < bytes.size()) {
        auto objectOffset = offset;
        const auto number = parseInteger(bytes, objectOffset);

        if (!number) {
          ++offset;

          continue;
        }

        const auto generation = parseInteger(bytes, objectOffset);

        if (!generation) {
          ++offset;

          continue;
        }

        skipWhitespace(bytes, objectOffset);

        if (!hasTokenAt(bytes, objectOffset, "obj")) {
          ++offset;

          continue;
        }

        objectOffset += 3;

        const auto endObject = bytes.find("endobj", objectOffset);

        if (endObject == std::string_view::npos)
          break;

        objects.push_back({
          .number = *number,
          .body = bytes.substr(objectOffset, endObject - objectOffset),
        });

        offset = endObject + 6;
      }

      return objects;
    }

    [[nodiscard]]
    bool containsName(std::string_view text, std::string_view name)
    {
      auto offset = text.find(name);

      while (offset != std::string_view::npos) {
        if (tokenBoundary(text, offset + name.size()))
          return true;

        offset = text.find(name, offset + name.size());
      }

      return false;
    }

    [[nodiscard]]
    bool isPageObject(std::string_view body)
    {
      const auto typeOffset = body.find(pageTypeMarker);

      if (typeOffset == std::string_view::npos)
        return false;

      const auto afterType = body.substr(typeOffset + pageTypeMarker.size());

      return containsName(afterType, pageMarker) && !containsName(afterType, pagesMarker);
    }

    [[nodiscard]]
    std::map<int, PdfObject> objectMap(const std::vector<PdfObject>& objects)
    {
      std::map<int, PdfObject> mappedObjects;

      for (const auto& object : objects) {
        mappedObjects.emplace(object.number, object);
      }

      return mappedObjects;
    }

    [[nodiscard]]
    std::vector<int> contentReferences(std::string_view pageBody)
    {
      std::vector<int> references;
      const auto contentsOffset = pageBody.find(contentsMarker);

      if (contentsOffset == std::string_view::npos)
        return references;

      auto scan = contentsOffset + contentsMarker.size();
      const auto scanEnd = std::min(pageBody.size(), scan + maximumReferenceScanBytes);

      while (scan < scanEnd) {
        const auto numberOffset = scan;
        const auto number = parseInteger(pageBody, scan);

        if (!number) {
          ++scan;

          continue;
        }

        const auto generation = parseInteger(pageBody, scan);

        if (!generation) {
          scan = numberOffset + 1;

          continue;
        }

        skipWhitespace(pageBody, scan);

        if (scan < pageBody.size() && pageBody[scan] == 'R') {
          references.push_back(*number);
          ++scan;

          continue;
        }

        scan = numberOffset + 1;
      }

      return references;
    }

    [[nodiscard]]
    std::optional<int> objectReferenceAfter(std::string_view body, std::string_view marker)
    {
      const auto markerOffset = body.find(marker);

      if (markerOffset == std::string_view::npos)
        return std::nullopt;

      auto scan = markerOffset + marker.size();
      const auto number = parseInteger(body, scan);

      if (!number)
        return std::nullopt;

      const auto generation = parseInteger(body, scan);

      if (!generation)
        return std::nullopt;

      skipWhitespace(body, scan);

      if (scan >= body.size() || body[scan] != 'R')
        return std::nullopt;

      return *number;
    }

    [[nodiscard]]
    std::map<std::string, int> fontReferences(std::string_view pageBody)
    {
      std::map<std::string, int> references;
      const auto fontOffset = pageBody.find(fontMarker);

      if (fontOffset == std::string_view::npos)
        return references;

      auto scan = fontOffset + fontMarker.size();
      const auto scanEnd = std::min(pageBody.size(), scan + maximumFontResourceScanBytes);

      while (scan < scanEnd) {
        const auto name = parseName(pageBody, scan);

        if (!name) {
          ++scan;

          continue;
        }

        const auto numberOffset = scan;
        const auto number = parseInteger(pageBody, scan);

        if (!number) {
          scan = numberOffset + 1;

          continue;
        }

        const auto generation = parseInteger(pageBody, scan);

        if (!generation) {
          scan = numberOffset + 1;

          continue;
        }

        skipWhitespace(pageBody, scan);

        if (scan < pageBody.size() && pageBody[scan] == 'R') {
          references.emplace(*name, *number);
          ++scan;

          continue;
        }

        scan = numberOffset + 1;
      }

      return references;
    }

    [[nodiscard]]
    std::vector<PdfStream> streamsFromObject(std::string_view body)
    {
      std::vector<PdfStream> streams;
      std::size_t offset = 0;

      while (offset < body.size()) {
        const auto streamOffset = body.find(streamMarker, offset);

        if (streamOffset == std::string_view::npos)
          break;

        auto dataOffset = streamOffset + streamMarker.size();

        if (dataOffset < body.size() && body[dataOffset] == '\r')
          ++dataOffset;

        if (dataOffset < body.size() && body[dataOffset] == '\n')
          ++dataOffset;

        const auto endOffset = body.find(endStreamMarker, dataOffset);

        if (endOffset == std::string_view::npos)
          break;

        streams.push_back({
          .dictionary = body.substr(0, streamOffset),
          .bytes = body.substr(dataOffset, endOffset - dataOffset),
        });
        offset = endOffset + endStreamMarker.size();
      }

      return streams;
    }

    [[nodiscard]]
    std::optional<std::string> inflateStream(std::string_view compressed)
    {
      z_stream stream{};

      if (inflateInit2(&stream, zlibWindowBits) != Z_OK)
        return std::nullopt;

      std::string output;
      std::array<unsigned char, zlibOutputBufferBytes> buffer{};

      stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.data()));
      stream.avail_in = static_cast<uInt>(compressed.size());

      int status = Z_OK;

      while (status == Z_OK) {
        stream.next_out = buffer.data();
        stream.avail_out = static_cast<uInt>(buffer.size());
        status = inflate(&stream, Z_NO_FLUSH);

        if (status != Z_OK && status != Z_STREAM_END)
          break;

        const auto producedBytes = buffer.size() - static_cast<std::size_t>(stream.avail_out);
        output.append(reinterpret_cast<const char*>(buffer.data()), producedBytes);
      }

      inflateEnd(&stream);

      if (status != Z_STREAM_END)
        return std::nullopt;

      return output;
    }

    [[nodiscard]]
    std::optional<std::string> decodedStream(PdfStream stream)
    {
      if (stream.dictionary.find(flateDecodeMarker) != std::string_view::npos)
        return inflateStream(stream.bytes);

      return std::string{stream.bytes};
    }

    [[nodiscard]]
    int hexValue(char character)
    {
      if (character >= '0' && character <= '9')
        return character - '0';

      if (character >= 'a' && character <= 'f')
        return character - 'a' + 10;

      if (character >= 'A' && character <= 'F')
        return character - 'A' + 10;

      return -1;
    }

    void appendEscapedPdfCharacter(std::string_view text, std::size_t& offset, std::string& output)
    {
      if (offset >= text.size())
        return;

      const auto character = text[offset++];

      switch (character) {
      case 'n':
        output.push_back('\n');
        return;
      case 'r':
        output.push_back('\n');
        return;
      case 't':
        output.push_back('\t');
        return;
      case 'b':
      case 'f':
        return;
      case '\n':
        return;
      case '\r':
        if (offset < text.size() && text[offset] == '\n')
          ++offset;

        return;
      default:
        break;
      }

      if (character >= '0' && character <= '7') {
        int value = character - '0';
        std::size_t digits = 1;

        while (digits < maximumOctalDigits && offset < text.size() && text[offset] >= '0' && text[offset] <= '7') {
          value = (value * 8) + (text[offset] - '0');
          ++offset;
          ++digits;
        }

        output.push_back(static_cast<char>(value));

        return;
      }

      output.push_back(character);
    }

    [[nodiscard]]
    std::optional<std::string> literalString(std::string_view text, std::size_t& offset)
    {
      if (offset >= text.size() || text[offset] != '(')
        return std::nullopt;

      ++offset;

      std::string value;
      int depth = 1;

      while (offset < text.size() && depth > 0) {
        const auto character = text[offset++];

        if (character == '\\') {
          appendEscapedPdfCharacter(text, offset, value);

          continue;
        }

        if (character == '(') {
          ++depth;
          value.push_back(character);

          continue;
        }

        if (character == ')') {
          --depth;

          if (depth > 0)
            value.push_back(character);

          continue;
        }

        value.push_back(character);
      }

      if (depth != 0)
        return std::nullopt;

      return value;
    }

    [[nodiscard]]
    std::optional<std::string> hexString(std::string_view text, std::size_t& offset)
    {
      if (offset + 1 >= text.size() || text[offset] != '<' || text[offset + 1] == '<')
        return std::nullopt;

      ++offset;

      std::string value;
      std::optional<int> highNibble;

      while (offset < text.size()) {
        const auto character = text[offset++];

        if (character == '>')
          break;

        if (isAsciiWhitespace(character))
          continue;

        const auto nibble = hexValue(character);

        if (nibble < 0)
          return std::nullopt;

        if (!highNibble) {
          highNibble = nibble;

          continue;
        }

        value.push_back(static_cast<char>((*highNibble << 4) | nibble));
        highNibble.reset();
      }

      if (highNibble)
        value.push_back(static_cast<char>(*highNibble << 4));

      return value;
    }

    [[nodiscard]]
    std::vector<std::string> hexStringsFromLine(std::string_view line)
    {
      std::vector<std::string> values;
      std::size_t offset = 0;

      while (offset < line.size()) {
        if (line[offset] == '<' && offset + 1 < line.size() && line[offset + 1] != '<') {
          if (auto value = hexString(line, offset)) {
            values.push_back(std::move(*value));

            continue;
          }
        }

        ++offset;
      }

      return values;
    }

    [[nodiscard]]
    std::uint32_t bigEndianInteger(std::string_view bytes)
    {
      std::uint32_t value = 0;

      for (const auto byte : bytes) {
        value = (value << 8U) | static_cast<unsigned char>(byte);
      }

      return value;
    }

    [[nodiscard]]
    std::string bigEndianBytes(std::uint32_t value, std::size_t bytes)
    {
      std::string output(bytes, '\0');

      for (std::size_t index = 0; index < bytes; ++index) {
        const auto shift = (bytes - index - 1U) * 8U;
        output[index] = static_cast<char>((value >> shift) & 0xFFU);
      }

      return output;
    }

    [[nodiscard]]
    std::optional<char32_t> firstUtf16Scalar(std::string_view bytes)
    {
      if (bytes.empty() || bytes.size() % utf16CodeUnitBytes != 0)
        return std::nullopt;

      std::size_t offset = hasUtf16BigEndianBom(bytes) ? utf16CodeUnitBytes : 0;

      if (offset >= bytes.size())
        return std::nullopt;

      return utf16Scalar(bytes, offset);
    }

    void addToUnicodeEntry(ToUnicodeMap& map, std::string code, std::string_view utf16Text)
    {
      if (code.empty())
        return;

      map.maximumCodeBytes = std::max(map.maximumCodeBytes, code.size());
      map.entries[std::move(code)] = utf16BigEndianToUtf8(utf16Text);
    }

    void parseBfCharLine(ToUnicodeMap& map, std::string_view line)
    {
      auto values = hexStringsFromLine(line);

      if (values.size() < 2)
        return;

      addToUnicodeEntry(map, std::move(values[0]), values[1]);
    }

    void parseBfRangeLine(ToUnicodeMap& map, std::string_view line)
    {
      auto values = hexStringsFromLine(line);

      if (values.size() < 3)
        return;

      const auto sourceStart = bigEndianInteger(values[0]);
      const auto sourceEnd = bigEndianInteger(values[1]);

      if (sourceEnd < sourceStart)
        return;

      if (line.find('[') != std::string_view::npos) {
        for (std::uint32_t source = sourceStart; source <= sourceEnd && source - sourceStart + 2U < values.size();
             ++source) {
          addToUnicodeEntry(
            map,
            bigEndianBytes(source, values[0].size()),
            values[static_cast<std::size_t>(source - sourceStart + 2U)]);
        }

        return;
      }

      const auto destinationStart = firstUtf16Scalar(values[2]);

      if (!destinationStart)
        return;

      for (std::uint32_t source = sourceStart; source <= sourceEnd; ++source) {
        std::string destination;
        appendUtf8(destination, *destinationStart + (source - sourceStart));
        auto code = bigEndianBytes(source, values[0].size());
        map.maximumCodeBytes = std::max(map.maximumCodeBytes, code.size());
        map.entries[std::move(code)] = std::move(destination);
      }
    }

    [[nodiscard]]
    ToUnicodeMap parseToUnicodeMap(std::string_view cmapText)
    {
      ToUnicodeMap map;
      bool insideBfChar = false;
      bool insideBfRange = false;
      std::size_t offset = 0;

      while (offset < cmapText.size()) {
        const auto lineEnd = cmapText.find_first_of("\r\n", offset);
        auto line = lineEnd == std::string_view::npos
          ? cmapText.substr(offset)
          : cmapText.substr(offset, lineEnd - offset);
        trimAsciiWhitespace(line);

        if (line.find(beginBfCharMarker) != std::string_view::npos) {
          insideBfChar = true;
          insideBfRange = false;
        } else if (line.find(endBfCharMarker) != std::string_view::npos) {
          insideBfChar = false;
        } else if (line.find(beginBfRangeMarker) != std::string_view::npos) {
          insideBfRange = true;
          insideBfChar = false;
        } else if (line.find(endBfRangeMarker) != std::string_view::npos) {
          insideBfRange = false;
        } else if (insideBfChar) {
          parseBfCharLine(map, line);
        } else if (insideBfRange) {
          parseBfRangeLine(map, line);
        }

        if (lineEnd == std::string_view::npos)
          break;

        offset = lineEnd + 1;

        if (offset < cmapText.size() && cmapText[lineEnd] == '\r' && cmapText[offset] == '\n')
          ++offset;
      }

      return map;
    }

    [[nodiscard]]
    FontUnicodeMaps fontUnicodeMapsForPage(const PdfObject& page, const std::map<int, PdfObject>& objects)
    {
      FontUnicodeMaps maps;

      for (const auto& [fontName, fontObjectNumber] : fontReferences(page.body)) {
        const auto fontObject = objects.find(fontObjectNumber);

        if (fontObject == objects.end())
          continue;

        const auto toUnicodeObjectNumber = objectReferenceAfter(fontObject->second.body, toUnicodeMarker);

        if (!toUnicodeObjectNumber)
          continue;

        const auto toUnicodeObject = objects.find(*toUnicodeObjectNumber);

        if (toUnicodeObject == objects.end())
          continue;

        const auto streams = streamsFromObject(toUnicodeObject->second.body);

        if (streams.empty())
          continue;

        const auto decoded = decodedStream(streams.front());

        if (!decoded)
          continue;

        auto map = parseToUnicodeMap(*decoded);

        if (!map.entries.empty())
          maps.emplace(fontName, std::move(map));
      }

      return maps;
    }

    [[nodiscard]]
    std::string decodePdfTextBytes(std::string bytes, const ToUnicodeMap* map)
    {
      if (map != nullptr && !map->entries.empty()) {
        std::string output;
        std::size_t offset = 0;

        while (offset < bytes.size()) {
          bool matched = false;
          const auto maximumCodeBytes = std::min(map->maximumCodeBytes, bytes.size() - offset);

          for (std::size_t size = maximumCodeBytes; size > 0; --size) {
            const auto mapped = map->entries.find(bytes.substr(offset, size));

            if (mapped == map->entries.end())
              continue;

            output += mapped->second;
            offset += size;
            matched = true;

            break;
          }

          if (!matched) {
            output += singleBytePdfTextToUtf8(std::string_view{bytes.data() + offset, 1});
            ++offset;
          }
        }

        return output;
      }

      if (hasUtf16BigEndianBom(bytes))
        return utf16BigEndianToUtf8(bytes);

      return singleBytePdfTextToUtf8(bytes);
    }

    void appendTextToken(std::string& pageText, std::string text)
    {
      xml::trimTrailingAsciiWhitespace(text);

      if (text.empty())
        return;

      if (!pageText.empty() && pageText.back() != '\n' && pageText.back() != ' ')
        pageText.push_back(' ');

      pageText += text;
    }

    [[nodiscard]]
    std::string textFromContentStream(std::string_view streamText, const FontUnicodeMaps& fontMaps)
    {
      std::string pageText;
      std::string lastName;
      const ToUnicodeMap* activeToUnicodeMap = nullptr;
      bool insideTextObject = false;
      std::size_t offset = 0;

      while (offset < streamText.size()) {
        if (hasTokenAt(streamText, offset, "BT")) {
          insideTextObject = true;
          offset += 2;

          continue;
        }

        if (hasTokenAt(streamText, offset, "ET")) {
          insideTextObject = false;
          offset += 2;

          if (!pageText.empty() && pageText.back() != '\n')
            pageText.push_back('\n');

          continue;
        }

        if (!insideTextObject) {
          ++offset;

          continue;
        }

        if (streamText[offset] == '/') {
          if (auto name = parseName(streamText, offset)) {
            lastName = std::move(*name);

            continue;
          }
        }

        if (hasTokenAt(streamText, offset, "Tf")) {
          const auto font = fontMaps.find(lastName);
          activeToUnicodeMap = font == fontMaps.end() ? nullptr : &font->second;
          offset += 2;

          continue;
        }

        if (streamText[offset] == '(') {
          if (auto value = literalString(streamText, offset)) {
            appendTextToken(pageText, decodePdfTextBytes(std::move(*value), activeToUnicodeMap));

            continue;
          }
        }

        if (streamText[offset] == '<') {
          if (auto value = hexString(streamText, offset)) {
            appendTextToken(pageText, decodePdfTextBytes(std::move(*value), activeToUnicodeMap));

            continue;
          }
        }

        ++offset;
      }

      xml::trimTrailingAsciiWhitespace(pageText);

      return pageText;
    }

    [[nodiscard]]
    std::string pageTextFromObject(
      const PdfObject& page,
      const std::map<int, PdfObject>& objects,
      DocumentExtractionSummary& summary)
    {
      std::string pageText;
      const auto fontMaps = fontUnicodeMapsForPage(page, objects);
      auto streams = streamsFromObject(page.body);

      for (const auto reference : contentReferences(page.body)) {
        const auto object = objects.find(reference);

        if (object == objects.end())
          continue;

        const auto referencedStreams = streamsFromObject(object->second.body);
        streams.insert(streams.end(), referencedStreams.begin(), referencedStreams.end());
      }

      for (const auto& stream : streams) {
        const auto decoded = decodedStream(stream);

        if (!decoded) {
          summary.status = DocumentExtractionStatus::parserFailed;

          return {};
        }

        appendTextToken(pageText, textFromContentStream(*decoded, fontMaps));
      }

      xml::trimTrailingAsciiWhitespace(pageText);

      return pageText;
    }

  } // namespace

  std::string_view PdfDocumentExtractor::name() const
  {
    return extractorName;
  }

  bool PdfDocumentExtractor::supports(const std::filesystem::path& path) const
  {
    return xml::lowerAscii(path.extension().string()) == ".pdf";
  }

  DocumentExtractionSummary PdfDocumentExtractor::extract(
    const std::filesystem::path& path,
    const DocumentExtractionOptions& options,
    const ExtractedTextSink& sink,
    std::stop_token stopToken) const
  {
    DocumentExtractionSummary summary;
    const auto bytes = readPdfBytes(path, options, summary, stopToken);

    if (!bytes)
      return summary;

    if (!hasPdfHeader(*bytes)) {
      summary.status = DocumentExtractionStatus::parserFailed;

      return summary;
    }

    if (bytes->find(encryptedDocumentMarker) != std::string::npos) {
      summary.status = DocumentExtractionStatus::encryptedOrProtected;

      return summary;
    }

    const auto objects = pdfObjects(*bytes);
    const auto mappedObjects = objectMap(objects);
    std::vector<PdfObject> pages;

    for (const auto& object : objects) {
      if (isPageObject(object.body))
        pages.push_back(object);
    }

    if (pages.empty()) {
      summary.status = DocumentExtractionStatus::parserFailed;

      return summary;
    }

    std::uintmax_t totalBytes = 0;

    for (std::size_t index = 0; index < pages.size(); ++index) {
      if (stopToken.stop_requested()) {
        summary.status = DocumentExtractionStatus::cancelled;

        return summary;
      }

      auto pageText = pageTextFromObject(pages[index], mappedObjects, summary);

      if (summary.status != DocumentExtractionStatus::completed)
        return summary;

      if (pageText.empty())
        continue;

      totalBytes += pageText.size();

      if (wouldExceedByteLimit(options, totalBytes) ||
          wouldExceedSegmentLimit(options, summary.segmentsExtracted + 1)) {
        summary.status = DocumentExtractionStatus::safetyLimitExceeded;

        return summary;
      }

      ExtractedTextSegment segment;

      segment.text = std::move(pageText);
      segment.location.kind = DocumentLocationKind::page;
      segment.location.primary = index + 1;
      segment.location.label = "page " + std::to_string(index + 1);

      if (!sink(segment)) {
        summary.status = DocumentExtractionStatus::cancelled;

        return summary;
      }

      ++summary.segmentsExtracted;
    }

    summary.bytesExtracted = totalBytes;

    return summary;
  }

} // namespace uburu::document
