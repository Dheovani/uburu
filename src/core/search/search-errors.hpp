#pragma once

#include "core/search/search-engine.hpp"

namespace uburu::search
{

  [[nodiscard]] std::string translation_key_for(SearchErrorCode code);
  [[nodiscard]] SearchError make_search_error(SearchErrorCode code, std::string context = {},
                                              std::optional<std::size_t> offset = std::nullopt);

} // namespace uburu::search
