#include "core/text/regex-matcher.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

using uburu::SearchMode;
using uburu::SearchOptions;
using uburu::text::compile_regex;
using uburu::text::RegexMatchStatus;

TEST_CASE("regex matching returns all PCRE2 matches")
{
  SearchOptions options;
  options.mode = SearchMode::regex;

  auto compiled = compile_regex(R"(todo\(\d+\))", options);

#ifdef UBURU_HAS_PCRE2
  REQUIRE(compiled.matcher.has_value());
  const auto result = compiled.matcher->find_all("todo(1) skip todo(42)");

  CHECK(result.status == RegexMatchStatus::completed);
  REQUIRE(result.matches.size() == 2);
  CHECK(result.matches[0].offset == 0);
  CHECK(result.matches[0].length == 7);
  CHECK(result.matches[1].offset == 13);
  CHECK(result.matches[1].length == 8);
#else
  REQUIRE_FALSE(compiled.matcher.has_value());
  REQUIRE(compiled.error.has_value());
#endif
}

TEST_CASE("regex matching uses case insensitive mode by default")
{
  SearchOptions options;
  options.mode = SearchMode::regex;

  auto compiled = compile_regex("ação", options);

#ifdef UBURU_HAS_PCRE2
  REQUIRE(compiled.matcher.has_value());
  const auto result = compiled.matcher->find_all("AÇÃO ação");

  CHECK(result.status == RegexMatchStatus::completed);
  REQUIRE(result.matches.size() == 2);
#else
  REQUIRE_FALSE(compiled.matcher.has_value());
#endif
}

TEST_CASE("regex matching can be case sensitive")
{
  SearchOptions options;
  options.mode = SearchMode::regex;
  options.case_sensitive = true;

  auto compiled = compile_regex("ação", options);

#ifdef UBURU_HAS_PCRE2
  REQUIRE(compiled.matcher.has_value());
  const auto result = compiled.matcher->find_all("AÇÃO ação");

  CHECK(result.status == RegexMatchStatus::completed);
  REQUIRE(result.matches.size() == 1);
  CHECK(result.matches.front().offset == 7);
#else
  REQUIRE_FALSE(compiled.matcher.has_value());
#endif
}

TEST_CASE("regex matching reports compilation errors")
{
  SearchOptions options;
  options.mode = SearchMode::regex;

  const auto compiled = compile_regex("(", options);

  REQUIRE_FALSE(compiled.matcher.has_value());
  REQUIRE(compiled.error.has_value());
}

TEST_CASE("regex matching exposes JIT availability when PCRE2 supports it")
{
  SearchOptions options;
  options.mode = SearchMode::regex;

  auto compiled = compile_regex("needle", options);

#ifdef UBURU_HAS_PCRE2
  REQUIRE(compiled.matcher.has_value());
  CHECK(compiled.matcher->jit_enabled());
#else
  REQUIRE_FALSE(compiled.matcher.has_value());
#endif
}

TEST_CASE("regex matching reports timeout")
{
  SearchOptions options;
  options.mode = SearchMode::regex;
  options.regex_timeout = std::chrono::milliseconds{0};

  auto compiled = compile_regex("needle", options);

#ifdef UBURU_HAS_PCRE2
  REQUIRE(compiled.matcher.has_value());
  const auto result = compiled.matcher->find_all("needle");

  CHECK(result.status == RegexMatchStatus::timed_out);
#else
  REQUIRE_FALSE(compiled.matcher.has_value());
#endif
}
