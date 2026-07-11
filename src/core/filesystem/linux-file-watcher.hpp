#pragma once

#include "core/filesystem/file-watcher.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace uburu::filesystem
{

  /**
   * Linux watcher backed by inotify when the platform API is available.
   */
  class LinuxFileWatcher final : public FileWatcher
  {
  public:
    explicit LinuxFileWatcher(std::filesystem::path root);
    ~LinuxFileWatcher() override;

    LinuxFileWatcher(const LinuxFileWatcher&) = delete;
    LinuxFileWatcher& operator=(const LinuxFileWatcher&) = delete;

    [[nodiscard]]
    FileChangeBatch poll(std::stop_token stopToken = {}) override;

  private:
    [[nodiscard]]
    bool available() const;

    [[nodiscard]]
    FileChangeBatch unavailableBatch() const;

    [[nodiscard]]
    std::filesystem::path relativeFromRoot(const std::filesystem::path& path) const;

    void addDirectoryWatch(const std::filesystem::path& directory);
    void addRecursiveWatches();
    void removeWatch(int descriptor);

    std::filesystem::path root;
    int descriptor{-1};
    std::unordered_map<int, std::filesystem::path> watchedDirectories;
    std::vector<unsigned char> buffer;
  };

} // namespace uburu::filesystem
