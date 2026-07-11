#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace uburu::document::xml
{

  /**
   * Converts ASCII letters to lowercase without applying locale-sensitive Unicode rules.
   */
  [[nodiscard]]
  std::string lowerAscii(std::string_view text);

  /**
   * Checks the XML whitespace characters handled by the lightweight document parsers.
   */
  [[nodiscard]]
  bool isAsciiWhitespace(char character);

  /**
   * Returns true when a raw tag body represents a closing tag.
   */
  [[nodiscard]]
  bool isClosingTag(std::string_view tagContent);

  /**
   * Returns true when a raw tag body represents a self-closing tag.
   */
  [[nodiscard]]
  bool isSelfClosingTag(std::string_view tagContent);

  /**
   * Extracts the local lowercase element or attribute name, ignoring an optional namespace prefix.
   */
  [[nodiscard]]
  std::string localNameFromTag(std::string_view tagContent);

  /**
   * Reads and entity-decodes a quoted attribute value from a raw tag body.
   */
  [[nodiscard]]
  std::optional<std::string> attributeValue(std::string_view tagContent, std::string_view attributeName);

  /**
   * Appends XML text with the standard entities decoded into UTF-8.
   */
  void appendDecodedText(std::string_view text, std::string& output);

  /**
   * Removes trailing ASCII whitespace from extracted text.
   */
  void trimTrailingAsciiWhitespace(std::string& text);

} // namespace uburu::document::xml
