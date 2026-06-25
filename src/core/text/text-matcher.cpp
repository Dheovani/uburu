#include "core/text/text-matcher.hpp"

#include <algorithm>
#include <cstdint>

namespace uburu::text
{
  namespace
  {

    constexpr std::size_t singleByteLength = 1;
    constexpr std::size_t twoByteLength = 2;
    constexpr std::size_t threeByteLength = 3;
    constexpr std::size_t fourByteLength = 4;

    constexpr unsigned char asciiMax = 0x7FU;
    constexpr unsigned char utf8ContinuationTagMask = 0b1100'0000U;
    constexpr unsigned char utf8ContinuationTag = 0b1000'0000U;
    constexpr unsigned char utf8ContinuationPayloadMask = 0b0011'1111U;

    constexpr unsigned char utf8TwoByteTagMask = 0b1110'0000U;
    constexpr unsigned char utf8TwoByteTag = 0b1100'0000U;
    constexpr unsigned char utf8TwoBytePayloadMask = 0b0001'1111U;

    constexpr unsigned char utf8ThreeByteTagMask = 0b1111'0000U;
    constexpr unsigned char utf8ThreeByteTag = 0b1110'0000U;
    constexpr unsigned char utf8ThreeBytePayloadMask = 0b0000'1111U;

    constexpr unsigned char utf8FourByteTagMask = 0b1111'1000U;
    constexpr unsigned char utf8FourByteTag = 0b1111'0000U;
    constexpr unsigned char utf8FourBytePayloadMask = 0b0000'0111U;

    constexpr std::size_t utf8ContinuationPayloadBits = 6;
    constexpr std::size_t utf8ThreeByteLeadShift = 12;
    constexpr std::size_t utf8FourByteLeadShift = 18;
    constexpr std::size_t utf8FourByteSecondShift = 12;

    constexpr char32_t minimumTwoByteScalar = 0x80U;
    constexpr char32_t minimumThreeByteScalar = 0x800U;
    constexpr char32_t minimumFourByteScalar = 0x10000U;
    constexpr char32_t maximumUnicodeScalar = 0x10FFFFU;
    constexpr char32_t surrogateMin = 0xD800U;
    constexpr char32_t surrogateMax = 0xDFFFU;

    constexpr char32_t asciiUppercaseMin = U'A';
    constexpr char32_t asciiUppercaseMax = U'Z';
    constexpr char32_t asciiLowercaseMin = U'a';
    constexpr char32_t asciiLowercaseMax = U'z';
    constexpr char32_t asciiDigitMin = U'0';
    constexpr char32_t asciiDigitMax = U'9';
    constexpr char32_t asciiIdentifierConnector = U'_';

    constexpr char32_t latinUppercaseMin = 0x00C0U;
    constexpr char32_t latinUppercaseBeforeMultiplicationSignMax = 0x00D6U;
    constexpr char32_t latinUppercaseAfterMultiplicationSignMin = 0x00D8U;
    constexpr char32_t latinUppercaseMax = 0x00DEU;
    constexpr char32_t latinLowercaseMin = 0x00E0U;
    constexpr char32_t latinLowercaseBeforeDivisionSignMax = 0x00F6U;
    constexpr char32_t latinLowercaseAfterDivisionSignMin = 0x00F8U;
    constexpr char32_t latinLowercaseMax = 0x00FFU;
    constexpr char32_t lowercaseCodepointDelta = 32U;

    struct DecodedScalar
    {
      char32_t value{0};
      std::size_t byteLength{singleByteLength};
    };

    bool isAsciiLetter(char32_t scalar)
    {
      return (scalar >= asciiUppercaseMin && scalar <= asciiUppercaseMax) ||
             (scalar >= asciiLowercaseMin && scalar <= asciiLowercaseMax);
    }

    bool isAsciiDigit(char32_t scalar)
    {
      return scalar >= asciiDigitMin && scalar <= asciiDigitMax;
    }

    bool isLatinLetter(char32_t scalar)
    {
      return (scalar >= latinUppercaseMin &&
              scalar <= latinUppercaseBeforeMultiplicationSignMax) ||
             (scalar >= latinUppercaseAfterMultiplicationSignMin &&
              scalar <= latinUppercaseMax) ||
             (scalar >= latinLowercaseMin &&
              scalar <= latinLowercaseBeforeDivisionSignMax) ||
             (scalar >= latinLowercaseAfterDivisionSignMin && scalar <= latinLowercaseMax);
    }

    bool isNaturalWordScalar(char32_t scalar)
    {
      return isAsciiLetter(scalar) || isAsciiDigit(scalar) || isLatinLetter(scalar);
    }

    bool isCodeIdentifierScalar(char32_t scalar)
    {
      return isAsciiLetter(scalar) || isAsciiDigit(scalar) ||
             scalar == asciiIdentifierConnector;
    }

    bool isContinuationByte(unsigned char byte)
    {
      return (byte & utf8ContinuationTagMask) == utf8ContinuationTag;
    }

    DecodedScalar decodeUtf8At(std::string_view text, std::size_t offset)
    {
      const auto first = static_cast<unsigned char>(text[offset]);
      if (first <= asciiMax)
        return DecodedScalar{first, singleByteLength};

      auto continuation = [&](std::size_t position) -> std::optional<unsigned char> {
        if (position >= text.size())
          return std::nullopt;
        const auto byte = static_cast<unsigned char>(text[position]);
        if (!isContinuationByte(byte))
          return std::nullopt;
        return byte;
      };

      if ((first & utf8TwoByteTagMask) == utf8TwoByteTag) {
        const auto second = continuation(offset + 1);
        if (!second)
          return DecodedScalar{first, singleByteLength};
        const char32_t value =
            ((first & utf8TwoBytePayloadMask) << utf8ContinuationPayloadBits) |
            (*second & utf8ContinuationPayloadMask);
        if (value < minimumTwoByteScalar)
          return DecodedScalar{first, singleByteLength};
        return DecodedScalar{value, twoByteLength};
      }

      if ((first & utf8ThreeByteTagMask) == utf8ThreeByteTag) {
        const auto second = continuation(offset + 1);
        const auto third = continuation(offset + 2);
        if (!second || !third)
          return DecodedScalar{first, singleByteLength};
        const char32_t value =
            ((first & utf8ThreeBytePayloadMask) << utf8ThreeByteLeadShift) |
            ((*second & utf8ContinuationPayloadMask) << utf8ContinuationPayloadBits) |
            (*third & utf8ContinuationPayloadMask);
        if (value < minimumThreeByteScalar || (value >= surrogateMin && value <= surrogateMax))
          return DecodedScalar{first, singleByteLength};
        return DecodedScalar{value, threeByteLength};
      }

      if ((first & utf8FourByteTagMask) == utf8FourByteTag) {
        const auto second = continuation(offset + 1);
        const auto third = continuation(offset + 2);
        const auto fourth = continuation(offset + 3);
        if (!second || !third || !fourth)
          return DecodedScalar{first, singleByteLength};
        const char32_t value =
            ((first & utf8FourBytePayloadMask) << utf8FourByteLeadShift) |
            ((*second & utf8ContinuationPayloadMask) << utf8FourByteSecondShift) |
            ((*third & utf8ContinuationPayloadMask) << utf8ContinuationPayloadBits) |
            (*fourth & utf8ContinuationPayloadMask);
        if (value < minimumFourByteScalar || value > maximumUnicodeScalar)
          return DecodedScalar{first, singleByteLength};
        return DecodedScalar{value, fourByteLength};
      }

      return DecodedScalar{first, singleByteLength};
    }

    std::optional<DecodedScalar> scalarBefore(std::string_view text, std::size_t offset)
    {
      if (offset == 0 || offset > text.size())
        return std::nullopt;

      std::size_t scalarOffset = offset - 1;
      while (scalarOffset > 0 &&
             isContinuationByte(static_cast<unsigned char>(text[scalarOffset]))) {
        --scalarOffset;
      }

      return decodeUtf8At(text, scalarOffset);
    }

    std::optional<DecodedScalar> scalarAt(std::string_view text, std::size_t offset)
    {
      if (offset >= text.size())
        return std::nullopt;
      return decodeUtf8At(text, offset);
    }

    bool hasBoundaries(std::string_view text, std::size_t offset, std::size_t length,
                        bool (*isInsideBoundary)(char32_t))
    {
      const auto left = scalarBefore(text, offset);
      if (left && isInsideBoundary(left->value))
        return false;

      const auto right = scalarAt(text, offset + length);
      if (right && isInsideBoundary(right->value))
        return false;

      return true;
    }

    char32_t simpleCaseFold(char32_t scalar)
    {
      if (scalar >= asciiUppercaseMin && scalar <= asciiUppercaseMax)
        return scalar + lowercaseCodepointDelta;

      if ((scalar >= latinUppercaseMin &&
           scalar <= latinUppercaseBeforeMultiplicationSignMax) ||
          (scalar >= latinUppercaseAfterMultiplicationSignMin &&
           scalar <= latinUppercaseMax))
        return scalar + lowercaseCodepointDelta;

      return scalar;
    }

    bool sameScalar(DecodedScalar left, DecodedScalar right, const SearchOptions& options)
    {
      if (options.caseSensitive)
        return left.value == right.value;
      return simpleCaseFold(left.value) == simpleCaseFold(right.value);
    }

    std::optional<std::size_t> literalMatchLengthAt(std::string_view text,
                                                       std::string_view expression,
                                                       std::size_t textOffset,
                                                       const SearchOptions& options)
    {
      std::size_t textIndex = textOffset;
      std::size_t expressionIndex = 0;

      while (expressionIndex < expression.size()) {
        if (textIndex >= text.size())
          return std::nullopt;

        const auto textScalar = decodeUtf8At(text, textIndex);
        const auto expressionScalar = decodeUtf8At(expression, expressionIndex);
        if (!sameScalar(textScalar, expressionScalar, options))
          return std::nullopt;

        textIndex += textScalar.byteLength;
        expressionIndex += expressionScalar.byteLength;
      }

      return textIndex - textOffset;
    }

  } // namespace

  std::vector<MatchPosition> findAllLiterals(std::string_view text, std::string_view expression,
                                               const SearchOptions& options)
  {
    std::vector<MatchPosition> matches;
    if (expression.empty())
      return matches;

    const auto matchesAt = [&](std::size_t offset) -> std::optional<std::size_t> {
      const auto matchLength = literalMatchLengthAt(text, expression, offset, options);
      if (!matchLength)
        return std::nullopt;
      if (!matchesRequestedBoundaries(text, MatchPosition{offset, *matchLength}, options))
        return std::nullopt;
      return matchLength;
    };

    for (std::size_t offset = 0; offset < text.size();) {
      const auto matchLength = matchesAt(offset);
      if (matchLength)
        matches.push_back(MatchPosition{offset, *matchLength});

      const auto scalar = decodeUtf8At(text, offset);
      offset += scalar.byteLength;
    }

    return matches;
  }

  std::optional<MatchPosition> findLiteral(std::string_view text, std::string_view expression,
                                            const SearchOptions& options)
  {
    const auto matches = findAllLiterals(text, expression, options);
    if (matches.empty())
      return std::nullopt;
    return matches.front();
  }

  bool matchesRequestedBoundaries(std::string_view text, MatchPosition match,
                                    const SearchOptions& options)
  {
    return (!options.wholeWord ||
            hasBoundaries(text, match.offset, match.length, isNaturalWordScalar)) &&
           (!options.wholeIdentifier ||
            hasBoundaries(text, match.offset, match.length, isCodeIdentifierScalar));
  }

  bool looksBinary(std::string_view sample)
  {
    return std::ranges::find(sample, '\0') != sample.end();
  }

} // namespace uburu::text
