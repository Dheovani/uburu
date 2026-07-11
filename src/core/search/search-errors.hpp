#pragma once

#include "core/search/search-engine.hpp"

namespace uburu::search
{

  /**
   * Maps a stable error code to the UI translation key used by the application layer.
   */
  [[nodiscard]]
  std::string translationKeyFor(SearchErrorCode code);

  /**
   * Creates a structured search error while preserving optional context and byte offset.
   */
  [[nodiscard]]
  SearchError makeSearchError(
    SearchErrorCode code,
    std::string context = {},
    std::optional<std::size_t> offset = std::nullopt);

} // namespace uburu::search
