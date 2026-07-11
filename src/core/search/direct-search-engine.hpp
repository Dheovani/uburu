#pragma once

#include "core/filesystem/file-scanner.hpp"
#include "core/search/search-engine.hpp"

#include <memory>

namespace uburu::search
{

  /**
   * Searches the current filesystem tree directly through a FileScanner.
   */
  class DirectSearchEngine final : public SearchEngine
  {
  public:
    explicit DirectSearchEngine(std::shared_ptr<const filesystem::FileScanner> scanner);

    [[nodiscard]]
    SearchSummary search(
      const SearchQuery& query,
      ResultSink sink,
      std::stop_token stopToken = {}) const override;

  private:
    std::shared_ptr<const filesystem::FileScanner> scanner;
  };

} // namespace uburu::search
