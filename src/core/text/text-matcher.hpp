#pragma once

#include "shared/types/domain-types.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace uburu::text
{

  /**
   * Identifies one match using byte offsets into the decoded text.
   */
  struct MatchPosition
  {
    std::size_t offset{0};
    std::size_t length{0};
  };

  /**
   * Finds the first literal match using the requested case and boundary rules.
   */
  [[nodiscard]]
  std::optional<MatchPosition> findLiteral(
    std::string_view text,
    std::string_view expression,
    const SearchOptions& options);

  /**
   * Finds every literal match, including overlapping occurrences.
   */
  [[nodiscard]]
  std::vector<MatchPosition> findAllLiterals(
    std::string_view text,
    std::string_view expression,
    const SearchOptions& options);

  [[nodiscard]]
  bool matchesRequestedBoundaries(std::string_view text, MatchPosition match, const SearchOptions& options);

  [[nodiscard]]
  bool looksBinary(std::string_view sample);

} // namespace uburu::text
