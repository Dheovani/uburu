#pragma once

#include "core/diagnostics/structured-logger.hpp"

#include <cstddef>
#include <filesystem>

namespace uburu::diagnostics
{

  struct FileStructuredLogOptions
  {
    StructuredLogOptions structuredOptions;
    std::filesystem::path path;
    std::uintmax_t maximumFileSizeBytes{1024U * 1024U};
    std::size_t maximumRotatedFiles{3};
  };

  class FileStructuredLogger final : public StructuredLogger
  {
  public:
    explicit FileStructuredLogger(FileStructuredLogOptions options);

    void write(LogEvent event) override;

  private:
    FileStructuredLogOptions options;

    void rotateIfNeeded(std::uintmax_t nextLineSize);
  };

} // namespace uburu::diagnostics
