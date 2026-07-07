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

  struct TextLine
  {
    std::string text;
    std::size_t lineNumber{0};
    std::uintmax_t byteOffset{0};
    LineEnding ending{LineEnding::none};
  };

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

  [[nodiscard]]
  TextReadSummary readTextFileLines(const std::filesystem::path& path,
                                    const SearchOptions& options,
                                    const TextLineSink& sink,
                                    std::stop_token stop_token = {});

  [[nodiscard]]
  bool sampleLooksBinary(std::string_view sample, TextEncoding encoding);
  [[nodiscard]]
  std::size_t visualColumnForByteOffset(std::string_view utf8Text, std::size_t byteOffset);

} // namespace uburu::text
