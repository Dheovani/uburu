#pragma once

#include "core/filesystem/file-watcher.hpp"

#include <filesystem>
#include <mutex>
#include <vector>

namespace uburu::filesystem
{

  class MacosFileWatcher;

  void appendMacosEvents(MacosFileWatcher& watcher, FileChangeBatch batch);

  class MacosFileWatcher final : public FileWatcher
  {
  public:
    explicit MacosFileWatcher(std::filesystem::path root);
    ~MacosFileWatcher() override;

    MacosFileWatcher(const MacosFileWatcher&) = delete;
    MacosFileWatcher& operator=(const MacosFileWatcher&) = delete;

    [[nodiscard]] FileChangeBatch poll(std::stop_token stop_token = {}) override;

  private:
    friend void appendMacosEvents(MacosFileWatcher& watcher, FileChangeBatch batch);

    struct NativeStream;

    void append(FileChangeBatch batch);
    [[nodiscard]] std::filesystem::path relativeFromRoot(const std::filesystem::path& path) const;

    std::filesystem::path root;
    std::mutex mutex;
    FileChangeBatch pending;
    NativeStream* stream{nullptr};
  };

} // namespace uburu::filesystem
