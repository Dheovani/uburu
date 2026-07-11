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

  /**
   * Carries one normalized change event relative to the watched root.
   */
  struct FileChangeEvent
  {
    std::filesystem::path relativePath;
    FileChangeKind kind{FileChangeKind::modified};
    bool directory{false};
  };

  /**
   * Groups watcher events and records whether the batch is complete enough to trust.
   */
  struct FileChangeBatch
  {
    std::vector<FileChangeEvent> events;
    bool eventsMayBeIncomplete{false};
    bool requiresRescan{false};
  };

  /**
   * Polls filesystem changes behind platform-specific watcher implementations.
   */
  class FileWatcher
  {
  public:
    virtual ~FileWatcher() = default;

    [[nodiscard]]
    virtual FileChangeBatch poll(std::stop_token stopToken = {}) = 0;
  };

} // namespace uburu::filesystem
