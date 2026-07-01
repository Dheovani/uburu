#include "core/text/regex-matcher.hpp"

#include <chrono>
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

    constexpr int regexBackendUnavailableCode = -1;
    constexpr unsigned char utf8ContinuationTagMask = 0b1100'0000U;
    constexpr unsigned char utf8ContinuationTag = 0b1000'0000U;
    constexpr std::size_t pcreErrorMessageBufferSize = 256;

#ifdef UBURU_HAS_PCRE2
    struct RegexDeadline
    {
      std::chrono::steady_clock::time_point expiresAt;
    };
#endif

    bool isContinuationByte(unsigned char byte)
    {
      return (byte & utf8ContinuationTagMask) == utf8ContinuationTag;
    }

    std::size_t nextUtf8Offset(std::string_view text, std::size_t offset)
    {
      if (offset >= text.size())
        return text.size();
      ++offset;
      while (offset < text.size() && isContinuationByte(static_cast<unsigned char>(text[offset])))
        ++offset;
      return offset;
    }

#ifdef UBURU_HAS_PCRE2
    pcre2_code* asPcreCode(void* code)
    {
      return static_cast<pcre2_code*>(code);
    }

    int timeoutCallout(pcre2_callout_block*, void* data)
    {
      const auto* deadline = static_cast<const RegexDeadline*>(data);

      if (std::chrono::steady_clock::now() >= deadline->expiresAt)
        return PCRE2_ERROR_CALLOUT;

      return 0;
    }

    RegexMatchStatus statusFromPcreError(int code)
    {
      if (code == PCRE2_ERROR_MATCHLIMIT || code == PCRE2_ERROR_DEPTHLIMIT || code == PCRE2_ERROR_HEAPLIMIT)
        return RegexMatchStatus::resourceLimitExceeded;

      if (code == PCRE2_ERROR_CALLOUT)
        return RegexMatchStatus::timedOut;

      return RegexMatchStatus::internalError;
    }

    std::string pcreErrorMessage(int code)
    {
      PCRE2_UCHAR buffer[pcreErrorMessageBufferSize]{};
      const auto length = pcre2_get_error_message(code, buffer, pcreErrorMessageBufferSize);
      if (length <= 0)
        return {};
      return std::string{reinterpret_cast<const char*>(buffer), static_cast<std::size_t>(length)};
    }
#endif

  } // namespace

  RegexMatcher::RegexMatcher(void* code, SearchOptions options, bool jitWasEnabled)
    : code(code), options(std::move(options)), jitWasEnabled(jitWasEnabled)
  {}

  RegexMatcher::RegexMatcher(RegexMatcher&& other) noexcept
    : code(std::exchange(other.code, nullptr)),
      options(std::move(other.options)),
      jitWasEnabled(std::exchange(other.jitWasEnabled, false))
  {}

  RegexMatcher& RegexMatcher::operator=(RegexMatcher&& other) noexcept
  {
    if (this == &other)
      return *this;
    reset();
    code = std::exchange(other.code, nullptr);
    options = std::move(other.options);
    jitWasEnabled = std::exchange(other.jitWasEnabled, false);
    return *this;
  }

  RegexMatcher::~RegexMatcher()
  {
    reset();
  }

  RegexMatchResult RegexMatcher::findAll(std::string_view text) const
  {
    RegexMatchResult result;

#ifdef UBURU_HAS_PCRE2
    if (code == nullptr)
      return result;

    auto* matchData = pcre2_match_data_create_from_pattern(asPcreCode(code), nullptr);
    if (matchData == nullptr) {
      result.status = RegexMatchStatus::internalError;

      return result;
    }

    auto* matchContext = pcre2_match_context_create(nullptr);
    if (matchContext == nullptr) {
      pcre2_match_data_free(matchData);
      result.status = RegexMatchStatus::internalError;

      return result;
    }

    pcre2_set_match_limit(matchContext, options.regexMatchLimit);
    pcre2_set_depth_limit(matchContext, options.regexDepthLimit);
    pcre2_set_heap_limit(matchContext, options.regexHeapLimitKib);

    RegexDeadline deadline{std::chrono::steady_clock::now() + options.regexTimeout};
    pcre2_set_callout(matchContext, timeoutCallout, &deadline);

    std::size_t startOffset = 0;
    while (startOffset <= text.size()) {
      const auto matchResult = pcre2_match(asPcreCode(code),
                                           reinterpret_cast<PCRE2_SPTR>(text.data()),
                                           text.size(),
                                           startOffset,
                                           0,
                                           matchData,
                                           matchContext);
      if (matchResult == PCRE2_ERROR_NOMATCH)
        break;
      if (matchResult < 0) {
        result.status = statusFromPcreError(matchResult);
        result.backendErrorCode = matchResult;

        break;
      }

      const auto* ovector = pcre2_get_ovector_pointer(matchData);
      const auto start = static_cast<std::size_t>(ovector[0]);
      const auto end = static_cast<std::size_t>(ovector[1]);
      const MatchPosition match{start, end - start};

      if (matchesRequestedBoundaries(text, match, options))
        result.matches.push_back(match);

      if (end > start) {
        startOffset = nextUtf8Offset(text, start);
      } else {
        startOffset = nextUtf8Offset(text, start);
        if (startOffset == start)
          break;
      }
    }

    pcre2_match_context_free(matchContext);
    pcre2_match_data_free(matchData);
#else
    (void)text;
#endif

    return result;
  }

  bool RegexMatcher::jitEnabled() const noexcept
  {
    return jitWasEnabled;
  }

  void RegexMatcher::reset() noexcept
  {
#ifdef UBURU_HAS_PCRE2
    if (code != nullptr)
      pcre2_code_free(asPcreCode(code));
#endif
    code = nullptr;
    jitWasEnabled = false;
  }

  RegexCompileResult compileRegex(std::string_view expression, const SearchOptions& options)
  {
#ifdef UBURU_HAS_PCRE2
    int errorCode = 0;
    PCRE2_SIZE errorOffset = 0;
    std::uint32_t compileOptions = PCRE2_UTF | PCRE2_UCP | PCRE2_AUTO_CALLOUT;
    if (!options.caseSensitive)
      compileOptions |= PCRE2_CASELESS;

    auto* code = pcre2_compile(reinterpret_cast<PCRE2_SPTR>(expression.data()),
                               expression.size(),
                               compileOptions,
                               &errorCode,
                               &errorOffset,
                               nullptr);
    if (code == nullptr) {
      return RegexCompileResult{.matcher = std::nullopt,
                                .error = RegexCompileError{errorCode, errorOffset, pcreErrorMessage(errorCode)}};
    }

    const bool jitWasEnabled = pcre2_jit_compile(code, PCRE2_JIT_COMPLETE) == 0;
    return RegexCompileResult{
      .matcher = RegexMatcher{code, options, jitWasEnabled},
      .error = std::nullopt,
    };
#else
    (void)expression;
    (void)options;
    return RegexCompileResult{.matcher = std::nullopt,
                              .error = RegexCompileError{regexBackendUnavailableCode, 0, "PCRE2 unavailable"}};
#endif
  }

} // namespace uburu::text
