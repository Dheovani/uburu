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

  struct FileChangeBatch
  {
    std::vector<FileChangeEvent> events;
    bool events_may_be_incomplete{false};
    bool requires_rescan{false};
  };

  class FileWatcher
  {
  public:
    virtual ~FileWatcher() = default;

    [[nodiscard]] virtual FileChangeBatch poll(std::stop_token stop_token = {}) = 0;
  };

} // namespace uburu::filesystem
