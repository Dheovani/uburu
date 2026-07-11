#pragma once

#include "core/filesystem/file-scanner.hpp"

namespace uburu::filesystem
{

  /**
   * Recursively scans project files while applying search filters, ignore rules, and cancellation.
   */
  class RecursiveFileScanner final : public FileScanner
  {
  public:
    void scan(
      const std::filesystem::path& root,
      const SearchOptions& options,
      FileSink sink,
      std::stop_token stopToken = {},
      diagnostics::SearchMetrics* metrics = nullptr) const override;
  };

} // namespace uburu::filesystem
