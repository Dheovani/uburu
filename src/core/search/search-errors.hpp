#pragma once

#include "core/search/search-engine.hpp"

namespace uburu::search
{

  [[nodiscard]] std::string translationKeyFor(SearchErrorCode code);
  [[nodiscard]] SearchError makeSearchError(SearchErrorCode code, std::string context = {},
                                              std::optional<std::size_t> offset = std::nullopt);

} // namespace uburu::search
