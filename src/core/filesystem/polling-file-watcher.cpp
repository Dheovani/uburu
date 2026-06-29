#include "core/filesystem/polling-file-watcher.hpp"

#include "core/filesystem/path-normalization.hpp"

#include <algorithm>
#include <system_error>
#include <utility>

namespace uburu::filesystem
{
  PollingFileWatcher::PollingFileWatcher(std::filesystem::path root) : root(std::move(root))
  {
    entries = snapshot({}).entries;
  }

  FileChangeBatch PollingFileWatcher::poll(std::stop_token stop_token)
  {
    auto current = snapshot(stop_token);
    FileChangeBatch batch;

    if (current.incomplete) {
      batch.eventsMayBeIncomplete = true;
      batch.requiresRescan = true;

      return batch;
    }

    for (const auto& [key, entry] : current.entries) {
      const auto previous = entries.find(key);

      if (previous == entries.end()) {
        batch.events.push_back(FileChangeEvent{
            .relativePath = entry.relativePath, .kind = FileChangeKind::created, .directory = entry.directory});

        continue;
      }

      if (changed(previous->second, entry)) {
        batch.events.push_back(FileChangeEvent{
            .relativePath = entry.relativePath, .kind = FileChangeKind::modified, .directory = entry.directory});
      }
    }

    for (const auto& [key, entry] : entries) {
      if (!current.entries.contains(key)) {
        batch.events.push_back(FileChangeEvent{
            .relativePath = entry.relativePath, .kind = FileChangeKind::deleted, .directory = entry.directory});
      }
    }

    std::ranges::sort(batch.events, [](const auto& left, const auto& right) {
      return normalizedPathKey(left.relativePath) < normalizedPathKey(right.relativePath);
    });

    entries = std::move(current.entries);

    return batch;
  }

  bool PollingFileWatcher::changed(const WatchedEntry& previous, const WatchedEntry& current)
  {
    return previous.modifiedAt != current.modifiedAt || previous.size != current.size ||
           previous.directory != current.directory;
  }

  PollingFileWatcher::Snapshot PollingFileWatcher::snapshot(std::stop_token stop_token) const
  {
    Snapshot snapshot;
    std::error_code error;
    std::filesystem::recursive_directory_iterator iterator(
        root, std::filesystem::directory_options::skip_permission_denied, error);
    const std::filesystem::recursive_directory_iterator end;

    while (!error && iterator != end) {
      if (stop_token.stop_requested()) {
        snapshot.incomplete = true;

        break;
      }

      const auto path = iterator->path();
      const auto relativePath = std::filesystem::relative(path, root, error);

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

      WatchedEntry entry{.relativePath = relativePath,
                         .modifiedAt = iterator->last_write_time(error),
                         .size = size,
                         .directory = directory};

      if (!error)
        snapshot.entries.emplace(normalizedPathKey(relativePath), std::move(entry));

      iterator.increment(error);
    }

    return snapshot;
  }

} // namespace uburu::filesystem
