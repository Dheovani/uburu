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
    open_failed,
    read_failed,
    binary_skipped,
    invalid_encoding,
    line_too_long
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
    std::size_t line_number{0};
    std::uintmax_t byte_offset{0};
    LineEnding ending{LineEnding::none};
  };

  struct TextReadSummary
  {
    TextReadStatus status{TextReadStatus::completed};
    TextEncoding encoding{TextEncoding::utf8};
    std::error_code error;
    std::size_t lines_read{0};
    bool had_bom{false};
    bool had_invalid_sequences{false};
  };

  using TextLineSink = std::function<bool(const TextLine&)>;

  [[nodiscard]] TextReadSummary read_text_file_lines(const std::filesystem::path& path,
                                                     const SearchOptions& options,
                                                     const TextLineSink& sink,
                                                     std::stop_token stop_token = {});

  [[nodiscard]] bool sample_looks_binary(std::string_view sample, TextEncoding encoding);
  [[nodiscard]] std::size_t visual_column_for_byte_offset(std::string_view utf8_text,
                                                          std::size_t byte_offset);

} // namespace uburu::text
