#pragma once

#include "shared/types/domain-types.hpp"

#include <filesystem>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <system_error>

namespace uburu::text
{

  enum class TextReadStatus
  {
    completed,
    cancelled,
    openFailed,
    readFailed,
    binarySkipped,
    invalidEncoding,
    lineTooLong
  };

  enum class LineEnding
  {
    none,
    lf,
    crlf,
    cr
  };

  /**
   * Carries one decoded line with source position metadata.
   */
  struct TextLine
  {
    std::string text;
    std::size_t lineNumber{0};
    std::uintmax_t byteOffset{0};
    LineEnding ending{LineEnding::none};
  };

  /**
   * Summarizes text decoding and line streaming.
   */
  struct TextReadSummary
  {
    TextReadStatus status{TextReadStatus::completed};
    TextEncoding encoding{TextEncoding::utf8};
    std::error_code error;
    std::size_t linesRead{0};
    bool hadBom{false};
    bool hadInvalidSequences{false};
  };

  using TextLineSink = std::function<bool(const TextLine&)>;

  /**
   * Streams decoded text lines without requiring callers to load the whole file.
   */
  [[nodiscard]]
  TextReadSummary readTextFileLines(
    const std::filesystem::path& path,
    const SearchOptions& options,
    const TextLineSink& sink,
    std::stop_token stopToken = {});

  /**
   * Detects binary-looking samples after encoding detection has already run.
   */
  [[nodiscard]]
  bool sampleLooksBinary(std::string_view sample, TextEncoding encoding);

  /**
   * Converts a UTF-8 byte offset into a user-facing one-based visual column.
   */
  [[nodiscard]]
  std::size_t visualColumnForByteOffset(std::string_view utf8Text, std::size_t byteOffset);

} // namespace uburu::text
