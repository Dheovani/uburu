#pragma once

#include "core/diagnostics/metrics.hpp"
#include "shared/types/domain-types.hpp"

#include <functional>
#include <stop_token>

namespace uburu::filesystem
{

  using FileSink = std::function<bool(FileEntry)>;

  class FileScanner
  {
  public:
    virtual ~FileScanner() = default;
    virtual void scan(const std::filesystem::path& root,
                      const SearchOptions& options,
                      FileSink sink,
                      std::stop_token stop_token = {},
                      diagnostics::SearchMetrics* metrics = nullptr) const = 0;
  };

} // namespace uburu::filesystem
