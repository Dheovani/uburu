#pragma once

#include "core/filesystem/file-watcher.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace uburu::filesystem
{

  class LinuxFileWatcher final : public FileWatcher
  {
  public:
    explicit LinuxFileWatcher(std::filesystem::path root);
    ~LinuxFileWatcher() override;

    LinuxFileWatcher(const LinuxFileWatcher&) = delete;
    LinuxFileWatcher& operator=(const LinuxFileWatcher&) = delete;

    [[nodiscard]] FileChangeBatch poll(std::stop_token stop_token = {}) override;

  private:
    [[nodiscard]] bool available() const;
    [[nodiscard]] FileChangeBatch unavailable_batch() const;
    [[nodiscard]] std::filesystem::path relative_from_root(const std::filesystem::path& path) const;

    void add_directory_watch(const std::filesystem::path& directory);
    void add_recursive_watches();
    void remove_watch(int descriptor);

    std::filesystem::path root_;
    int descriptor_{-1};
    std::unordered_map<int, std::filesystem::path> watched_directories_;
    std::vector<unsigned char> buffer_;
  };

} // namespace uburu::filesystem
