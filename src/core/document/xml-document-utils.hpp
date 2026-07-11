#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace uburu::document::xml
{

  [[nodiscard]]
  std::string lowerAscii(std::string_view text);

  [[nodiscard]]
  bool isAsciiWhitespace(char character);

  [[nodiscard]]
  bool isClosingTag(std::string_view tagContent);

  [[nodiscard]]
  bool isSelfClosingTag(std::string_view tagContent);

  [[nodiscard]]
  std::string localNameFromTag(std::string_view tagContent);

  [[nodiscard]]
  std::optional<std::string> attributeValue(std::string_view tagContent, std::string_view attributeName);

  void appendDecodedText(std::string_view text, std::string& output);

  void trimTrailingAsciiWhitespace(std::string& text);

} // namespace uburu::document::xml
