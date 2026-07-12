#pragma once

#include "core/filesystem/file-watcher.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <variant>

namespace uburu::filesystem
{

  enum class FileWatcherBackendKind
  {
    native,
    polling
  };

  enum class FileWatcherOpenErrorCode
  {
    unavailable,
    invalidRoot,
    permissionDenied,
    resourceLimitExceeded
  };

  struct FileWatcherBackendCapabilities
  {
    std::string name;
    FileWatcherBackendKind kind{FileWatcherBackendKind::polling};
    bool recursive{false};
    bool overflowDetection{false};
    bool coalescesEvents{false};
  };

  struct FileWatcherOpenRequest
  {
    std::filesystem::path root;
    bool recursive{true};
    bool preferNative{true};
  };

  struct FileWatcherOpenError
  {
    FileWatcherOpenErrorCode code{FileWatcherOpenErrorCode::unavailable};
    std::string message;
  };

  using FileWatcherOpenResult = std::variant<std::unique_ptr<FileWatcher>, FileWatcherOpenError>;

  class FileWatcherFactory
  {
  public:
    virtual ~FileWatcherFactory() = default;

    [[nodiscard]]
    virtual FileWatcherBackendCapabilities capabilities() const = 0;

    [[nodiscard]]
    virtual FileWatcherOpenResult open(const FileWatcherOpenRequest& request) const = 0;
  };

} // namespace uburu::filesystem
