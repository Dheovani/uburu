#pragma once

#include <filesystem>
#include <stop_token>
#include <vector>

namespace uburu::filesystem
{

  enum class FileChangeKind
  {
    created,
    modified,
    deleted
  };

  struct FileChangeEvent
  {
    std::filesystem::path relative_path;
    FileChangeKind kind{FileChangeKind::modified};
    bool directory{false};
  };

  class FileWatcher
  {
  public:
    virtual ~FileWatcher() = default;

    [[nodiscard]] virtual std::vector<FileChangeEvent> poll(std::stop_token stop_token = {}) = 0;
  };

} // namespace uburu::filesystem
