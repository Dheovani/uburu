#pragma once

#include "app/dto/search-dto.hpp"
#include "cli-options.hpp"
#include "core/search/search-engine.hpp"
#include "shared/types/domain-types.hpp"

#include <iosfwd>

namespace uburu::cli
{

  /**
   * Writes one search result using the requested command-line format.
   */
  void writeSearchResult(std::ostream& output, const SearchResult& result, CliOutputFormat format);

  /**
   * Writes the final search summary using the requested command-line format.
   */
  void writeSearchSummary(std::ostream& output, const search::SearchSummary& summary, CliOutputFormat format);

} // namespace uburu::cli
