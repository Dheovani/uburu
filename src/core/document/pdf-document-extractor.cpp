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
    constexpr std::string_view streamMarker = "stream";
    constexpr std::string_view endStreamMarker = "endstream";
    constexpr std::string_view flateDecodeMarker = "/FlateDecode";
    constexpr std::uintmax_t defaultMaximumPdfBytes = 128ULL * 1024ULL * 1024ULL;
    constexpr std::uintmax_t pdfSafetyMultiplier = 16;
    constexpr std::size_t maximumReferenceScanBytes = 2048;
    constexpr std::size_t maximumPdfHeaderProbeBytes = 1024;
    constexpr std::size_t maximumOctalDigits = 3;
    constexpr std::size_t pdfReadBufferBytes = 8192;
    constexpr std::size_t zlibOutputBufferBytes = 8192;
    constexpr int zlibWindowBits = 15;

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

    void skipWhitespace(std::string_view text, std::size_t& offset)
    {
      while (offset < text.size() && isAsciiWhitespace(text[offset])) {
        ++offset;
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
    std::string textFromContentStream(std::string_view streamText)
    {
      std::string pageText;
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

        if (streamText[offset] == '(') {
          if (auto value = literalString(streamText, offset)) {
            appendTextToken(pageText, std::move(*value));

            continue;
          }
        }

        if (streamText[offset] == '<') {
          if (auto value = hexString(streamText, offset)) {
            appendTextToken(pageText, std::move(*value));

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

        appendTextToken(pageText, textFromContentStream(*decoded));
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
