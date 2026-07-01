#include "core/text/text-matcher.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>

using uburu::SearchOptions;
using uburu::text::findAllLiterals;
using uburu::text::findLiteral;

TEST_CASE("literal matching is case insensitive by default")
{
  const auto match = findLiteral("Uma Busca Rapida", "busca", SearchOptions{});
  REQUIRE(match.has_value());
  CHECK(match->offset == 4);
  CHECK(match->length == 5);
}

TEST_CASE("literal matching returns every occurrence on a line")
{
  const auto matches = findAllLiterals("needle here, needle there", "needle", SearchOptions{});

  REQUIRE(matches.size() == 2);
  CHECK(matches[0].offset == 0);
  CHECK(matches[0].length == 6);
  CHECK(matches[1].offset == 13);
  CHECK(matches[1].length == 6);
}

TEST_CASE("literal matching keeps overlapping occurrences")
{
  const auto matches = findAllLiterals("aaaa", "aa", SearchOptions{});

  REQUIRE(matches.size() == 3);
  CHECK(matches[0].offset == 0);
  CHECK(matches[1].offset == 1);
  CHECK(matches[2].offset == 2);
}

TEST_CASE("literal matching can be case sensitive")
{
  SearchOptions options;
  options.caseSensitive = true;
  CHECK_FALSE(findLiteral("Uburu", "uburu", options).has_value());
}

TEST_CASE("literal matching is case insensitive for precomposed latin Unicode")
{
  const auto match = findLiteral("A busca encontrou AÇÃO no arquivo", "ação", SearchOptions{});

  REQUIRE(match.has_value());
  CHECK(match->offset == 18);
  CHECK(match->length == 6);
}

TEST_CASE("literal matching treats precomposed and decomposed Portuguese accents as equivalent")
{
  const std::string decomposedText =
    "a gerac\xCC\xA7"
    "a\xCC\x83"
    "o e a corrupc\xCC\xA7"
    "a\xCC\x83"
    "o da mate\xCC\x81"
    "ria";
  const std::string precomposedExpression = "gera\xC3\xA7\xC3\xA3o e a corrup\xC3\xA7\xC3\xA3o da mat\xC3\xA9ria";

  const auto match = findLiteral(decomposedText, precomposedExpression, SearchOptions{});

  REQUIRE(match.has_value());
  CHECK(match->offset == 2);
}

TEST_CASE("literal matching treats decomposed and precomposed Portuguese accents as equivalent")
{
  const std::string precomposedText = "a gera\xC3\xA7\xC3\xA3o e a corrup\xC3\xA7\xC3\xA3o da mat\xC3\xA9ria";
  const std::string decomposedExpression =
    "gerac\xCC\xA7"
    "a\xCC\x83"
    "o e a corrupc\xCC\xA7"
    "a\xCC\x83"
    "o da mate\xCC\x81"
    "ria";

  const auto match = findLiteral(precomposedText, decomposedExpression, SearchOptions{});

  REQUIRE(match.has_value());
  CHECK(match->offset == 2);
}

TEST_CASE("literal matching does not treat missing accents as canonical equivalents")
{
  CHECK_FALSE(findLiteral("geracao", "gera\xC3\xA7\xC3\xA3o", SearchOptions{}).has_value());
}

TEST_CASE("case sensitive literal matching keeps Unicode casing strict")
{
  SearchOptions options;
  options.caseSensitive = true;

  CHECK_FALSE(findLiteral("CAFÉ", "café", options).has_value());
  CHECK(findLiteral("CAFÉ", "CAFÉ", options).has_value());
}

TEST_CASE("literal matching keeps UTF-8 byte offsets and byte lengths")
{
  const auto match = findLiteral("pré-ação", "ação", SearchOptions{});

  REQUIRE(match.has_value());
  CHECK(match->offset == 5);
  CHECK(match->length == 6);
}

TEST_CASE("whole word uses natural language boundaries")
{
  SearchOptions options;
  options.wholeWord = true;

  CHECK(findLiteral("search_engine", "search", options).has_value());
  CHECK(findLiteral("search engine", "search", options).has_value());
  CHECK(findLiteral("pré-ação", "ação", options).has_value());
  CHECK_FALSE(findLiteral("préação", "ação", options).has_value());
}

TEST_CASE("whole identifier does not match code identifier fragments")
{
  SearchOptions options;
  options.wholeIdentifier = true;

  CHECK_FALSE(findLiteral("search_engine", "search", options).has_value());
  CHECK_FALSE(findLiteral("searchEngine", "search", options).has_value());
  CHECK_FALSE(findLiteral("search2", "search", options).has_value());
  CHECK(findLiteral("search engine", "search", options).has_value());
  CHECK(findLiteral("call(search)", "search", options).has_value());
}

TEST_CASE("a null byte identifies a binary sample")
{
  const std::string sample{"text\0binary", 11};
  CHECK(uburu::text::looksBinary(sample));
  CHECK_FALSE(uburu::text::looksBinary("plain text"));
}
