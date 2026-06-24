#include "core/text/regex-matcher.hpp"

#include <cstdint>
#include <utility>

#ifdef UBURU_HAS_PCRE2
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#endif

namespace uburu::text
{
  namespace
  {

    constexpr int regex_backend_unavailable_code = -1;
    constexpr unsigned char utf8_continuation_tag_mask = 0b1100'0000U;
    constexpr unsigned char utf8_continuation_tag = 0b1000'0000U;
    constexpr std::size_t pcre_error_message_buffer_size = 256;

    bool is_continuation_byte(unsigned char byte)
    {
      return (byte & utf8_continuation_tag_mask) == utf8_continuation_tag;
    }

    std::size_t next_utf8_offset(std::string_view text, std::size_t offset)
    {
      if (offset >= text.size())
        return text.size();
      ++offset;
      while (offset < text.size() && is_continuation_byte(static_cast<unsigned char>(text[offset])))
        ++offset;
      return offset;
    }

#ifdef UBURU_HAS_PCRE2
    pcre2_code* as_pcre_code(void* code)
    {
      return static_cast<pcre2_code*>(code);
    }

    std::string pcre_error_message(int code)
    {
      PCRE2_UCHAR buffer[pcre_error_message_buffer_size]{};
      const auto length = pcre2_get_error_message(code, buffer, pcre_error_message_buffer_size);
      if (length <= 0)
        return {};
      return std::string{reinterpret_cast<const char*>(buffer), static_cast<std::size_t>(length)};
    }
#endif

  } // namespace

  RegexMatcher::RegexMatcher(void* code, SearchOptions options, bool jit_enabled)
      : code_(code), options_(std::move(options)), jit_enabled_(jit_enabled)
  {}

  RegexMatcher::RegexMatcher(RegexMatcher&& other) noexcept
      : code_(std::exchange(other.code_, nullptr)),
        options_(std::move(other.options_)),
        jit_enabled_(std::exchange(other.jit_enabled_, false))
  {}

  RegexMatcher& RegexMatcher::operator=(RegexMatcher&& other) noexcept
  {
    if (this == &other)
      return *this;
    reset();
    code_ = std::exchange(other.code_, nullptr);
    options_ = std::move(other.options_);
    jit_enabled_ = std::exchange(other.jit_enabled_, false);
    return *this;
  }

  RegexMatcher::~RegexMatcher()
  {
    reset();
  }

  std::vector<MatchPosition> RegexMatcher::find_all(std::string_view text) const
  {
    std::vector<MatchPosition> matches;

#ifdef UBURU_HAS_PCRE2
    if (code_ == nullptr)
      return matches;

    auto* match_data = pcre2_match_data_create_from_pattern(as_pcre_code(code_), nullptr);
    if (match_data == nullptr)
      return matches;

    std::size_t start_offset = 0;
    while (start_offset <= text.size()) {
      const auto result =
          pcre2_match(as_pcre_code(code_), reinterpret_cast<PCRE2_SPTR>(text.data()), text.size(),
                      start_offset, 0, match_data, nullptr);
      if (result == PCRE2_ERROR_NOMATCH)
        break;
      if (result < 0)
        break;

      const auto* ovector = pcre2_get_ovector_pointer(match_data);
      const auto start = static_cast<std::size_t>(ovector[0]);
      const auto end = static_cast<std::size_t>(ovector[1]);
      const MatchPosition match{start, end - start};
      if (matches_requested_boundaries(text, match, options_))
        matches.push_back(match);

      if (end > start) {
        start_offset = next_utf8_offset(text, start);
      } else {
        start_offset = next_utf8_offset(text, start);
        if (start_offset == start)
          break;
      }
    }

    pcre2_match_data_free(match_data);
#else
    (void)text;
#endif

    return matches;
  }

  bool RegexMatcher::jit_enabled() const noexcept
  {
    return jit_enabled_;
  }

  void RegexMatcher::reset() noexcept
  {
#ifdef UBURU_HAS_PCRE2
    if (code_ != nullptr)
      pcre2_code_free(as_pcre_code(code_));
#endif
    code_ = nullptr;
    jit_enabled_ = false;
  }

  RegexCompileResult compile_regex(std::string_view expression, const SearchOptions& options)
  {
#ifdef UBURU_HAS_PCRE2
    int error_code = 0;
    PCRE2_SIZE error_offset = 0;
    std::uint32_t compile_options = PCRE2_UTF | PCRE2_UCP;
    if (!options.case_sensitive)
      compile_options |= PCRE2_CASELESS;

    auto* code = pcre2_compile(reinterpret_cast<PCRE2_SPTR>(expression.data()), expression.size(),
                               compile_options, &error_code, &error_offset, nullptr);
    if (code == nullptr) {
      return RegexCompileResult{
          .matcher = std::nullopt,
          .error = RegexCompileError{error_code, error_offset, pcre_error_message(error_code)}};
    }

    const bool jit_enabled = pcre2_jit_compile(code, PCRE2_JIT_COMPLETE) == 0;
    return RegexCompileResult{
        .matcher = RegexMatcher{code, options, jit_enabled},
        .error = std::nullopt,
    };
#else
    (void)expression;
    (void)options;
    return RegexCompileResult{
        .matcher = std::nullopt,
        .error = RegexCompileError{regex_backend_unavailable_code, 0, "PCRE2 unavailable"}};
#endif
  }

} // namespace uburu::text
