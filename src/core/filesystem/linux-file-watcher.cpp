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

    constexpr std::size_t bytesPerKibibyte = 1024U;
    constexpr std::size_t inotifyBufferKibibytes = 64U;
    constexpr std::size_t inotifyBufferSize = inotifyBufferKibibytes * bytesPerKibibyte;
    constexpr std::uint32_t watchedEvents = IN_CREATE | IN_MODIFY | IN_CLOSE_WRITE | IN_ATTRIB | IN_DELETE |
                                            IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF | IN_MOVE_SELF;

    [[nodiscard]] FileChangeKind mapMask(std::uint32_t mask)
    {
      if ((mask & (IN_CREATE | IN_MOVED_TO)) != 0U)
        return FileChangeKind::created;

      if ((mask & (IN_DELETE | IN_MOVED_FROM | IN_DELETE_SELF | IN_MOVE_SELF)) != 0U)
        return FileChangeKind::deleted;

      return FileChangeKind::modified;
    }

  } // namespace

  LinuxFileWatcher::LinuxFileWatcher(std::filesystem::path root)
      : root(std::move(root)), descriptor(inotify_init1(IN_NONBLOCK | IN_CLOEXEC)), buffer(inotifyBufferSize)
  {
    if (available())
      addRecursiveWatches();
  }

  LinuxFileWatcher::~LinuxFileWatcher()
  {
    if (descriptor >= 0)
      close(descriptor);
  }

  FileChangeBatch LinuxFileWatcher::poll(std::stop_token stop_token)
  {
    if (!available())
      return unavailableBatch();

    FileChangeBatch batch;

    while (!stop_token.stop_requested()) {
      const auto bytesRead = read(descriptor, buffer.data(), buffer.size());

      if (bytesRead < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
          return batch;

        return unavailableBatch();
      }

      if (bytesRead == 0)
        return batch;

      std::size_t offset = 0;

      while (offset < static_cast<std::size_t>(bytesRead)) {
        const auto* event = reinterpret_cast<const inotify_event*>(buffer.data() + offset);

        if ((event->mask & IN_Q_OVERFLOW) != 0U) {
          batch.eventsMayBeIncomplete = true;
          batch.requiresRescan = true;

          return batch;
        }

        const auto watched = watchedDirectories.find(event->wd);

        if (watched != watchedDirectories.end() && event->len > 0) {
          const auto name = std::filesystem::path(event->name);
          const auto absolutePath = watched->second / name;
          const auto directory = (event->mask & IN_ISDIR) != 0U;

          batch.events.push_back(FileChangeEvent{
              .relativePath = relativeFromRoot(absolutePath), .kind = mapMask(event->mask), .directory = directory});

          if (directory && (event->mask & (IN_CREATE | IN_MOVED_TO)) != 0U)
            addDirectoryWatch(absolutePath);
        }

        if ((event->mask & IN_IGNORED) != 0U)
          removeWatch(event->wd);

        offset += sizeof(inotify_event) + event->len;
      }
    }

    batch.eventsMayBeIncomplete = true;
    batch.requiresRescan = true;

    return batch;
  }

  bool LinuxFileWatcher::available() const
  {
    return descriptor >= 0;
  }

  FileChangeBatch LinuxFileWatcher::unavailableBatch() const
  {
    return FileChangeBatch{.events = {}, .eventsMayBeIncomplete = true, .requiresRescan = true};
  }

  std::filesystem::path LinuxFileWatcher::relativeFromRoot(const std::filesystem::path& path) const
  {
    std::error_code error;
    auto relativePath = std::filesystem::relative(path, root, error);

    if (error)
      return path.filename();

    return relativePath;
  }

  void LinuxFileWatcher::addDirectoryWatch(const std::filesystem::path& directory)
  {
    if (!available())
      return;

    const auto watchDescriptor = inotify_add_watch(descriptor, directory.c_str(), watchedEvents);

    if (watchDescriptor >= 0)
      watchedDirectories[watchDescriptor] = directory;
  }

  void LinuxFileWatcher::addRecursiveWatches()
  {
    addDirectoryWatch(root);

    std::error_code error;
    std::filesystem::recursive_directory_iterator iterator(
        root, std::filesystem::directory_options::skip_permission_denied, error);
    const std::filesystem::recursive_directory_iterator end;

    while (!error && iterator != end) {
      if (iterator->is_directory(error))
        addDirectoryWatch(iterator->path());

      iterator.increment(error);
    }
  }

  void LinuxFileWatcher::removeWatch(int descriptor)
  {
    watchedDirectories.erase(descriptor);
  }

} // namespace uburu::filesystem

#endif
