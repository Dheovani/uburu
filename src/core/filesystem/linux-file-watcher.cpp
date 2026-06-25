#include "core/filesystem/linux-file-watcher.hpp"

#ifdef __linux__

#include <sys/inotify.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <system_error>
#include <utility>

namespace uburu::filesystem
{
  namespace
  {

    constexpr std::size_t bytes_per_kibibyte = 1024U;
    constexpr std::size_t inotify_buffer_kibibytes = 64U;
    constexpr std::size_t inotify_buffer_size = inotify_buffer_kibibytes * bytes_per_kibibyte;
    constexpr std::uint32_t watched_events =
      IN_CREATE |
      IN_MODIFY |
      IN_CLOSE_WRITE |
      IN_ATTRIB |
      IN_DELETE |
      IN_MOVED_FROM |
      IN_MOVED_TO |
      IN_DELETE_SELF |
      IN_MOVE_SELF;

    [[nodiscard]] FileChangeKind map_mask(std::uint32_t mask)
    {
      if ((mask & (IN_CREATE | IN_MOVED_TO)) != 0U)
        return FileChangeKind::created;

      if ((mask & (IN_DELETE | IN_MOVED_FROM | IN_DELETE_SELF | IN_MOVE_SELF)) != 0U)
        return FileChangeKind::deleted;

      return FileChangeKind::modified;
    }

  } // namespace

  LinuxFileWatcher::LinuxFileWatcher(std::filesystem::path root)
    : root_(std::move(root)),
      descriptor_(inotify_init1(IN_NONBLOCK | IN_CLOEXEC)),
      buffer_(inotify_buffer_size)
  {
    if (available())
      add_recursive_watches();
  }

  LinuxFileWatcher::~LinuxFileWatcher()
  {
    if (descriptor_ >= 0)
      close(descriptor_);
  }

  FileChangeBatch LinuxFileWatcher::poll(std::stop_token stop_token)
  {
    if (!available())
      return unavailable_batch();

    FileChangeBatch batch;

    while (!stop_token.stop_requested()) {
      const auto bytes_read = read(descriptor_, buffer_.data(), buffer_.size());

      if (bytes_read < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
          return batch;

        return unavailable_batch();
      }

      if (bytes_read == 0)
        return batch;

      std::size_t offset = 0;

      while (offset < static_cast<std::size_t>(bytes_read)) {
        const auto* event = reinterpret_cast<const inotify_event*>(buffer_.data() + offset);

        if ((event->mask & IN_Q_OVERFLOW) != 0U) {
          batch.events_may_be_incomplete = true;
          batch.requires_rescan = true;

          return batch;
        }

        const auto watched = watched_directories_.find(event->wd);

        if (watched != watched_directories_.end() && event->len > 0) {
          const auto name = std::filesystem::path(event->name);
          const auto absolute_path = watched->second / name;
          const auto directory = (event->mask & IN_ISDIR) != 0U;

          batch.events.push_back(FileChangeEvent{
            .relative_path = relative_from_root(absolute_path),
            .kind = map_mask(event->mask),
            .directory = directory});

          if (directory && (event->mask & (IN_CREATE | IN_MOVED_TO)) != 0U)
            add_directory_watch(absolute_path);
        }

        if ((event->mask & IN_IGNORED) != 0U)
          remove_watch(event->wd);

        offset += sizeof(inotify_event) + event->len;
      }
    }

    batch.events_may_be_incomplete = true;
    batch.requires_rescan = true;

    return batch;
  }

  bool LinuxFileWatcher::available() const
  {
    return descriptor_ >= 0;
  }

  FileChangeBatch LinuxFileWatcher::unavailable_batch() const
  {
    return FileChangeBatch{.events = {}, .events_may_be_incomplete = true, .requires_rescan = true};
  }

  std::filesystem::path LinuxFileWatcher::relative_from_root(const std::filesystem::path& path) const
  {
    std::error_code error;
    auto relative_path = std::filesystem::relative(path, root_, error);

    if (error)
      return path.filename();

    return relative_path;
  }

  void LinuxFileWatcher::add_directory_watch(const std::filesystem::path& directory)
  {
    if (!available())
      return;

    const auto watch_descriptor = inotify_add_watch(descriptor_, directory.c_str(), watched_events);

    if (watch_descriptor >= 0)
      watched_directories_[watch_descriptor] = directory;
  }

  void LinuxFileWatcher::add_recursive_watches()
  {
    add_directory_watch(root_);

    std::error_code error;
    std::filesystem::recursive_directory_iterator iterator(
      root_, std::filesystem::directory_options::skip_permission_denied, error);
    const std::filesystem::recursive_directory_iterator end;

    while (!error && iterator != end) {
      if (iterator->is_directory(error))
        add_directory_watch(iterator->path());

      iterator.increment(error);
    }
  }

  void LinuxFileWatcher::remove_watch(int descriptor)
  {
    watched_directories_.erase(descriptor);
  }

} // namespace uburu::filesystem

#endif
