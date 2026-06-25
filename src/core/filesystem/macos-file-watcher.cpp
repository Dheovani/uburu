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

    constexpr CFTimeInterval fsevents_latency_seconds = 0.25;

    [[nodiscard]] bool requires_rescan(FSEventStreamEventFlags flags)
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

    [[nodiscard]] FileChangeKind map_flags(FSEventStreamEventFlags flags)
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
                  std::size_t event_count,
                  void* event_paths,
                  const FSEventStreamEventFlags event_flags[],
                  const FSEventStreamEventId[])
    {
      auto* watcher = static_cast<MacosFileWatcher*>(context);
      auto** paths = static_cast<char**>(event_paths);
      FileChangeBatch batch;

      for (std::size_t index = 0; index < event_count; ++index) {
        const std::filesystem::path path(paths[index]);

        if (requires_rescan(event_flags[index])) {
          batch.events_may_be_incomplete = true;
          batch.requires_rescan = true;
        }

        batch.events.push_back(FileChangeEvent{
          .relative_path = path,
          .kind = map_flags(event_flags[index]),
          .directory = is_directory(event_flags[index])});
      }

      append_macos_events(*watcher, std::move(batch));
    }

  } // namespace

  struct MacosFileWatcher::NativeStream
  {
    FSEventStreamRef stream{nullptr};
    dispatch_queue_t queue{nullptr};
  };

  MacosFileWatcher::MacosFileWatcher(std::filesystem::path root) : root_(std::move(root))
  {
    stream_ = new NativeStream;
    stream_->queue = dispatch_queue_create("uburu.macos-file-watcher", DISPATCH_QUEUE_SERIAL);

    const auto root_string = CFStringCreateWithCString(nullptr, root_.string().c_str(), kCFStringEncodingUTF8);
    const void* values[] = {root_string};
    const auto paths = CFArrayCreate(nullptr, values, 1, &kCFTypeArrayCallBacks);

    FSEventStreamContext context{};
    context.info = this;

    stream_->stream = FSEventStreamCreate(nullptr, callback, &context, paths, kFSEventStreamEventIdSinceNow,
                                          fsevents_latency_seconds,
                                          kFSEventStreamCreateFlagFileEvents);

    if (stream_->stream != nullptr) {
      FSEventStreamSetDispatchQueue(stream_->stream, stream_->queue);
      FSEventStreamStart(stream_->stream);
    }

    CFRelease(paths);
    CFRelease(root_string);
  }

  MacosFileWatcher::~MacosFileWatcher()
  {
    if (stream_ == nullptr)
      return;

    if (stream_->stream != nullptr) {
      FSEventStreamStop(stream_->stream);
      FSEventStreamInvalidate(stream_->stream);
      FSEventStreamRelease(stream_->stream);
    }

    if (stream_->queue != nullptr)
      dispatch_release(stream_->queue);

    delete stream_;
  }

  FileChangeBatch MacosFileWatcher::poll(std::stop_token stop_token)
  {
    if (stop_token.stop_requested())
      return FileChangeBatch{.events = {}, .events_may_be_incomplete = true, .requires_rescan = true};

    std::lock_guard lock(mutex_);
    auto batch = std::move(pending_);
    pending_ = {};

    return batch;
  }

  void append_macos_events(MacosFileWatcher& watcher, FileChangeBatch batch)
  {
    watcher.append(std::move(batch));
  }

  void MacosFileWatcher::append(FileChangeBatch batch)
  {
    std::lock_guard lock(mutex_);

    for (auto& event : batch.events) {
      event.relative_path = relative_from_root(event.relative_path);
      pending_.events.push_back(std::move(event));
    }

    pending_.events_may_be_incomplete = pending_.events_may_be_incomplete || batch.events_may_be_incomplete;
    pending_.requires_rescan = pending_.requires_rescan || batch.requires_rescan;
  }

  std::filesystem::path MacosFileWatcher::relative_from_root(const std::filesystem::path& path) const
  {
    std::error_code error;
    auto relative_path = std::filesystem::relative(path, root_, error);

    if (error)
      return path.filename();

    return relative_path;
  }

} // namespace uburu::filesystem

#endif
