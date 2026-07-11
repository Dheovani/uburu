#pragma once

#include "core/diagnostics/metrics.hpp"
#include "shared/types/domain-types.hpp"

#include <functional>
#include <stop_token>

namespace uburu::filesystem
{

  using FileSink = std::function<bool(FileEntry)>;

  /**
   * Scans a root and streams candidate files without owning search or matching logic.
   */
  class FileScanner
  {
  public:
    virtual ~FileScanner() = default;

    virtual void scan(
      const std::filesystem::path& root,
      const SearchOptions& options,
      FileSink sink,
      std::stop_token stopToken = {},
      diagnostics::SearchMetrics* metrics = nullptr) const = 0;
  };

} // namespace uburu::filesystem
