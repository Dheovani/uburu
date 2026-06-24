#pragma once

#include "core/text/text-matcher.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace uburu::text
{

  struct RegexCompileError
  {
    int code{0};
    std::size_t offset{0};
    std::string message;
  };

  enum class RegexMatchStatus
  {
    completed,
    resource_limit_exceeded,
    timed_out,
    internal_error
  };

  struct RegexMatchResult
  {
    std::vector<MatchPosition> matches;
    RegexMatchStatus status{RegexMatchStatus::completed};
    int backend_error_code{0};
  };

  struct RegexCompileResult;

  [[nodiscard]] RegexCompileResult compile_regex(std::string_view expression,
                                                 const SearchOptions& options);

  class RegexMatcher final
  {
  public:
    RegexMatcher() = default;
    RegexMatcher(const RegexMatcher&) = delete;
    RegexMatcher& operator=(const RegexMatcher&) = delete;
    RegexMatcher(RegexMatcher&& other) noexcept;
    RegexMatcher& operator=(RegexMatcher&& other) noexcept;
    ~RegexMatcher();

    [[nodiscard]] RegexMatchResult find_all(std::string_view text) const;
    [[nodiscard]] bool jit_enabled() const noexcept;

  private:
    friend struct RegexCompileResult;
    friend RegexCompileResult compile_regex(std::string_view expression,
                                            const SearchOptions& options);

    explicit RegexMatcher(void* code, SearchOptions options, bool jit_enabled);

    void reset() noexcept;

    void* code_{nullptr};
    SearchOptions options_;
    bool jit_enabled_{false};
  };

  struct RegexCompileResult
  {
    std::optional<RegexMatcher> matcher;
    std::optional<RegexCompileError> error;
  };

} // namespace uburu::text
