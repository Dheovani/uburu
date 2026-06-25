#include "core/filesystem/polling-file-watcher.hpp"

#include "core/filesystem/path-normalization.hpp"

#include <algorithm>
#include <system_error>
#include <utility>

namespace uburu::filesystem
{
  PollingFileWatcher::PollingFileWatcher(std::filesystem::path root) : root_(std::move(root))
  {
    entries_ = snapshot({});
  }

  std::vector<FileChangeEvent> PollingFileWatcher::poll(std::stop_token stop_token)
  {
    auto current = snapshot(stop_token);
    std::vector<FileChangeEvent> events;

    for (const auto& [key, entry] : current) {
      const auto previous = entries_.find(key);

      if (previous == entries_.end()) {
        events.push_back(FileChangeEvent{
          .relative_path = entry.relative_path,
          .kind = FileChangeKind::created,
          .directory = entry.directory});

        continue;
      }

      if (changed(previous->second, entry)) {
        events.push_back(FileChangeEvent{
          .relative_path = entry.relative_path,
          .kind = FileChangeKind::modified,
          .directory = entry.directory});
      }
    }

    for (const auto& [key, entry] : entries_) {
      if (!current.contains(key)) {
        events.push_back(FileChangeEvent{
          .relative_path = entry.relative_path,
          .kind = FileChangeKind::deleted,
          .directory = entry.directory});
      }
    }

    std::ranges::sort(events, [](const auto& left, const auto& right) {
      return normalized_path_key(left.relative_path) < normalized_path_key(right.relative_path);
    });

    entries_ = std::move(current);

    return events;
  }

  bool PollingFileWatcher::changed(const WatchedEntry& previous, const WatchedEntry& current)
  {
    return previous.modified_at != current.modified_at || previous.size != current.size ||
           previous.directory != current.directory;
  }

  std::unordered_map<std::string, PollingFileWatcher::WatchedEntry>
  PollingFileWatcher::snapshot(std::stop_token stop_token) const
  {
    std::unordered_map<std::string, WatchedEntry> entries;
    std::error_code error;
    std::filesystem::recursive_directory_iterator iterator(
      root_, std::filesystem::directory_options::skip_permission_denied, error);
    const std::filesystem::recursive_directory_iterator end;

    while (!error && iterator != end) {
      if (stop_token.stop_requested())
        break;

      const auto path = iterator->path();
      const auto relative_path = std::filesystem::relative(path, root_, error);

      if (error) {
        iterator.increment(error);

        continue;
      }

      const auto directory = iterator->is_directory(error);
      const auto size = directory ? 0 : iterator->file_size(error);

      if (error) {
        iterator.increment(error);

        continue;
      }

      WatchedEntry entry{.relative_path = relative_path,
                         .modified_at = iterator->last_write_time(error),
                         .size = size,
                         .directory = directory};

      if (!error)
        entries.emplace(normalized_path_key(relative_path), std::move(entry));

      iterator.increment(error);
    }

    return entries;
  }

} // namespace uburu::filesystem
