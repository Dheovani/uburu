#pragma once

#include "core/filesystem/file-watcher.hpp"

#include <filesystem>
#include <mutex>
#include <vector>

namespace uburu::filesystem
{

  class MacosFileWatcher;

  /**
   * Appends native FSEvents batches from the platform callback into the watcher queue.
   */
  void appendMacosEvents(MacosFileWatcher& watcher, FileChangeBatch batch);

  /**
   * macOS watcher backed by FSEvents when native support is available.
   */
  class MacosFileWatcher final : public FileWatcher
  {
  public:
    explicit MacosFileWatcher(std::filesystem::path root);
    ~MacosFileWatcher() override;

    MacosFileWatcher(const MacosFileWatcher&) = delete;
    MacosFileWatcher& operator=(const MacosFileWatcher&) = delete;

    [[nodiscard]]
    FileChangeBatch poll(std::stop_token stopToken = {}) override;

  private:
    friend void appendMacosEvents(MacosFileWatcher& watcher, FileChangeBatch batch);

    struct NativeStream;

    void append(FileChangeBatch batch);

    [[nodiscard]]
    std::filesystem::path relativeFromRoot(const std::filesystem::path& path) const;

    std::filesystem::path root;
    std::mutex mutex;
    FileChangeBatch pending;
    NativeStream* stream{nullptr};
  };

} // namespace uburu::filesystem
