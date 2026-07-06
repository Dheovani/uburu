#include "core/text/text-matcher.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace
{

  constexpr std::size_t minimumSplitInputSize = 2;
  constexpr unsigned char caseSensitiveFlag = 0b0000'0001U;
  constexpr unsigned char wholeWordFlag = 0b0000'0010U;
  constexpr unsigned char wholeIdentifierFlag = 0b0000'0100U;

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
  if (size < minimumSplitInputSize)
    return 0;

  const auto flags = data[0];
  const auto expressionSize = std::min<std::size_t>(data[1], size - minimumSplitInputSize);
  const auto* expressionStart = reinterpret_cast<const char*>(data + minimumSplitInputSize);
  const auto* textStart = expressionStart + expressionSize;
  const auto textSize = size - minimumSplitInputSize - expressionSize;

  uburu::SearchOptions options;
  options.caseSensitive = (flags & caseSensitiveFlag) != 0;
  options.wholeWord = (flags & wholeWordFlag) != 0;
  options.wholeIdentifier = (flags & wholeIdentifierFlag) != 0;

  const std::string_view expression(expressionStart, expressionSize);
  const std::string_view text(textStart, textSize);
  const auto matches = uburu::text::findAllLiterals(text, expression, options);

  for (const auto& match : matches) {
    if (match.offset > text.size())
      __builtin_trap();

    if (match.length > text.size() - match.offset)
      __builtin_trap();

    static_cast<void>(uburu::text::matchesRequestedBoundaries(text, match, options));
  }

  static_cast<void>(uburu::text::findLiteral(text, expression, options));
  static_cast<void>(uburu::text::looksBinary(text));

  return 0;
}
