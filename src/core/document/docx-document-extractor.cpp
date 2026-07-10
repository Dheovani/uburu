#include "core/document/docx-document-extractor.hpp"

#include "core/archive/zip-archive-reader.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace uburu::document
{
  namespace
  {

    constexpr std::string_view wordDocumentXmlPath = "word/document.xml";
    constexpr std::string_view extractorName = "docx";
    constexpr std::string_view documentLocationLabel = "document";
    constexpr std::size_t maximumXmlEntityLength = 32;
    constexpr char32_t maximumUnicodeScalar = 0x10FFFFU;
    constexpr char32_t surrogateRangeStart = 0xD800U;
    constexpr char32_t surrogateRangeEnd = 0xDFFFU;
    constexpr unsigned char utf8ContinuationByteTag = 0b1000'0000U;
    constexpr unsigned char utf8TwoByteLeadTag = 0b1100'0000U;
    constexpr unsigned char utf8ThreeByteLeadTag = 0b1110'0000U;
    constexpr unsigned char utf8FourByteLeadTag = 0b1111'0000U;
    constexpr unsigned char utf8ContinuationPayloadMask = 0b0011'1111U;
    constexpr unsigned int utf8OneByteMaximum = 0x7FU;
    constexpr unsigned int utf8TwoByteMaximum = 0x7FFU;
    constexpr unsigned int utf8ThreeByteMaximum = 0xFFFFU;
    constexpr unsigned int utf8SixBitShift = 6U;
    constexpr unsigned int utf8TwelveBitShift = 12U;
    constexpr unsigned int utf8EighteenBitShift = 18U;

    [[nodiscard]]
    std::string lowerAscii(std::string_view text)
    {
      std::string lowered;
      lowered.reserve(text.size());

      for (const auto character : text) {
        if (character >= 'A' && character <= 'Z') {
          lowered.push_back(static_cast<char>(character - 'A' + 'a'));

          continue;
        }

        lowered.push_back(character);
      }

      return lowered;
    }

    [[nodiscard]]
    bool isAsciiWhitespace(char character)
    {
      return character == ' ' ||
             character == '\t' ||
             character == '\n' ||
             character == '\r' ||
             character == '\f';
    }

    [[nodiscard]]
    bool isValidUnicodeScalar(char32_t scalar)
    {
      return scalar <= maximumUnicodeScalar &&
             (scalar < surrogateRangeStart || scalar > surrogateRangeEnd);
    }

    void appendUtf8(char32_t scalar, std::string& output)
    {
      if (!isValidUnicodeScalar(scalar))
        return;

      if (scalar <= utf8OneByteMaximum) {
        output.push_back(static_cast<char>(scalar));

        return;
      }

      if (scalar <= utf8TwoByteMaximum) {
        output.push_back(static_cast<char>(utf8TwoByteLeadTag | (scalar >> utf8SixBitShift)));
        output.push_back(static_cast<char>(utf8ContinuationByteTag | (scalar & utf8ContinuationPayloadMask)));

        return;
      }

      if (scalar <= utf8ThreeByteMaximum) {
        output.push_back(static_cast<char>(utf8ThreeByteLeadTag | (scalar >> utf8TwelveBitShift)));
        output.push_back(
          static_cast<char>(utf8ContinuationByteTag | ((scalar >> utf8SixBitShift) & utf8ContinuationPayloadMask)));
        output.push_back(static_cast<char>(utf8ContinuationByteTag | (scalar & utf8ContinuationPayloadMask)));

        return;
      }

      output.push_back(static_cast<char>(utf8FourByteLeadTag | (scalar >> utf8EighteenBitShift)));
      output.push_back(
        static_cast<char>(utf8ContinuationByteTag | ((scalar >> utf8TwelveBitShift) & utf8ContinuationPayloadMask)));
      output.push_back(
        static_cast<char>(utf8ContinuationByteTag | ((scalar >> utf8SixBitShift) & utf8ContinuationPayloadMask)));
      output.push_back(static_cast<char>(utf8ContinuationByteTag | (scalar & utf8ContinuationPayloadMask)));
    }

    [[nodiscard]]
    std::optional<char32_t> parseNumericEntity(std::string_view entity)
    {
      if (entity.size() < 2 || entity.front() != '#')
        return std::nullopt;

      int base = 10;
      std::string_view digits = entity.substr(1);

      if (!digits.empty() && (digits.front() == 'x' || digits.front() == 'X')) {
        base = 16;
        digits.remove_prefix(1);
      }

      if (digits.empty())
        return std::nullopt;

      std::uint32_t scalar = 0;
      const auto* begin = digits.data();
      const auto* end = digits.data() + digits.size();
      const auto parseResult = std::from_chars(begin, end, scalar, base);

      if (parseResult.ec != std::errc{} ||
          parseResult.ptr != end ||
          !isValidUnicodeScalar(static_cast<char32_t>(scalar)))
        return std::nullopt;

      return static_cast<char32_t>(scalar);
    }

    [[nodiscard]]
    std::optional<std::string_view> namedEntityReplacement(std::string_view entity)
    {
      struct EntityReplacement
      {
        std::string_view entity;
        std::string_view replacement;
      };

      constexpr std::array replacements{
        EntityReplacement{"amp", "&"},
        EntityReplacement{"apos", "'"},
        EntityReplacement{"gt", ">"},
        EntityReplacement{"lt", "<"},
        EntityReplacement{"quot", "\""},
      };

      for (const auto& replacement : replacements) {
        if (entity == replacement.entity)
          return replacement.replacement;
      }

      return std::nullopt;
    }

    void appendDecodedXmlText(std::string_view text, std::string& output)
    {
      std::size_t offset = 0;

      while (offset < text.size()) {
        if (text[offset] != '&') {
          output.push_back(text[offset]);
          ++offset;

          continue;
        }

        const auto semicolonOffset = text.find(';', offset + 1);

        if (semicolonOffset == std::string_view::npos || semicolonOffset - offset - 1 > maximumXmlEntityLength) {
          output.push_back(text[offset]);
          ++offset;

          continue;
        }

        const auto entity = text.substr(offset + 1, semicolonOffset - offset - 1);

        if (const auto scalar = parseNumericEntity(entity)) {
          appendUtf8(*scalar, output);
          offset = semicolonOffset + 1;

          continue;
        }

        if (const auto replacement = namedEntityReplacement(lowerAscii(entity))) {
          output += *replacement;
          offset = semicolonOffset + 1;

          continue;
        }

        output.push_back(text[offset]);
        ++offset;
      }
    }

    void appendSpace(std::string& output)
    {
      if (output.empty() || output.back() == '\n' || output.back() == ' ')
        return;

      output.push_back(' ');
    }

    void appendLineBreak(std::string& output)
    {
      while (!output.empty() && output.back() == ' ') {
        output.pop_back();
      }

      if (output.empty() || output.back() == '\n')
        return;

      output.push_back('\n');
    }

    [[nodiscard]]
    bool isClosingTag(std::string_view tagContent)
    {
      while (!tagContent.empty() && isAsciiWhitespace(tagContent.front())) {
        tagContent.remove_prefix(1);
      }

      return !tagContent.empty() && tagContent.front() == '/';
    }

    [[nodiscard]]
    bool isSelfClosingTag(std::string_view tagContent)
    {
      while (!tagContent.empty() && isAsciiWhitespace(tagContent.back())) {
        tagContent.remove_suffix(1);
      }

      return !tagContent.empty() && tagContent.back() == '/';
    }

    [[nodiscard]]
    std::string localXmlNameFromTag(std::string_view tagContent)
    {
      while (!tagContent.empty() && isAsciiWhitespace(tagContent.front())) {
        tagContent.remove_prefix(1);
      }

      if (!tagContent.empty() && tagContent.front() == '/')
        tagContent.remove_prefix(1);

      while (!tagContent.empty() && isAsciiWhitespace(tagContent.front())) {
        tagContent.remove_prefix(1);
      }

      std::size_t size = 0;

      while (size < tagContent.size()) {
        const auto character = tagContent[size];

        if (isAsciiWhitespace(character) || character == '/' || character == '>')
          break;

        ++size;
      }

      auto name = lowerAscii(tagContent.substr(0, size));
      const auto namespaceSeparator = name.find(':');

      if (namespaceSeparator != std::string::npos)
        name.erase(0, namespaceSeparator + 1);

      return name;
    }

    [[nodiscard]]
    bool wouldExceedByteLimit(const DocumentExtractionOptions& options, std::size_t bytes)
    {
      return options.maximumExtractedBytes > 0 && bytes > options.maximumExtractedBytes;
    }

    [[nodiscard]]
    std::string visibleTextFromWordDocumentXml(std::string_view xml, std::stop_token stopToken)
    {
      std::string visibleText;
      bool insideTextNode = false;
      std::size_t offset = 0;

      visibleText.reserve(xml.size() / 2);

      while (offset < xml.size()) {
        if (stopToken.stop_requested())
          break;

        const auto tagStart = xml.find('<', offset);

        if (tagStart == std::string_view::npos) {
          if (insideTextNode)
            appendDecodedXmlText(xml.substr(offset), visibleText);

          break;
        }

        if (insideTextNode)
          appendDecodedXmlText(xml.substr(offset, tagStart - offset), visibleText);

        const auto tagEnd = xml.find('>', tagStart + 1);

        if (tagEnd == std::string_view::npos)
          break;

        const auto tagContent = xml.substr(tagStart + 1, tagEnd - tagStart - 1);
        const auto tagName = localXmlNameFromTag(tagContent);
        const auto closingTag = isClosingTag(tagContent);

        if (tagName == "t") {
          insideTextNode = !closingTag && !isSelfClosingTag(tagContent);
        } else if (!insideTextNode && tagName == "tab") {
          appendSpace(visibleText);
        } else if (!insideTextNode && (tagName == "br" || tagName == "cr")) {
          appendLineBreak(visibleText);
        } else if (closingTag && tagName == "p") {
          appendLineBreak(visibleText);
        }

        offset = tagEnd + 1;
      }

      while (!visibleText.empty() && isAsciiWhitespace(visibleText.back())) {
        visibleText.pop_back();
      }

      return visibleText;
    }

    [[nodiscard]]
    DocumentExtractionStatus statusFromZipStatus(archive::ZipArchiveReadStatus status)
    {
      switch (status) {
      case archive::ZipArchiveReadStatus::completed:
        return DocumentExtractionStatus::completed;
      case archive::ZipArchiveReadStatus::cancelled:
        return DocumentExtractionStatus::cancelled;
      case archive::ZipArchiveReadStatus::openFailed:
        return DocumentExtractionStatus::openFailed;
      case archive::ZipArchiveReadStatus::readFailed:
        return DocumentExtractionStatus::readFailed;
      case archive::ZipArchiveReadStatus::safetyLimitExceeded:
      case archive::ZipArchiveReadStatus::unsafeEntryName:
      case archive::ZipArchiveReadStatus::unsupportedCompressionMethod:
      case archive::ZipArchiveReadStatus::unsupportedZip64:
        return DocumentExtractionStatus::safetyLimitExceeded;
      case archive::ZipArchiveReadStatus::decompressionFailed:
      case archive::ZipArchiveReadStatus::entryNotFound:
      case archive::ZipArchiveReadStatus::invalidArchive:
        return DocumentExtractionStatus::parserFailed;
      }

      return DocumentExtractionStatus::parserFailed;
    }

  } // namespace

  std::string_view DocxDocumentExtractor::name() const
  {
    return extractorName;
  }

  bool DocxDocumentExtractor::supports(const std::filesystem::path& path) const
  {
    return lowerAscii(path.extension().string()) == ".docx";
  }

  DocumentExtractionSummary DocxDocumentExtractor::extract(
    const std::filesystem::path& path,
    const DocumentExtractionOptions& options,
    const ExtractedTextSink& sink,
    std::stop_token stopToken) const
  {
    DocumentExtractionSummary summary;
    archive::ZipArchiveReader reader;
    const auto catalog = reader.readCatalog(path, {}, stopToken);

    if (catalog.status != archive::ZipArchiveReadStatus::completed) {
      summary.status = statusFromZipStatus(catalog.status);
      summary.error = catalog.error;

      return summary;
    }

    const auto documentEntry =
      std::ranges::find(catalog.entries, wordDocumentXmlPath, &archive::ZipArchiveEntry::rawName);

    if (documentEntry == catalog.entries.end()) {
      summary.status = DocumentExtractionStatus::parserFailed;

      return summary;
    }

    const auto entryRead = reader.readEntry(path, *documentEntry, {}, stopToken);

    if (entryRead.status != archive::ZipArchiveReadStatus::completed) {
      summary.status = statusFromZipStatus(entryRead.status);
      summary.error = entryRead.error;

      return summary;
    }

    if (stopToken.stop_requested()) {
      summary.status = DocumentExtractionStatus::cancelled;

      return summary;
    }

    const std::string xml(reinterpret_cast<const char*>(entryRead.bytes.data()), entryRead.bytes.size());
    auto visibleText = visibleTextFromWordDocumentXml(xml, stopToken);

    if (stopToken.stop_requested()) {
      summary.status = DocumentExtractionStatus::cancelled;

      return summary;
    }

    if (wouldExceedByteLimit(options, visibleText.size())) {
      summary.status = DocumentExtractionStatus::safetyLimitExceeded;

      return summary;
    }

    ExtractedTextSegment segment;

    segment.text = std::move(visibleText);
    segment.location.kind = DocumentLocationKind::none;
    segment.location.label = documentLocationLabel;

    if (!sink(segment)) {
      summary.status = DocumentExtractionStatus::cancelled;

      return summary;
    }

    summary.segmentsExtracted = 1;
    summary.bytesExtracted = segment.text.size();

    return summary;
  }

} // namespace uburu::document
