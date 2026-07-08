#include "core/document/html-document-extractor.hpp"

#include "core/text/text-file-reader.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace uburu::document
{
  namespace
  {

    constexpr std::size_t maximumHtmlEntityLength = 32;
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
      return character == ' ' || character == '\t' || character == '\n' || character == '\r' || character == '\f';
    }

    [[nodiscard]]
    bool isValidUnicodeScalar(char32_t scalar)
    {
      return scalar <= maximumUnicodeScalar && (scalar < surrogateRangeStart || scalar > surrogateRangeEnd);
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

      if (parseResult.ec != std::errc{} || parseResult.ptr != end ||
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
        EntityReplacement{"nbsp", " "},
        EntityReplacement{"quot", "\""},
      };

      for (const auto& replacement : replacements) {
        if (entity == replacement.entity)
          return replacement.replacement;
      }

      return std::nullopt;
    }

    void appendDecodedText(std::string_view text, std::string& output)
    {
      std::size_t offset = 0;

      while (offset < text.size()) {
        if (text[offset] != '&') {
          output.push_back(text[offset]);
          ++offset;

          continue;
        }

        const auto semicolonOffset = text.find(';', offset + 1);

        if (semicolonOffset == std::string_view::npos || semicolonOffset - offset - 1 > maximumHtmlEntityLength) {
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

    void appendWordSeparator(std::string& output)
    {
      if (output.empty() || output.back() == '\n' || output.back() == ' ')
        return;

      output.push_back(' ');
    }

    void appendBlockSeparator(std::string& output)
    {
      while (!output.empty() && output.back() == ' ') {
        output.pop_back();
      }

      if (output.empty() || output.back() == '\n')
        return;

      output.push_back('\n');
    }

    [[nodiscard]]
    bool isBlockTag(std::string_view tagName)
    {
      constexpr std::array blockTags{"address", "article", "aside",      "blockquote", "br",     "dd",      "div",
                                     "dl",      "dt",      "figcaption", "figure",     "footer", "h1",      "h2",
                                     "h3",      "h4",      "h5",         "h6",         "header", "hr",      "li",
                                     "main",    "nav",     "ol",         "p",          "pre",    "section", "table",
                                     "tbody",   "td",      "tfoot",      "th",         "thead",  "tr",      "ul"};

      for (const auto blockTag : blockTags) {
        if (tagName == blockTag)
          return true;
      }

      return false;
    }

    [[nodiscard]]
    std::string tagNameFromContent(std::string_view content)
    {
      while (!content.empty() && isAsciiWhitespace(content.front())) {
        content.remove_prefix(1);
      }

      if (!content.empty() && content.front() == '/')
        content.remove_prefix(1);

      while (!content.empty() && isAsciiWhitespace(content.front())) {
        content.remove_prefix(1);
      }

      std::size_t size = 0;

      while (size < content.size()) {
        const auto character = content[size];

        if (isAsciiWhitespace(character) || character == '/' || character == '>')
          break;

        ++size;
      }

      return lowerAscii(content.substr(0, size));
    }

    [[nodiscard]]
    bool isClosingTag(std::string_view content)
    {
      while (!content.empty() && isAsciiWhitespace(content.front())) {
        content.remove_prefix(1);
      }

      return !content.empty() && content.front() == '/';
    }

    [[nodiscard]]
    std::size_t findCaseInsensitive(std::string_view text, std::string_view needle, std::size_t start)
    {
      if (needle.empty() || needle.size() > text.size())
        return std::string_view::npos;

      for (std::size_t offset = start; offset + needle.size() <= text.size(); ++offset) {
        bool matched = true;

        for (std::size_t index = 0; index < needle.size(); ++index) {
          auto character = text[offset + index];
          auto expected = needle[index];

          if (character >= 'A' && character <= 'Z')
            character = static_cast<char>(character - 'A' + 'a');

          if (expected >= 'A' && expected <= 'Z')
            expected = static_cast<char>(expected - 'A' + 'a');

          if (character != expected) {
            matched = false;

            break;
          }
        }

        if (matched)
          return offset;
      }

      return std::string_view::npos;
    }

    [[nodiscard]]
    std::string visibleTextFromHtml(std::string_view html, std::stop_token stopToken)
    {
      std::string visibleText;
      std::size_t offset = 0;

      visibleText.reserve(html.size());

      while (offset < html.size()) {
        if (stopToken.stop_requested())
          break;

        const auto tagStart = html.find('<', offset);

        if (tagStart == std::string_view::npos) {
          appendDecodedText(html.substr(offset), visibleText);

          break;
        }

        appendDecodedText(html.substr(offset, tagStart - offset), visibleText);

        if (html.substr(tagStart, 4) == "<!--") {
          const auto commentEnd = html.find("-->", tagStart + 4);
          offset = commentEnd == std::string_view::npos ? html.size() : commentEnd + 3;

          continue;
        }

        const auto tagEnd = html.find('>', tagStart + 1);

        if (tagEnd == std::string_view::npos) {
          appendWordSeparator(visibleText);

          break;
        }

        const auto tagContent = html.substr(tagStart + 1, tagEnd - tagStart - 1);
        const auto tagName = tagNameFromContent(tagContent);

        if ((tagName == "script" || tagName == "style") && !isClosingTag(tagContent)) {
          const auto closingTag = std::string{"</"} + tagName;
          const auto closingStart = findCaseInsensitive(html, closingTag, tagEnd + 1);

          appendWordSeparator(visibleText);

          if (closingStart == std::string_view::npos) {
            offset = html.size();

            continue;
          }

          const auto closingEnd = html.find('>', closingStart + closingTag.size());
          offset = closingEnd == std::string_view::npos ? html.size() : closingEnd + 1;

          continue;
        }

        if (isBlockTag(tagName))
          appendBlockSeparator(visibleText);
        else
          appendWordSeparator(visibleText);

        offset = tagEnd + 1;
      }

      while (!visibleText.empty() && isAsciiWhitespace(visibleText.back())) {
        visibleText.pop_back();
      }

      return visibleText;
    }

    [[nodiscard]]
    bool wouldExceedByteLimit(const DocumentExtractionOptions& options, std::size_t bytes)
    {
      return options.maximumExtractedBytes > 0 && bytes > options.maximumExtractedBytes;
    }

    [[nodiscard]]
    DocumentExtractionStatus extractionStatusFromTextStatus(text::TextReadStatus status)
    {
      switch (status) {
      case text::TextReadStatus::completed:
        return DocumentExtractionStatus::completed;
      case text::TextReadStatus::cancelled:
        return DocumentExtractionStatus::cancelled;
      case text::TextReadStatus::openFailed:
        return DocumentExtractionStatus::openFailed;
      case text::TextReadStatus::readFailed:
        return DocumentExtractionStatus::readFailed;
      case text::TextReadStatus::binarySkipped:
        return DocumentExtractionStatus::binarySkipped;
      case text::TextReadStatus::invalidEncoding:
        return DocumentExtractionStatus::invalidEncoding;
      case text::TextReadStatus::lineTooLong:
        return DocumentExtractionStatus::safetyLimitExceeded;
      }

      return DocumentExtractionStatus::parserFailed;
    }

  } // namespace

  std::string_view HtmlDocumentExtractor::name() const
  {
    return "html";
  }

  bool HtmlDocumentExtractor::supports(const std::filesystem::path& path) const
  {
    const auto extension = lowerAscii(path.extension().string());

    return extension == ".html" || extension == ".htm" || extension == ".xhtml";
  }

  DocumentExtractionSummary HtmlDocumentExtractor::extract(const std::filesystem::path& path,
                                                           const DocumentExtractionOptions& options,
                                                           const ExtractedTextSink& sink,
                                                           std::stop_token stopToken) const
  {
    DocumentExtractionSummary extractionSummary;
    std::string html;
    bool firstLine = true;

    const auto readSummary = text::readTextFileLines(
      path,
      options.textOptions,
      [&](const text::TextLine& line) {
        if (!firstLine)
          html.push_back('\n');

        html += line.text;
        firstLine = false;

        return !stopToken.stop_requested();
      },
      stopToken);

    extractionSummary.encoding = readSummary.encoding;
    extractionSummary.error = readSummary.error;
    extractionSummary.hadBom = readSummary.hadBom;
    extractionSummary.hadInvalidSequences = readSummary.hadInvalidSequences;
    extractionSummary.status = extractionStatusFromTextStatus(readSummary.status);

    if (extractionSummary.status != DocumentExtractionStatus::completed)
      return extractionSummary;

    if (stopToken.stop_requested()) {
      extractionSummary.status = DocumentExtractionStatus::cancelled;

      return extractionSummary;
    }

    auto visibleText = visibleTextFromHtml(html, stopToken);

    if (stopToken.stop_requested()) {
      extractionSummary.status = DocumentExtractionStatus::cancelled;

      return extractionSummary;
    }

    if (wouldExceedByteLimit(options, visibleText.size())) {
      extractionSummary.status = DocumentExtractionStatus::safetyLimitExceeded;

      return extractionSummary;
    }

    ExtractedTextSegment segment;

    segment.text = std::move(visibleText);
    segment.location.kind = DocumentLocationKind::none;
    segment.location.label = "document";

    if (!sink(segment)) {
      extractionSummary.status = DocumentExtractionStatus::cancelled;

      return extractionSummary;
    }

    extractionSummary.segmentsExtracted = 1;
    extractionSummary.bytesExtracted = segment.text.size();

    return extractionSummary;
  }

} // namespace uburu::document
