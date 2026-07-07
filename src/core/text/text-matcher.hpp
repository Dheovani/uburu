#pragma once

#include "shared/types/domain-types.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace uburu::text
{

  struct MatchPosition
  {
    std::size_t offset{0};
    std::size_t length{0};
  };

  [[nodiscard]]
  std::optional<MatchPosition>
  findLiteral(std::string_view text, std::string_view expression, const SearchOptions& options);
  [[nodiscard]]
  std::vector<MatchPosition>
  findAllLiterals(std::string_view text, std::string_view expression, const SearchOptions& options);
  [[nodiscard]]
  bool matchesRequestedBoundaries(std::string_view text, MatchPosition match, const SearchOptions& options);
  [[nodiscard]]
  bool looksBinary(std::string_view sample);

} // namespace uburu::text
