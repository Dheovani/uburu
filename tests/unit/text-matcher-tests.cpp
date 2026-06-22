#include "core/text/text-matcher.hpp"

#include <catch2/catch_test_macros.hpp>

using uburu::SearchOptions;
using uburu::text::find_literal;

TEST_CASE("literal matching is case insensitive by default")
{
  const auto match = find_literal("Uma Busca Rapida", "busca", SearchOptions{});
  REQUIRE(match.has_value());
  CHECK(match->offset == 4);
  CHECK(match->length == 5);
}

TEST_CASE("literal matching can be case sensitive")
{
  SearchOptions options;
  options.case_sensitive = true;
  CHECK_FALSE(find_literal("Uburu", "uburu", options).has_value());
}

TEST_CASE("whole word does not match an identifier fragment")
{
  SearchOptions options;
  options.whole_word = true;
  CHECK_FALSE(find_literal("search_engine", "search", options).has_value());
  CHECK(find_literal("search engine", "search", options).has_value());
}

TEST_CASE("a null byte identifies a binary sample")
{
  const std::string sample{"text\0binary", 11};
  CHECK(uburu::text::looks_binary(sample));
  CHECK_FALSE(uburu::text::looks_binary("plain text"));
}
