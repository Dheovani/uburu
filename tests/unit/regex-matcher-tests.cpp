#include "core/text/regex-matcher.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

using uburu::SearchMode;
using uburu::SearchOptions;
using uburu::text::compileRegex;
using uburu::text::RegexMatchStatus;

TEST_CASE("regex matching returns all PCRE2 matches")
{
  SearchOptions options;
  options.mode = SearchMode::regex;

  auto compiled = compileRegex(R"(todo\(\d+\))", options);

#ifdef UBURU_HAS_PCRE2
  REQUIRE(compiled.matcher.has_value());
  const auto result = compiled.matcher->findAll("todo(1) skip todo(42)");

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

  auto compiled = compileRegex("ação", options);

#ifdef UBURU_HAS_PCRE2
  REQUIRE(compiled.matcher.has_value());
  const auto result = compiled.matcher->findAll("AÇÃO ação");

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
  options.caseSensitive = true;

  auto compiled = compileRegex("ação", options);

#ifdef UBURU_HAS_PCRE2
  REQUIRE(compiled.matcher.has_value());
  const auto result = compiled.matcher->findAll("AÇÃO ação");

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

  const auto compiled = compileRegex("(", options);

  REQUIRE_FALSE(compiled.matcher.has_value());
  REQUIRE(compiled.error.has_value());
}

TEST_CASE("regex matching exposes JIT availability when PCRE2 supports it")
{
  SearchOptions options;
  options.mode = SearchMode::regex;

  auto compiled = compileRegex("needle", options);

#ifdef UBURU_HAS_PCRE2
  REQUIRE(compiled.matcher.has_value());
  CHECK(compiled.matcher->jitEnabled());
#else
  REQUIRE_FALSE(compiled.matcher.has_value());
#endif
}

TEST_CASE("regex matching reports timeout")
{
  SearchOptions options;
  options.mode = SearchMode::regex;
  options.regexTimeout = std::chrono::milliseconds{0};

  auto compiled = compileRegex("needle", options);

#ifdef UBURU_HAS_PCRE2
  REQUIRE(compiled.matcher.has_value());
  const auto result = compiled.matcher->findAll("needle");

  CHECK(result.status == RegexMatchStatus::timedOut);
#else
  REQUIRE_FALSE(compiled.matcher.has_value());
#endif
}
