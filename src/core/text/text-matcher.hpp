#pragma once

#include "shared/types/domain-types.hpp"

#include <optional>
#include <string_view>

namespace uburu::text
{

  struct MatchPosition
  {
    std::size_t offset{0};
    std::size_t length{0};
  };

  [[nodiscard]] std::optional<MatchPosition>
  find_literal(std::string_view text, std::string_view expression, const SearchOptions& options);
  [[nodiscard]] bool looks_binary(std::string_view sample);

} // namespace uburu::text
