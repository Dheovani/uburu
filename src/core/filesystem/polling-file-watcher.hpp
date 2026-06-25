#pragma once

#include "core/filesystem/file-watcher.hpp"

#include <filesystem>
#include <string>
#include <unordered_map>

namespace uburu::filesystem
{

  class PollingFileWatcher final : public FileWatcher
  {
  public:
    explicit PollingFileWatcher(std::filesystem::path root);

    [[nodiscard]] FileChangeBatch poll(std::stop_token stop_token = {}) override;

  private:
    struct WatchedEntry
    {
      std::filesystem::path relativePath;
      std::filesystem::file_time_type modifiedAt{};
      std::uintmax_t size{0};
      bool directory{false};
    };

    struct Snapshot
    {
      std::unordered_map<std::string, WatchedEntry> entries;
      bool incomplete{false};
    };

    [[nodiscard]] Snapshot snapshot(std::stop_token stop_token) const;

    [[nodiscard]] static bool changed(const WatchedEntry& previous, const WatchedEntry& current);

    std::filesystem::path root;
    std::unordered_map<std::string, WatchedEntry> entries;
  };

} // namespace uburu::filesystem
