#pragma once

#include "core/filesystem/file-scanner.hpp"
#include "core/search/search-engine.hpp"

#include <memory>

namespace uburu::search
{

  class DirectSearchEngine final : public SearchEngine
  {
  public:
    explicit DirectSearchEngine(std::shared_ptr<const filesystem::FileScanner> scanner);
    [[nodiscard]] SearchSummary
    search(const SearchQuery& query, ResultSink sink, std::stop_token stop_token = {}) const override;

  private:
    std::shared_ptr<const filesystem::FileScanner> scanner;
  };

} // namespace uburu::search
