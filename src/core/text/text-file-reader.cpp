#include "core/text/text-file-reader.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string_view>
#include <vector>

namespace uburu::text
{
  namespace
  {

    constexpr std::size_t binary_control_sample_minimum = 32;
    constexpr std::size_t binary_control_ratio_denominator = 10;
    constexpr std::size_t binary_control_ratio_numerator = 3;
    constexpr std::size_t utf16_code_unit_size = 2;
    constexpr std::size_t utf8_bom_size = 3;
    constexpr std::size_t utf16_bom_size = 2;
    constexpr std::size_t read_block_size = 64U * 1024U;
    constexpr unsigned char utf8_bom_first = 0xEFU;
    constexpr unsigned char utf8_bom_second = 0xBBU;
    constexpr unsigned char utf8_bom_third = 0xBFU;
    constexpr unsigned char utf16_le_bom_first = 0xFFU;
    constexpr unsigned char utf16_le_bom_second = 0xFEU;
    constexpr unsigned char utf16_be_bom_first = 0xFEU;
    constexpr unsigned char utf16_be_bom_second = 0xFFU;
    constexpr unsigned char ascii_tab = 0x09U;
    constexpr unsigned char ascii_line_feed = 0x0AU;
    constexpr unsigned char ascii_carriage_return = 0x0DU;
    constexpr unsigned char ascii_space = 0x20U;
    constexpr unsigned char ascii_delete = 0x7FU;
    constexpr char32_t unicode_replacement_character = 0xFFFDU;
    constexpr char32_t unicode_maximum_scalar = 0x10FFFFU;
    constexpr char32_t utf16_high_surrogate_min = 0xD800U;
    constexpr char32_t utf16_high_surrogate_max = 0xDBFFU;
    constexpr char32_t utf16_low_surrogate_min = 0xDC00U;
    constexpr char32_t utf16_low_surrogate_max = 0xDFFFU;
    constexpr char32_t utf16_surrogate_base = 0x10000U;
    constexpr std::size_t utf16_high_surrogate_shift = 10;
    constexpr std::uint16_t utf16_low_surrogate_payload_mask = 0x03FFU;
    constexpr unsigned char utf8_one_byte_max = 0x7FU;
    constexpr char32_t utf8_two_byte_scalar_max = 0x7FFU;
    constexpr char32_t utf8_three_byte_scalar_max = 0xFFFFU;
    constexpr unsigned char utf8_two_byte_prefix = 0xC0U;
    constexpr unsigned char utf8_three_byte_prefix = 0xE0U;
    constexpr unsigned char utf8_four_byte_prefix = 0xF0U;
    constexpr unsigned char utf8_continuation_prefix = 0x80U;
    constexpr unsigned char utf8_two_byte_min = 0xC2U;
    constexpr unsigned char utf8_two_byte_max = 0xDFU;
    constexpr unsigned char utf8_three_byte_min = 0xE0U;
    constexpr unsigned char utf8_three_byte_max = 0xEFU;
    constexpr unsigned char utf8_three_byte_surrogate_lead = 0xEDU;
    constexpr unsigned char utf8_four_byte_min = 0xF0U;
    constexpr unsigned char utf8_four_byte_max = 0xF4U;
    constexpr unsigned char utf8_continuation_min = 0x80U;
    constexpr unsigned char utf8_continuation_max = 0xBFU;
    constexpr unsigned char utf8_second_after_e0_min = 0xA0U;
    constexpr unsigned char utf8_second_before_surrogate_max = 0x9FU;
    constexpr unsigned char utf8_second_after_f0_min = 0x90U;
    constexpr unsigned char utf8_second_before_max_scalar_max = 0x8FU;
    constexpr unsigned char utf8_two_byte_payload_mask = 0x1FU;
    constexpr unsigned char utf8_three_byte_payload_mask = 0x0FU;
    constexpr unsigned char utf8_four_byte_payload_mask = 0x07U;
    constexpr unsigned char utf8_continuation_payload_mask = 0x3FU;
    constexpr std::size_t utf8_continuation_payload_bits = 6;
    constexpr std::size_t utf8_three_byte_lead_shift = 12;
    constexpr std::size_t utf8_four_byte_lead_shift = 18;
    constexpr std::size_t utf8_four_byte_second_shift = 12;
    constexpr char32_t latin1_control_c1_min = 0x80U;
    constexpr char32_t latin1_control_c1_max = 0x9FU;

    struct EncodingDetection
    {
      TextEncoding encoding{TextEncoding::utf8};
      std::size_t bom_size{0};
      bool had_bom{false};
    };

    struct Utf8DecodeResult
    {
      char32_t scalar{unicode_replacement_character};
      std::size_t bytes_consumed{1};
      bool valid{false};
    };

    struct LineEmitter
    {
      const SearchOptions& options;
      const TextLineSink& sink;
      TextReadSummary& summary;
      std::string line;
      std::size_t line_number{0};
      std::uintmax_t line_offset{0};
      std::uintmax_t next_byte_offset{0};

      [[nodiscard]] bool append(char scalar, std::uintmax_t byte_width)
      {
        if (line.empty())
          line_offset = next_byte_offset;

        line.push_back(scalar);
        next_byte_offset += byte_width;

        return line.size() <= options.maximum_line_length;
      }

      [[nodiscard]] bool append_utf8(std::string_view utf8, std::uintmax_t byte_width)
      {
        if (line.empty())
          line_offset = next_byte_offset;

        line.append(utf8);
        next_byte_offset += byte_width;

        return line.size() <= options.maximum_line_length;
      }

      [[nodiscard]] bool emit(LineEnding ending, std::uintmax_t ending_byte_width)
      {
        ++line_number;
        ++summary.lines_read;

        const TextLine text_line{
            .text = line, .line_number = line_number, .byte_offset = line_offset, .ending = ending};
        if (!sink(text_line))
          return false;

        line.clear();
        next_byte_offset += ending_byte_width;
        line_offset = next_byte_offset;

        return true;
      }

      [[nodiscard]] bool finish()
      {
        if (line.empty())
          return true;

        return emit(LineEnding::none, 0);
      }
    };

    bool is_utf8_continuation(unsigned char byte)
    {
      return byte >= utf8_continuation_min && byte <= utf8_continuation_max;
    }

    EncodingDetection detect_encoding(std::string_view sample, TextEncoding fallback_encoding)
    {
      if (sample.size() >= utf8_bom_size &&
          static_cast<unsigned char>(sample[0]) == utf8_bom_first &&
          static_cast<unsigned char>(sample[1]) == utf8_bom_second &&
          static_cast<unsigned char>(sample[2]) == utf8_bom_third)
        return {TextEncoding::utf8, utf8_bom_size, true};

      if (sample.size() >= utf16_bom_size &&
          static_cast<unsigned char>(sample[0]) == utf16_le_bom_first &&
          static_cast<unsigned char>(sample[1]) == utf16_le_bom_second)
        return {TextEncoding::utf16_le, utf16_bom_size, true};

      if (sample.size() >= utf16_bom_size &&
          static_cast<unsigned char>(sample[0]) == utf16_be_bom_first &&
          static_cast<unsigned char>(sample[1]) == utf16_be_bom_second)
        return {TextEncoding::utf16_be, utf16_bom_size, true};

      return {fallback_encoding == TextEncoding::utf16_le ||
              fallback_encoding == TextEncoding::utf16_be ||
              fallback_encoding == TextEncoding::latin1
                ? fallback_encoding
                : TextEncoding::utf8,
              0, false};
    }

    bool is_binary_control_byte(unsigned char byte)
    {
      if (byte == ascii_tab || byte == ascii_line_feed || byte == ascii_carriage_return)
        return false;

      return byte < ascii_space || byte == ascii_delete;
    }

    std::string read_sample(std::ifstream& stream, std::size_t sample_size)
    {
      std::string sample(sample_size, '\0');
      stream.read(sample.data(), static_cast<std::streamsize>(sample.size()));
      sample.resize(static_cast<std::size_t>(stream.gcount()));
      stream.clear();
      stream.seekg(0, std::ios::beg);

      return sample;
    }

    void append_utf8_scalar(char32_t scalar, std::string& output)
    {
      if (scalar <= utf8_one_byte_max) {
        output.push_back(static_cast<char>(scalar));

        return;
      }

      if (scalar <= utf8_two_byte_scalar_max) {
        output.push_back(
          static_cast<char>(utf8_two_byte_prefix | (scalar >> utf8_continuation_payload_bits)));
        output.push_back(
          static_cast<char>(utf8_continuation_prefix | (scalar & utf8_continuation_payload_mask)));

        return;
      }

      if (scalar <= utf8_three_byte_scalar_max) {
        output.push_back(
            static_cast<char>(utf8_three_byte_prefix | (scalar >> utf8_three_byte_lead_shift)));
        output.push_back(static_cast<char>(
            utf8_continuation_prefix |
            ((scalar >> utf8_continuation_payload_bits) & utf8_continuation_payload_mask)));
        output.push_back(static_cast<char>(utf8_continuation_prefix |
                                           (scalar & utf8_continuation_payload_mask)));

        return;
      }

      output.push_back(
        static_cast<char>(utf8_four_byte_prefix | (scalar >> utf8_four_byte_lead_shift)));
      output.push_back(static_cast<char>(
          utf8_continuation_prefix | ((scalar >> utf8_four_byte_second_shift) & utf8_continuation_payload_mask)));
      output.push_back(static_cast<char>(
          utf8_continuation_prefix | ((scalar >> utf8_continuation_payload_bits) & utf8_continuation_payload_mask)));
      output.push_back(
        static_cast<char>(utf8_continuation_prefix | (scalar & utf8_continuation_payload_mask)));
    }

    std::string utf8_from_scalar(char32_t scalar)
    {
      std::string output;
      append_utf8_scalar(scalar, output);

      return output;
    }

    Utf8DecodeResult decode_utf8_at(std::string_view text, std::size_t offset)
    {
      const auto first = static_cast<unsigned char>(text[offset]);
      if (first <= utf8_one_byte_max)
        return {.scalar = first, .bytes_consumed = 1, .valid = true};

      const auto has = [&](std::size_t index) { return offset + index < text.size(); };

      const auto byte = [&](std::size_t index) {
        return static_cast<unsigned char>(text[offset + index]);
      };

      if (first >= utf8_two_byte_min && first <= utf8_two_byte_max && has(1) &&
          is_utf8_continuation(byte(1))) {
        const char32_t scalar =
            ((first & utf8_two_byte_payload_mask) << utf8_continuation_payload_bits) |
            (byte(1) & utf8_continuation_payload_mask);

        return {.scalar = scalar, .bytes_consumed = 2, .valid = true};
      }

      if (first >= utf8_three_byte_min && first <= utf8_three_byte_max && has(2) &&
          is_utf8_continuation(byte(1)) && is_utf8_continuation(byte(2))) {
        if ((first == utf8_three_byte_min && byte(1) < utf8_second_after_e0_min) ||
            (first == utf8_three_byte_surrogate_lead && byte(1) > utf8_second_before_surrogate_max))
          return {};

        const char32_t scalar =
            ((first & utf8_three_byte_payload_mask) << utf8_three_byte_lead_shift) |
            ((byte(1) & utf8_continuation_payload_mask) << utf8_continuation_payload_bits) |
            (byte(2) & utf8_continuation_payload_mask);

        return {.scalar = scalar, .bytes_consumed = 3, .valid = true};
      }

      if (first >= utf8_four_byte_min && first <= utf8_four_byte_max && has(3) &&
          is_utf8_continuation(byte(1)) && is_utf8_continuation(byte(2)) &&
          is_utf8_continuation(byte(3))) {
        if ((first == utf8_four_byte_min && byte(1) < utf8_second_after_f0_min) ||
            (first == utf8_four_byte_max && byte(1) > utf8_second_before_max_scalar_max))
          return {};

        const char32_t scalar =
            ((first & utf8_four_byte_payload_mask) << utf8_four_byte_lead_shift) |
            ((byte(1) & utf8_continuation_payload_mask) << utf8_four_byte_second_shift) |
            ((byte(2) & utf8_continuation_payload_mask) << utf8_continuation_payload_bits) |
            (byte(3) & utf8_continuation_payload_mask);
        if (scalar > unicode_maximum_scalar)
          return {};

        return {.scalar = scalar, .bytes_consumed = 4, .valid = true};
      }

      return {};
    }

    std::uint16_t read_utf16_code_unit(std::string_view bytes, std::size_t offset,
                                       TextEncoding encoding)
    {
      const auto first = static_cast<unsigned char>(bytes[offset]);
      const auto second = static_cast<unsigned char>(bytes[offset + 1]);

      if (encoding == TextEncoding::utf16_le)
        return static_cast<std::uint16_t>(first | (second << 8U));

      return static_cast<std::uint16_t>((first << 8U) | second);
    }

    bool process_decoded_scalar(LineEmitter& emitter, char32_t scalar,
                                std::uintmax_t original_byte_width)
    {
      if (scalar == U'\n')
        return emitter.emit(LineEnding::lf, original_byte_width);

      if (scalar == U'\r')
        return emitter.emit(LineEnding::cr, original_byte_width);

      return emitter.append_utf8(utf8_from_scalar(scalar), original_byte_width);
    }

    TextReadStatus decode_utf8_lines(std::ifstream& stream, const SearchOptions& options,
                                     TextReadSummary& summary, const TextLineSink& sink,
                                     std::stop_token stop_token, std::size_t bom_size)
    {
      LineEmitter emitter{.options = options,
                          .sink = sink,
                          .summary = summary,
                          .line = {},
                          .line_number = 0,
                          .line_offset = 0,
                          .next_byte_offset = 0};
      emitter.next_byte_offset = bom_size;
      stream.seekg(static_cast<std::streamoff>(bom_size), std::ios::beg);

      std::string pending;
      std::vector<char> block(read_block_size);
      while (!stop_token.stop_requested()) {
        stream.read(block.data(), static_cast<std::streamsize>(block.size()));
        const auto bytes_read = static_cast<std::size_t>(stream.gcount());
        if (bytes_read == 0)
          break;

        pending.append(block.data(), bytes_read);
        std::size_t offset = 0;
        while (offset < pending.size()) {
          const auto decoded = decode_utf8_at(pending, offset);
          if (!decoded.valid) {
            if (pending.size() - offset < 4 && !stream.eof())
              break;

            summary.had_invalid_sequences = true;
            if (options.invalid_utf8_policy == InvalidUtf8Policy::fail)
              return TextReadStatus::invalid_encoding;

            if (options.invalid_utf8_policy == InvalidUtf8Policy::replace &&
                !process_decoded_scalar(emitter, unicode_replacement_character, 1))
              return TextReadStatus::cancelled;

            ++offset;
            continue;
          }

          if (decoded.scalar == U'\r' && offset + decoded.bytes_consumed < pending.size() &&
              pending[offset + decoded.bytes_consumed] == '\n') {
            if (!emitter.emit(LineEnding::crlf, 2))
              return TextReadStatus::cancelled;

            offset += 2;
            continue;
          }

          if (decoded.scalar == U'\r' && offset + decoded.bytes_consumed >= pending.size() &&
              !stream.eof())
            break;

          if (!process_decoded_scalar(emitter, decoded.scalar, decoded.bytes_consumed))
            return TextReadStatus::line_too_long;

          offset += decoded.bytes_consumed;
        }

        pending.erase(0, offset);
      }

      if (stop_token.stop_requested())
        return TextReadStatus::cancelled;

      if (stream.bad())
        return TextReadStatus::read_failed;

      return emitter.finish() ? TextReadStatus::completed : TextReadStatus::cancelled;
    }

    TextReadStatus decode_latin1_lines(std::ifstream& stream, const SearchOptions& options,
                                       TextReadSummary& summary, const TextLineSink& sink,
                                       std::stop_token stop_token)
    {
      LineEmitter emitter{.options = options,
                          .sink = sink,
                          .summary = summary,
                          .line = {},
                          .line_number = 0,
                          .line_offset = 0,
                          .next_byte_offset = 0};
      bool pending_cr = false;
      std::vector<char> block(read_block_size);
      while (!stop_token.stop_requested()) {
        stream.read(block.data(), static_cast<std::streamsize>(block.size()));
        const auto bytes_read = static_cast<std::size_t>(stream.gcount());
        if (bytes_read == 0)
          break;

        for (std::size_t offset = 0; offset < bytes_read; ++offset) {
          const auto byte = static_cast<unsigned char>(block[offset]);

          if (pending_cr) {
            pending_cr = false;
            if (byte == ascii_line_feed) {
              if (!emitter.emit(LineEnding::crlf, 2))
                return TextReadStatus::cancelled;

              continue;
            }

            if (!emitter.emit(LineEnding::cr, 1))
              return TextReadStatus::cancelled;
          }

          if (byte == ascii_carriage_return) {
            if (offset + 1 < bytes_read && block[offset + 1] == '\n') {
              if (!emitter.emit(LineEnding::crlf, 2))
                return TextReadStatus::cancelled;

              ++offset;
              continue;
            }

            if (offset + 1 == bytes_read) {
              pending_cr = true;

              continue;
            }

            if (!emitter.emit(LineEnding::cr, 1))
              return TextReadStatus::cancelled;

            continue;
          }

          if (byte == ascii_line_feed) {
            if (!emitter.emit(LineEnding::lf, 1))
              return TextReadStatus::cancelled;

            continue;
          }

          const auto scalar = byte >= latin1_control_c1_min && byte <= latin1_control_c1_max
                                  ? unicode_replacement_character
                                  : static_cast<char32_t>(byte);
          if (!emitter.append_utf8(utf8_from_scalar(scalar), 1))
            return TextReadStatus::line_too_long;
        }
      }

      if (stop_token.stop_requested())
        return TextReadStatus::cancelled;

      if (stream.bad())
        return TextReadStatus::read_failed;

      if (pending_cr && !emitter.emit(LineEnding::cr, 1))
        return TextReadStatus::cancelled;

      return emitter.finish() ? TextReadStatus::completed : TextReadStatus::cancelled;
    }

    TextReadStatus decode_utf16_lines(std::ifstream& stream, TextEncoding encoding,
                                      const SearchOptions& options, TextReadSummary& summary,
                                      const TextLineSink& sink, std::stop_token stop_token,
                                      std::size_t bom_size)
    {
      LineEmitter emitter{.options = options,
                          .sink = sink,
                          .summary = summary,
                          .line = {},
                          .line_number = 0,
                          .line_offset = 0,
                          .next_byte_offset = 0};
      emitter.next_byte_offset = bom_size;
      stream.seekg(static_cast<std::streamoff>(bom_size), std::ios::beg);

      std::string pending;
      std::vector<char> block(read_block_size);
      while (!stop_token.stop_requested()) {
        stream.read(block.data(), static_cast<std::streamsize>(block.size()));
        const auto bytes_read = static_cast<std::size_t>(stream.gcount());
        pending.append(block.data(), bytes_read);

        std::size_t offset = 0;
        while (offset + 1 < pending.size()) {
          if (stop_token.stop_requested())
            return TextReadStatus::cancelled;

          const auto unit = read_utf16_code_unit(pending, offset, encoding);
          char32_t scalar = unit;
          std::uintmax_t byte_width = utf16_code_unit_size;

          if (unit >= utf16_high_surrogate_min && unit <= utf16_high_surrogate_max) {
            if (offset + 3 >= pending.size()) {
              if (!stream.eof())
                break;

              summary.had_invalid_sequences = true;
              scalar = unicode_replacement_character;
            } else {
              const auto low =
                  read_utf16_code_unit(pending, offset + utf16_code_unit_size, encoding);
              if (low < utf16_low_surrogate_min || low > utf16_low_surrogate_max) {
                summary.had_invalid_sequences = true;
                scalar = unicode_replacement_character;
              } else {
                const char32_t high_payload = unit - utf16_high_surrogate_min;
                const char32_t low_payload = low & utf16_low_surrogate_payload_mask;
                scalar = utf16_surrogate_base + (high_payload << utf16_high_surrogate_shift) +
                         low_payload;
                byte_width = utf16_code_unit_size * 2;
              }
            }
          } else if (unit >= utf16_low_surrogate_min && unit <= utf16_low_surrogate_max) {
            summary.had_invalid_sequences = true;
            scalar = unicode_replacement_character;
          }

          if (scalar == U'\r' && offset + byte_width + 1 >= pending.size() && !stream.eof())
            break;

          if (scalar == U'\r' && offset + byte_width + 1 < pending.size() &&
              read_utf16_code_unit(pending, offset + static_cast<std::size_t>(byte_width),
                                   encoding) == U'\n') {
            if (!emitter.emit(LineEnding::crlf, byte_width + utf16_code_unit_size))
              return TextReadStatus::cancelled;

            offset += static_cast<std::size_t>(byte_width) + utf16_code_unit_size;
            continue;
          }

          if (!process_decoded_scalar(emitter, scalar, byte_width))
            return TextReadStatus::line_too_long;

          offset += static_cast<std::size_t>(byte_width);
        }

        pending.erase(0, offset);

        if (bytes_read == 0)
          break;
      }

      if (stop_token.stop_requested())
        return TextReadStatus::cancelled;

      if (stream.bad())
        return TextReadStatus::read_failed;

      if (!pending.empty()) {
        if (stop_token.stop_requested())
          return TextReadStatus::cancelled;

        summary.had_invalid_sequences = true;
        if (options.invalid_utf8_policy == InvalidUtf8Policy::fail)
          return TextReadStatus::invalid_encoding;
      }

      return emitter.finish() ? TextReadStatus::completed : TextReadStatus::cancelled;
    }

  } // namespace

  bool sample_looks_binary(std::string_view sample, TextEncoding encoding)
  {
    if (encoding == TextEncoding::utf16_le || encoding == TextEncoding::utf16_be)
      return false;

    if (sample.empty())
      return false;

    const auto has_nul = std::ranges::find(sample, '\0') != sample.end();
    if (has_nul)
      return true;

    if (sample.size() < binary_control_sample_minimum)
      return false;

    const auto control_count =
        static_cast<std::size_t>(std::ranges::count_if(sample, [](char value) {
          return is_binary_control_byte(static_cast<unsigned char>(value));
        }));

    return control_count * binary_control_ratio_denominator >
           sample.size() * binary_control_ratio_numerator;
  }

  std::size_t visual_column_for_byte_offset(std::string_view utf8_text, std::size_t byte_offset)
  {
    std::size_t column = 1;
    for (std::size_t offset = 0; offset < byte_offset && offset < utf8_text.size();) {
      const auto decoded = decode_utf8_at(utf8_text, offset);
      offset += decoded.valid ? decoded.bytes_consumed : 1;
      ++column;
    }

    return column;
  }

  TextReadSummary read_text_file_lines(const std::filesystem::path& path,
                                       const SearchOptions& options, const TextLineSink& sink,
                                       std::stop_token stop_token)
  {
    TextReadSummary summary;
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
      summary.status = TextReadStatus::open_failed;

      return summary;
    }

    const auto sample = read_sample(stream, options.binary_sample_size);
    const auto detected = detect_encoding(sample, options.fallback_encoding);
    summary.encoding = detected.encoding;
    summary.had_bom = detected.had_bom;

    if (!options.include_binary && sample_looks_binary(sample, detected.encoding)) {
      summary.status = TextReadStatus::binary_skipped;

      return summary;
    }

    switch (detected.encoding) {
    case TextEncoding::utf8:
      summary.status =
          decode_utf8_lines(stream, options, summary, sink, stop_token, detected.bom_size);
      break;
    case TextEncoding::utf16_le:
    case TextEncoding::utf16_be:
      summary.status = decode_utf16_lines(stream, detected.encoding, options, summary, sink,
                                          stop_token, detected.bom_size);
      break;
    case TextEncoding::latin1:
      summary.status = decode_latin1_lines(stream, options, summary, sink, stop_token);
      break;
    }

    return summary;
  }

} // namespace uburu::text
