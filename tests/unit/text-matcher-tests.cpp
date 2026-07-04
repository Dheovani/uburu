#include "core/text/text-matcher.hpp"
#include "fixtures/test-fixtures.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <string_view>
#include <vector>

using uburu::SearchOptions;
using uburu::text::findAllLiterals;
using uburu::text::findLiteral;
using uburu::text::matchesRequestedBoundaries;

namespace
{

  struct BoundaryFixture
  {
    std::string_view text;
    std::size_t offset{0};
    std::size_t length{0};
    bool wholeWord{false};
    bool wholeIdentifier{false};
    bool expected{false};
  };

} // namespace

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
  const auto decomposedText = uburu::tests::fixtures::portugueseDecomposedText();
  const auto precomposedExpression = uburu::tests::fixtures::portuguesePrecomposedText().substr(2);

  const auto match = findLiteral(decomposedText, precomposedExpression, SearchOptions{});

  REQUIRE(match.has_value());
  CHECK(match->offset == 2);
}

TEST_CASE("literal matching treats decomposed and precomposed Portuguese accents as equivalent")
{
  const auto precomposedText = uburu::tests::fixtures::portuguesePrecomposedText();
  const auto decomposedExpression = uburu::tests::fixtures::portugueseDecomposedText().substr(2);

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

TEST_CASE("requested boundaries combine whole word and whole identifier rules")
{
  const std::vector<BoundaryFixture> fixtures{
    {.text = "search engine", .offset = 0, .length = 6, .wholeWord = true, .wholeIdentifier = true, .expected = true},
    {.text = "search_engine", .offset = 0, .length = 6, .wholeWord = true, .wholeIdentifier = true, .expected = false},
    {.text = "search2", .offset = 0, .length = 6, .wholeWord = true, .wholeIdentifier = true, .expected = false},
    {.text = "pre-action", .offset = 4, .length = 6, .wholeWord = true, .wholeIdentifier = false, .expected = true},
    {.text = "preaction", .offset = 3, .length = 6, .wholeWord = true, .wholeIdentifier = false, .expected = false},
    {.text = "call(search)", .offset = 5, .length = 6, .wholeWord = false, .wholeIdentifier = true, .expected = true},
  };

  for (const auto& fixture : fixtures) {
    SearchOptions options;
    options.wholeWord = fixture.wholeWord;
    options.wholeIdentifier = fixture.wholeIdentifier;

    CHECK(matchesRequestedBoundaries(fixture.text, {fixture.offset, fixture.length}, options) == fixture.expected);
  }
}

TEST_CASE("a null byte identifies a binary sample")
{
  const std::string sample{"text\0binary", 11};
  CHECK(uburu::text::looksBinary(sample));
  CHECK_FALSE(uburu::text::looksBinary("plain text"));
}
