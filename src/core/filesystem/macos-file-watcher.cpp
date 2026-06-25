#include "core/filesystem/macos-file-watcher.hpp"

#ifdef __APPLE__

#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>

#include <system_error>
#include <utility>

namespace uburu::filesystem
{
  namespace
  {

    constexpr CFTimeInterval fseventsLatencySeconds = 0.25;

    [[nodiscard]] bool requiresRescan(FSEventStreamEventFlags flags)
    {
      return (flags & kFSEventStreamEventFlagMustScanSubDirs) != 0U ||
             (flags & kFSEventStreamEventFlagUserDropped) != 0U ||
             (flags & kFSEventStreamEventFlagKernelDropped) != 0U ||
             (flags & kFSEventStreamEventFlagEventIdsWrapped) != 0U ||
             (flags & kFSEventStreamEventFlagRootChanged) != 0U;
    }

    [[nodiscard]] bool is_directory(FSEventStreamEventFlags flags)
    {
      return (flags & kFSEventStreamEventFlagItemIsDir) != 0U;
    }

    [[nodiscard]] FileChangeKind mapFlags(FSEventStreamEventFlags flags)
    {
      if ((flags & kFSEventStreamEventFlagItemCreated) != 0U)
        return FileChangeKind::created;

      if ((flags & kFSEventStreamEventFlagItemRemoved) != 0U ||
          (flags & kFSEventStreamEventFlagItemRenamed) != 0U)
        return FileChangeKind::deleted;

      return FileChangeKind::modified;
    }

    void callback(ConstFSEventStreamRef,
                  void* context,
                  std::size_t eventCount,
                  void* eventPaths,
                  const FSEventStreamEventFlags eventFlags[],
                  const FSEventStreamEventId[])
    {
      auto* watcher = static_cast<MacosFileWatcher*>(context);
      auto** paths = static_cast<char**>(eventPaths);
      FileChangeBatch batch;

      for (std::size_t index = 0; index < eventCount; ++index) {
        const std::filesystem::path path(paths[index]);

        if (requiresRescan(eventFlags[index])) {
          batch.eventsMayBeIncomplete = true;
          batch.requiresRescan = true;
        }

        batch.events.push_back(FileChangeEvent{
          .relativePath = path,
          .kind = mapFlags(eventFlags[index]),
          .directory = is_directory(eventFlags[index])});
      }

      appendMacosEvents(*watcher, std::move(batch));
    }

  } // namespace

  struct MacosFileWatcher::NativeStream
  {
    FSEventStreamRef stream{nullptr};
    dispatch_queue_t queue{nullptr};
  };

  MacosFileWatcher::MacosFileWatcher(std::filesystem::path root) : root(std::move(root))
  {
    stream = new NativeStream;
    stream->queue = dispatch_queue_create("uburu.macos-file-watcher", DISPATCH_QUEUE_SERIAL);

    const auto rootString = CFStringCreateWithCString(nullptr, root.string().c_str(), kCFStringEncodingUTF8);
    const void* values[] = {rootString};
    const auto paths = CFArrayCreate(nullptr, values, 1, &kCFTypeArrayCallBacks);

    FSEventStreamContext context{};
    context.info = this;

    stream->stream = FSEventStreamCreate(nullptr, callback, &context, paths, kFSEventStreamEventIdSinceNow,
                                          fseventsLatencySeconds,
                                          kFSEventStreamCreateFlagFileEvents);

    if (stream->stream != nullptr) {
      FSEventStreamSetDispatchQueue(stream->stream, stream->queue);
      FSEventStreamStart(stream->stream);
    }

    CFRelease(paths);
    CFRelease(rootString);
  }

  MacosFileWatcher::~MacosFileWatcher()
  {
    if (stream == nullptr)
      return;

    if (stream->stream != nullptr) {
      FSEventStreamStop(stream->stream);
      FSEventStreamInvalidate(stream->stream);
      FSEventStreamRelease(stream->stream);
    }

    if (stream->queue != nullptr)
      dispatch_release(stream->queue);

    delete stream;
  }

  FileChangeBatch MacosFileWatcher::poll(std::stop_token stop_token)
  {
    if (stop_token.stop_requested())
      return FileChangeBatch{.events = {}, .eventsMayBeIncomplete = true, .requiresRescan = true};

    std::lock_guard lock(mutex);
    auto batch = std::move(pending);
    pending = {};

    return batch;
  }

  void appendMacosEvents(MacosFileWatcher& watcher, FileChangeBatch batch)
  {
    watcher.append(std::move(batch));
  }

  void MacosFileWatcher::append(FileChangeBatch batch)
  {
    std::lock_guard lock(mutex);

    for (auto& event : batch.events) {
      event.relativePath = relativeFromRoot(event.relativePath);
      pending.events.push_back(std::move(event));
    }

    pending.eventsMayBeIncomplete = pending.eventsMayBeIncomplete || batch.eventsMayBeIncomplete;
    pending.requiresRescan = pending.requiresRescan || batch.requiresRescan;
  }

  std::filesystem::path MacosFileWatcher::relativeFromRoot(const std::filesystem::path& path) const
  {
    std::error_code error;
    auto relativePath = std::filesystem::relative(path, root, error);

    if (error)
      return path.filename();

    return relativePath;
  }

} // namespace uburu::filesystem

#endif
