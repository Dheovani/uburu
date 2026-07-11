#include "core/document/xml-document-utils.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <system_error>

namespace uburu::document::xml
{
  namespace
  {

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
      const bool invalidUnicodeScalar = !isValidUnicodeScalar(static_cast<char32_t>(scalar));

      if (parseResult.ec != std::errc{} || parseResult.ptr != end || invalidUnicodeScalar)
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

    void trimLeadingTagSyntax(std::string_view& tagContent)
    {
      while (!tagContent.empty() && isAsciiWhitespace(tagContent.front())) {
        tagContent.remove_prefix(1);
      }

      if (!tagContent.empty() && tagContent.front() == '/')
        tagContent.remove_prefix(1);

      while (!tagContent.empty() && isAsciiWhitespace(tagContent.front())) {
        tagContent.remove_prefix(1);
      }
    }

    [[nodiscard]]
    std::string_view consumeXmlName(std::string_view& text)
    {
      std::size_t size = 0;

      while (size < text.size()) {
        const auto character = text[size];

        if (isAsciiWhitespace(character) || character == '=' || character == '/' || character == '>')
          break;

        ++size;
      }

      const auto name = text.substr(0, size);
      text.remove_prefix(size);

      return name;
    }

    void trimLeadingAsciiWhitespace(std::string_view& text)
    {
      while (!text.empty() && isAsciiWhitespace(text.front())) {
        text.remove_prefix(1);
      }
    }

    void trimTrailingAsciiWhitespace(std::string_view& text)
    {
      while (!text.empty() && isAsciiWhitespace(text.back())) {
        text.remove_suffix(1);
      }
    }

    [[nodiscard]]
    bool xmlNamesMatch(std::string_view left, std::string_view right)
    {
      if (right.find(':') != std::string_view::npos)
        return lowerAscii(left) == lowerAscii(right);

      return localNameFromTag(left) == lowerAscii(right);
    }

  } // namespace

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

  bool isAsciiWhitespace(char character)
  {
    return character == ' '
        || character == '\t'
        || character == '\n'
        || character == '\r'
        || character == '\f';
  }

  bool isClosingTag(std::string_view tagContent)
  {
    while (!tagContent.empty() && isAsciiWhitespace(tagContent.front())) {
      tagContent.remove_prefix(1);
    }

    return !tagContent.empty() && tagContent.front() == '/';
  }

  bool isSelfClosingTag(std::string_view tagContent)
  {
    while (!tagContent.empty() && isAsciiWhitespace(tagContent.back())) {
      tagContent.remove_suffix(1);
    }

    return !tagContent.empty() && tagContent.back() == '/';
  }

  std::string localNameFromTag(std::string_view tagContent)
  {
    trimLeadingTagSyntax(tagContent);
    auto name = lowerAscii(consumeXmlName(tagContent));
    const auto namespaceSeparator = name.find(':');

    if (namespaceSeparator != std::string::npos)
      name.erase(0, namespaceSeparator + 1);

    return name;
  }

  std::optional<std::string> attributeValue(std::string_view tagContent, std::string_view attributeName)
  {
    trimLeadingTagSyntax(tagContent);

    // The input is the whole tag body, such as `c r="A1" t="s"`. Consume the element name first so the loop below
    // works only on attributes.
    const auto elementName = consumeXmlName(tagContent);

    if (elementName.empty())
      return std::nullopt;

    while (!tagContent.empty()) {
      trimLeadingAsciiWhitespace(tagContent);

      if (tagContent.empty() || tagContent.front() == '/' || tagContent.front() == '>')
        return std::nullopt;

      auto name = consumeXmlName(tagContent);
      trimTrailingAsciiWhitespace(name);
      trimLeadingAsciiWhitespace(tagContent);

      if (name.empty() || tagContent.empty() || tagContent.front() != '=')
        return std::nullopt;

      tagContent.remove_prefix(1);
      trimLeadingAsciiWhitespace(tagContent);

      if (tagContent.empty())
        return std::nullopt;

      const auto quote = tagContent.front();

      if (quote != '"' && quote != '\'')
        return std::nullopt;

      tagContent.remove_prefix(1);

      const auto endQuote = tagContent.find(quote);

      if (endQuote == std::string_view::npos)
        return std::nullopt;

      if (xmlNamesMatch(name, attributeName)) {
        std::string decoded;
        appendDecodedText(tagContent.substr(0, endQuote), decoded);

        return decoded;
      }

      tagContent.remove_prefix(endQuote + 1);
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

  void trimTrailingAsciiWhitespace(std::string& text)
  {
    while (!text.empty() && isAsciiWhitespace(text.back())) {
      text.pop_back();
    }
  }

} // namespace uburu::document::xml
