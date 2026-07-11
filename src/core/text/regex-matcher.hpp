#pragma once

#include "core/text/text-matcher.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace uburu::text
{

  /**
   * Captures regex compilation errors without exposing PCRE2 types to callers.
   */
  struct RegexCompileError
  {
    int code{0};
    std::size_t offset{0};
    std::string message;
  };

  enum class RegexMatchStatus
  {
    completed,
    resourceLimitExceeded,
    timedOut,
    internalError
  };

  /**
   * Holds regex matches plus backend execution status.
   */
  struct RegexMatchResult
  {
    std::vector<MatchPosition> matches;
    RegexMatchStatus status{RegexMatchStatus::completed};
    int backendErrorCode{0};
  };

  struct RegexCompileResult;

  /**
   * Compiles a regex according to search options and configured backend limits.
   */
  [[nodiscard]]
  RegexCompileResult compileRegex(std::string_view expression, const SearchOptions& options);

  /**
   * Owns a compiled regex program and hides the backend allocation details.
   */
  class RegexMatcher final
  {
  public:
    RegexMatcher() = default;
    RegexMatcher(const RegexMatcher&) = delete;
    RegexMatcher& operator=(const RegexMatcher&) = delete;
    RegexMatcher(RegexMatcher&& other) noexcept;
    RegexMatcher& operator=(RegexMatcher&& other) noexcept;
    ~RegexMatcher();

    [[nodiscard]]
    RegexMatchResult findAll(std::string_view text) const;
    [[nodiscard]]
    bool jitEnabled() const noexcept;

  private:
    friend struct RegexCompileResult;
    friend RegexCompileResult compileRegex(std::string_view expression, const SearchOptions& options);

    explicit RegexMatcher(void* code, SearchOptions options, bool jitWasEnabled);

    void reset() noexcept;

    void* code{nullptr};
    SearchOptions options;
    bool jitWasEnabled{false};
  };

  struct RegexCompileResult
  {
    std::optional<RegexMatcher> matcher;
    std::optional<RegexCompileError> error;
  };

} // namespace uburu::text
