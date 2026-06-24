#include "core/text/text-matcher.hpp"

#include <algorithm>
#include <cctype>

namespace uburu::text
{
  namespace
  {

    bool is_word_character(unsigned char character)
    {
      return std::isalnum(character) != 0 || character == '_';
    }

    char folded(char character)
    {
      return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }

  } // namespace

  std::vector<MatchPosition> find_all_literals(std::string_view text, std::string_view expression,
                                               const SearchOptions& options)
  {
    std::vector<MatchPosition> matches;
    if (expression.empty())
      return matches;

    const auto matches_at = [&](std::size_t offset) {
      for (std::size_t index = 0; index < expression.size(); ++index) {
        const char left = text[offset + index];
        const char right = expression[index];
        if (options.case_sensitive ? left != right : folded(left) != folded(right))
          return false;
      }
      if (!options.whole_word)
        return true;
      const bool left_boundary =
          offset == 0 || !is_word_character(static_cast<unsigned char>(text[offset - 1]));
      const auto end = offset + expression.size();
      const bool right_boundary =
          end == text.size() || !is_word_character(static_cast<unsigned char>(text[end]));
      return left_boundary && right_boundary;
    };

    for (std::size_t offset = 0; offset + expression.size() <= text.size(); ++offset) {
      if (matches_at(offset))
        matches.push_back(MatchPosition{offset, expression.size()});
    }
    return matches;
  }

  std::optional<MatchPosition> find_literal(std::string_view text, std::string_view expression,
                                            const SearchOptions& options)
  {
    const auto matches = find_all_literals(text, expression, options);
    if (matches.empty())
      return std::nullopt;
    return matches.front();
  }

  bool looks_binary(std::string_view sample)
  {
    return std::ranges::find(sample, '\0') != sample.end();
  }

} // namespace uburu::text
