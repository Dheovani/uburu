#pragma once

#include "core/filesystem/file-watcher.hpp"

#include <filesystem>
#include <mutex>
#include <vector>

namespace uburu::filesystem
{

  class MacosFileWatcher;

  void append_macos_events(MacosFileWatcher& watcher, FileChangeBatch batch);

  class MacosFileWatcher final : public FileWatcher
  {
  public:
    explicit MacosFileWatcher(std::filesystem::path root);
    ~MacosFileWatcher() override;

    MacosFileWatcher(const MacosFileWatcher&) = delete;
    MacosFileWatcher& operator=(const MacosFileWatcher&) = delete;

    [[nodiscard]] FileChangeBatch poll(std::stop_token stop_token = {}) override;

  private:
    friend void append_macos_events(MacosFileWatcher& watcher, FileChangeBatch batch);

    struct NativeStream;

    void append(FileChangeBatch batch);
    [[nodiscard]] std::filesystem::path relative_from_root(const std::filesystem::path& path) const;

    std::filesystem::path root_;
    std::mutex mutex_;
    FileChangeBatch pending_;
    NativeStream* stream_{nullptr};
  };

} // namespace uburu::filesystem
