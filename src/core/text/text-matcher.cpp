#include "core/text/text-matcher.hpp"

#include <algorithm>
#include <cstdint>

namespace uburu::text
{
  namespace
  {

    constexpr std::size_t single_byte_length = 1;
    constexpr std::size_t two_byte_length = 2;
    constexpr std::size_t three_byte_length = 3;
    constexpr std::size_t four_byte_length = 4;

    constexpr unsigned char ascii_max = 0x7FU;
    constexpr unsigned char utf8_continuation_tag_mask = 0b1100'0000U;
    constexpr unsigned char utf8_continuation_tag = 0b1000'0000U;
    constexpr unsigned char utf8_continuation_payload_mask = 0b0011'1111U;

    constexpr unsigned char utf8_two_byte_tag_mask = 0b1110'0000U;
    constexpr unsigned char utf8_two_byte_tag = 0b1100'0000U;
    constexpr unsigned char utf8_two_byte_payload_mask = 0b0001'1111U;

    constexpr unsigned char utf8_three_byte_tag_mask = 0b1111'0000U;
    constexpr unsigned char utf8_three_byte_tag = 0b1110'0000U;
    constexpr unsigned char utf8_three_byte_payload_mask = 0b0000'1111U;

    constexpr unsigned char utf8_four_byte_tag_mask = 0b1111'1000U;
    constexpr unsigned char utf8_four_byte_tag = 0b1111'0000U;
    constexpr unsigned char utf8_four_byte_payload_mask = 0b0000'0111U;

    constexpr std::size_t utf8_continuation_payload_bits = 6;
    constexpr std::size_t utf8_three_byte_lead_shift = 12;
    constexpr std::size_t utf8_four_byte_lead_shift = 18;
    constexpr std::size_t utf8_four_byte_second_shift = 12;

    constexpr char32_t minimum_two_byte_scalar = 0x80U;
    constexpr char32_t minimum_three_byte_scalar = 0x800U;
    constexpr char32_t minimum_four_byte_scalar = 0x10000U;
    constexpr char32_t maximum_unicode_scalar = 0x10FFFFU;
    constexpr char32_t surrogate_min = 0xD800U;
    constexpr char32_t surrogate_max = 0xDFFFU;

    constexpr char32_t ascii_uppercase_min = U'A';
    constexpr char32_t ascii_uppercase_max = U'Z';
    constexpr char32_t ascii_lowercase_min = U'a';
    constexpr char32_t ascii_lowercase_max = U'z';
    constexpr char32_t ascii_digit_min = U'0';
    constexpr char32_t ascii_digit_max = U'9';
    constexpr char32_t ascii_identifier_connector = U'_';

    constexpr char32_t latin_uppercase_min = 0x00C0U;
    constexpr char32_t latin_uppercase_before_multiplication_sign_max = 0x00D6U;
    constexpr char32_t latin_uppercase_after_multiplication_sign_min = 0x00D8U;
    constexpr char32_t latin_uppercase_max = 0x00DEU;
    constexpr char32_t latin_lowercase_min = 0x00E0U;
    constexpr char32_t latin_lowercase_before_division_sign_max = 0x00F6U;
    constexpr char32_t latin_lowercase_after_division_sign_min = 0x00F8U;
    constexpr char32_t latin_lowercase_max = 0x00FFU;
    constexpr char32_t lowercase_codepoint_delta = 32U;

    struct DecodedScalar
    {
      char32_t value{0};
      std::size_t byte_length{single_byte_length};
    };

    bool is_ascii_letter(char32_t scalar)
    {
      return (scalar >= ascii_uppercase_min && scalar <= ascii_uppercase_max) ||
             (scalar >= ascii_lowercase_min && scalar <= ascii_lowercase_max);
    }

    bool is_ascii_digit(char32_t scalar)
    {
      return scalar >= ascii_digit_min && scalar <= ascii_digit_max;
    }

    bool is_latin_letter(char32_t scalar)
    {
      return (scalar >= latin_uppercase_min &&
              scalar <= latin_uppercase_before_multiplication_sign_max) ||
             (scalar >= latin_uppercase_after_multiplication_sign_min &&
              scalar <= latin_uppercase_max) ||
             (scalar >= latin_lowercase_min &&
              scalar <= latin_lowercase_before_division_sign_max) ||
             (scalar >= latin_lowercase_after_division_sign_min && scalar <= latin_lowercase_max);
    }

    bool is_natural_word_scalar(char32_t scalar)
    {
      return is_ascii_letter(scalar) || is_ascii_digit(scalar) || is_latin_letter(scalar);
    }

    bool is_code_identifier_scalar(char32_t scalar)
    {
      return is_ascii_letter(scalar) || is_ascii_digit(scalar) ||
             scalar == ascii_identifier_connector;
    }

    bool is_continuation_byte(unsigned char byte)
    {
      return (byte & utf8_continuation_tag_mask) == utf8_continuation_tag;
    }

    DecodedScalar decode_utf8_at(std::string_view text, std::size_t offset)
    {
      const auto first = static_cast<unsigned char>(text[offset]);
      if (first <= ascii_max)
        return DecodedScalar{first, single_byte_length};

      auto continuation = [&](std::size_t position) -> std::optional<unsigned char> {
        if (position >= text.size())
          return std::nullopt;
        const auto byte = static_cast<unsigned char>(text[position]);
        if (!is_continuation_byte(byte))
          return std::nullopt;
        return byte;
      };

      if ((first & utf8_two_byte_tag_mask) == utf8_two_byte_tag) {
        const auto second = continuation(offset + 1);
        if (!second)
          return DecodedScalar{first, single_byte_length};
        const char32_t value =
            ((first & utf8_two_byte_payload_mask) << utf8_continuation_payload_bits) |
            (*second & utf8_continuation_payload_mask);
        if (value < minimum_two_byte_scalar)
          return DecodedScalar{first, single_byte_length};
        return DecodedScalar{value, two_byte_length};
      }

      if ((first & utf8_three_byte_tag_mask) == utf8_three_byte_tag) {
        const auto second = continuation(offset + 1);
        const auto third = continuation(offset + 2);
        if (!second || !third)
          return DecodedScalar{first, single_byte_length};
        const char32_t value =
            ((first & utf8_three_byte_payload_mask) << utf8_three_byte_lead_shift) |
            ((*second & utf8_continuation_payload_mask) << utf8_continuation_payload_bits) |
            (*third & utf8_continuation_payload_mask);
        if (value < minimum_three_byte_scalar || (value >= surrogate_min && value <= surrogate_max))
          return DecodedScalar{first, single_byte_length};
        return DecodedScalar{value, three_byte_length};
      }

      if ((first & utf8_four_byte_tag_mask) == utf8_four_byte_tag) {
        const auto second = continuation(offset + 1);
        const auto third = continuation(offset + 2);
        const auto fourth = continuation(offset + 3);
        if (!second || !third || !fourth)
          return DecodedScalar{first, single_byte_length};
        const char32_t value =
            ((first & utf8_four_byte_payload_mask) << utf8_four_byte_lead_shift) |
            ((*second & utf8_continuation_payload_mask) << utf8_four_byte_second_shift) |
            ((*third & utf8_continuation_payload_mask) << utf8_continuation_payload_bits) |
            (*fourth & utf8_continuation_payload_mask);
        if (value < minimum_four_byte_scalar || value > maximum_unicode_scalar)
          return DecodedScalar{first, single_byte_length};
        return DecodedScalar{value, four_byte_length};
      }

      return DecodedScalar{first, single_byte_length};
    }

    std::optional<DecodedScalar> scalar_before(std::string_view text, std::size_t offset)
    {
      if (offset == 0 || offset > text.size())
        return std::nullopt;

      std::size_t scalar_offset = offset - 1;
      while (scalar_offset > 0 &&
             is_continuation_byte(static_cast<unsigned char>(text[scalar_offset])))
      {
        --scalar_offset;
      }

      return decode_utf8_at(text, scalar_offset);
    }

    std::optional<DecodedScalar> scalar_at(std::string_view text, std::size_t offset)
    {
      if (offset >= text.size())
        return std::nullopt;
      return decode_utf8_at(text, offset);
    }

    bool has_boundaries(std::string_view text, std::size_t offset, std::size_t length,
                        bool (*is_inside_boundary)(char32_t))
    {
      const auto left = scalar_before(text, offset);
      if (left && is_inside_boundary(left->value))
        return false;

      const auto right = scalar_at(text, offset + length);
      if (right && is_inside_boundary(right->value))
        return false;

      return true;
    }

    char32_t simple_case_fold(char32_t scalar)
    {
      if (scalar >= ascii_uppercase_min && scalar <= ascii_uppercase_max)
        return scalar + lowercase_codepoint_delta;

      if ((scalar >= latin_uppercase_min &&
           scalar <= latin_uppercase_before_multiplication_sign_max) ||
          (scalar >= latin_uppercase_after_multiplication_sign_min &&
           scalar <= latin_uppercase_max))
        return scalar + lowercase_codepoint_delta;

      return scalar;
    }

    bool same_scalar(DecodedScalar left, DecodedScalar right, const SearchOptions& options)
    {
      if (options.case_sensitive)
        return left.value == right.value;
      return simple_case_fold(left.value) == simple_case_fold(right.value);
    }

    std::optional<std::size_t> literal_match_length_at(std::string_view text,
                                                       std::string_view expression,
                                                       std::size_t text_offset,
                                                       const SearchOptions& options)
    {
      std::size_t text_index = text_offset;
      std::size_t expression_index = 0;

      while (expression_index < expression.size()) {
        if (text_index >= text.size())
          return std::nullopt;

        const auto text_scalar = decode_utf8_at(text, text_index);
        const auto expression_scalar = decode_utf8_at(expression, expression_index);
        if (!same_scalar(text_scalar, expression_scalar, options))
          return std::nullopt;

        text_index += text_scalar.byte_length;
        expression_index += expression_scalar.byte_length;
      }

      return text_index - text_offset;
    }

  } // namespace

  std::vector<MatchPosition> find_all_literals(std::string_view text, std::string_view expression,
                                               const SearchOptions& options)
  {
    std::vector<MatchPosition> matches;
    if (expression.empty())
      return matches;

    const auto matches_at = [&](std::size_t offset) -> std::optional<std::size_t> {
      const auto match_length = literal_match_length_at(text, expression, offset, options);
      if (!match_length)
        return std::nullopt;
      if (options.whole_word &&
          !has_boundaries(text, offset, *match_length, is_natural_word_scalar))
        return std::nullopt;
      if (options.whole_identifier &&
          !has_boundaries(text, offset, *match_length, is_code_identifier_scalar))
        return std::nullopt;
      return match_length;
    };

    for (std::size_t offset = 0; offset < text.size();) {
      const auto match_length = matches_at(offset);
      if (match_length)
        matches.push_back(MatchPosition{offset, *match_length});

      const auto scalar = decode_utf8_at(text, offset);
      offset += scalar.byte_length;
    }

    return matches;
  }

  std::optional<MatchPosition> find_literal(std::string_view text, std::string_view expression,
                                            const SearchOptions& options)
  {
    const auto matches = find_all_literals(text, expression, options);
    if (matches.empty())
      return std::nullopt;
    return matches.front();
  }

  bool looks_binary(std::string_view sample)
  {
    return std::ranges::find(sample, '\0') != sample.end();
  }

} // namespace uburu::text
