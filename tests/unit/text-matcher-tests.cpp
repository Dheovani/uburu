#include "core/text/text-matcher.hpp"

#include <catch2/catch_test_macros.hpp>

using uburu::SearchOptions;
using uburu::text::find_all_literals;
using uburu::text::find_literal;

TEST_CASE("literal matching is case insensitive by default")
{
  const auto match = find_literal("Uma Busca Rapida", "busca", SearchOptions{});
  REQUIRE(match.has_value());
  CHECK(match->offset == 4);
  CHECK(match->length == 5);
}

TEST_CASE("literal matching returns every occurrence on a line")
{
  const auto matches = find_all_literals("needle here, needle there", "needle", SearchOptions{});

  REQUIRE(matches.size() == 2);
  CHECK(matches[0].offset == 0);
  CHECK(matches[0].length == 6);
  CHECK(matches[1].offset == 13);
  CHECK(matches[1].length == 6);
}

TEST_CASE("literal matching keeps overlapping occurrences")
{
  const auto matches = find_all_literals("aaaa", "aa", SearchOptions{});

  REQUIRE(matches.size() == 3);
  CHECK(matches[0].offset == 0);
  CHECK(matches[1].offset == 1);
  CHECK(matches[2].offset == 2);
}

TEST_CASE("literal matching can be case sensitive")
{
  SearchOptions options;
  options.case_sensitive = true;
  CHECK_FALSE(find_literal("Uburu", "uburu", options).has_value());
}

TEST_CASE("literal matching is case insensitive for precomposed latin Unicode")
{
  const auto match = find_literal("A busca encontrou AÇÃO no arquivo", "ação", SearchOptions{});

  REQUIRE(match.has_value());
  CHECK(match->offset == 18);
  CHECK(match->length == 6);
}

TEST_CASE("case sensitive literal matching keeps Unicode casing strict")
{
  SearchOptions options;
  options.case_sensitive = true;

  CHECK_FALSE(find_literal("CAFÉ", "café", options).has_value());
  CHECK(find_literal("CAFÉ", "CAFÉ", options).has_value());
}

TEST_CASE("literal matching keeps UTF-8 byte offsets and byte lengths")
{
  const auto match = find_literal("pré-ação", "ação", SearchOptions{});

  REQUIRE(match.has_value());
  CHECK(match->offset == 5);
  CHECK(match->length == 6);
}

TEST_CASE("whole word uses natural language boundaries")
{
  SearchOptions options;
  options.whole_word = true;

  CHECK(find_literal("search_engine", "search", options).has_value());
  CHECK(find_literal("search engine", "search", options).has_value());
  CHECK(find_literal("pré-ação", "ação", options).has_value());
  CHECK_FALSE(find_literal("préação", "ação", options).has_value());
}

TEST_CASE("whole identifier does not match code identifier fragments")
{
  SearchOptions options;
  options.whole_identifier = true;

  CHECK_FALSE(find_literal("search_engine", "search", options).has_value());
  CHECK_FALSE(find_literal("searchEngine", "search", options).has_value());
  CHECK_FALSE(find_literal("search2", "search", options).has_value());
  CHECK(find_literal("search engine", "search", options).has_value());
  CHECK(find_literal("call(search)", "search", options).has_value());
}

TEST_CASE("a null byte identifies a binary sample")
{
  const std::string sample{"text\0binary", 11};
  CHECK(uburu::text::looks_binary(sample));
  CHECK_FALSE(uburu::text::looks_binary("plain text"));
}
