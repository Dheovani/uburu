#pragma once

#include "core/filesystem/file-scanner.hpp"

namespace uburu::filesystem
{

  class RecursiveFileScanner final : public FileScanner
  {
  public:
    void scan(const std::filesystem::path& root, const SearchOptions& options, FileSink sink,
              std::stop_token stop_token = {}, diagnostics::SearchMetrics* metrics = nullptr) const override;
  };

} // namespace uburu::filesystem
