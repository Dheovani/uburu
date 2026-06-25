#include "core/filesystem/windows-file-watcher.hpp"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <chrono>
#include <system_error>
#include <utility>

namespace uburu::filesystem
{
  namespace
  {

    constexpr std::size_t bytes_per_kibibyte = 1024U;
    constexpr std::size_t native_change_buffer_kibibytes = 64U;
    constexpr std::size_t change_buffer_size = native_change_buffer_kibibytes * bytes_per_kibibyte;
    constexpr auto poll_wait_step = std::chrono::milliseconds{5};
    constexpr auto poll_wait_timeout = std::chrono::milliseconds{50};

    [[nodiscard]] FileChangeKind map_action(DWORD action)
    {
      switch (action) {
      case FILE_ACTION_ADDED:
      case FILE_ACTION_RENAMED_NEW_NAME:
        return FileChangeKind::created;
      case FILE_ACTION_REMOVED:
      case FILE_ACTION_RENAMED_OLD_NAME:
        return FileChangeKind::deleted;
      default:
        return FileChangeKind::modified;
      }
    }

    [[nodiscard]] DWORD watched_changes()
    {
      return FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE |
             FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_ATTRIBUTES;
    }

  } // namespace

  struct WindowsFileWatcher::NativeHandle
  {
    HANDLE directory{INVALID_HANDLE_VALUE};
  };

  WindowsFileWatcher::WindowsFileWatcher(std::filesystem::path root)
    : root_(std::move(root)), handle_(std::make_unique<NativeHandle>()), buffer_(change_buffer_size)
  {
    handle_->directory = CreateFileW(root_.wstring().c_str(), FILE_LIST_DIRECTORY,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                     OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                     nullptr);
  }

  WindowsFileWatcher::~WindowsFileWatcher()
  {
    if (handle_ && handle_->directory != INVALID_HANDLE_VALUE) {
      CancelIoEx(handle_->directory, nullptr);
      CloseHandle(handle_->directory);
    }
  }

  FileChangeBatch WindowsFileWatcher::poll(std::stop_token stop_token)
  {
    if (!handle_ || handle_->directory == INVALID_HANDLE_VALUE)
      return unavailable_batch();

    OVERLAPPED overlapped{};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    if (overlapped.hEvent == nullptr)
      return unavailable_batch();

    DWORD bytes_returned = 0;
    const auto started = ReadDirectoryChangesW(handle_->directory, buffer_.data(),
                                               static_cast<DWORD>(buffer_.size()), TRUE,
                                               watched_changes(), nullptr, &overlapped, nullptr);

    if (started == FALSE) {
      CloseHandle(overlapped.hEvent);

      return unavailable_batch();
    }

    const auto deadline = std::chrono::steady_clock::now() + poll_wait_timeout;

    while (!stop_token.stop_requested()) {
      const auto wait_result = WaitForSingleObject(overlapped.hEvent, static_cast<DWORD>(poll_wait_step.count()));

      if (wait_result == WAIT_OBJECT_0)
        break;

      if (wait_result != WAIT_TIMEOUT || std::chrono::steady_clock::now() >= deadline) {
        CancelIoEx(handle_->directory, &overlapped);
        GetOverlappedResult(handle_->directory, &overlapped, &bytes_returned, TRUE);
        CloseHandle(overlapped.hEvent);

        return {};
      }
    }

    if (stop_token.stop_requested()) {
      CancelIoEx(handle_->directory, &overlapped);
      GetOverlappedResult(handle_->directory, &overlapped, &bytes_returned, TRUE);
      CloseHandle(overlapped.hEvent);

      return FileChangeBatch{.events = {}, .events_may_be_incomplete = true, .requires_rescan = true};
    }

    if (GetOverlappedResult(handle_->directory, &overlapped, &bytes_returned, FALSE) == FALSE) {
      CloseHandle(overlapped.hEvent);

      return unavailable_batch();
    }

    CloseHandle(overlapped.hEvent);

    FileChangeBatch batch;

    if (bytes_returned == 0) {
      batch.events_may_be_incomplete = true;
      batch.requires_rescan = true;

      return batch;
    }

    std::size_t offset = 0;

    while (offset < bytes_returned) {
      const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(buffer_.data() + offset);
      const std::wstring file_name(info->FileName, info->FileNameLength / sizeof(wchar_t));
      const auto absolute_path = root_ / std::filesystem::path(file_name);
      std::error_code error;

      batch.events.push_back(FileChangeEvent{
        .relative_path = relative_from_root(absolute_path),
        .kind = map_action(info->Action),
        .directory = std::filesystem::is_directory(absolute_path, error)});

      if (info->NextEntryOffset == 0)
        break;

      offset += info->NextEntryOffset;
    }

    return batch;
  }

  FileChangeBatch WindowsFileWatcher::unavailable_batch() const
  {
    return FileChangeBatch{.events = {}, .events_may_be_incomplete = true, .requires_rescan = true};
  }

  std::filesystem::path WindowsFileWatcher::relative_from_root(const std::filesystem::path& path) const
  {
    std::error_code error;
    auto relative_path = std::filesystem::relative(path, root_, error);

    if (error)
      return path.filename();

    return relative_path;
  }

} // namespace uburu::filesystem

#endif
