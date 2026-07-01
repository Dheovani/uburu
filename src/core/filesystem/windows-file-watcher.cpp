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

    constexpr std::size_t bytesPerKibibyte = 1024U;
    constexpr std::size_t nativeChangeBufferKibibytes = 64U;
    constexpr std::size_t changeBufferSize = nativeChangeBufferKibibytes * bytesPerKibibyte;
    constexpr auto pollWaitStep = std::chrono::milliseconds{5};
    constexpr auto pollWaitTimeout = std::chrono::milliseconds{50};

    [[nodiscard]] FileChangeKind mapAction(DWORD action)
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

    [[nodiscard]] DWORD watchedChanges()
    {
      return FILE_NOTIFY_CHANGE_FILE_NAME
           | FILE_NOTIFY_CHANGE_DIR_NAME
           | FILE_NOTIFY_CHANGE_LAST_WRITE
           | FILE_NOTIFY_CHANGE_SIZE
           | FILE_NOTIFY_CHANGE_ATTRIBUTES;
    }

  } // namespace

  struct WindowsFileWatcher::NativeHandle
  {
    HANDLE directory{INVALID_HANDLE_VALUE};
  };

  WindowsFileWatcher::WindowsFileWatcher(std::filesystem::path root)
    : root(std::move(root)), handle(std::make_unique<NativeHandle>()), buffer(changeBufferSize)
  {
    handle->directory = CreateFileW(root.wstring().c_str(),
                                    FILE_LIST_DIRECTORY,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr,
                                    OPEN_EXISTING,
                                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                                    nullptr);
  }

  WindowsFileWatcher::~WindowsFileWatcher()
  {
    if (handle && handle->directory != INVALID_HANDLE_VALUE) {
      CancelIoEx(handle->directory, nullptr);
      CloseHandle(handle->directory);
    }
  }

  FileChangeBatch WindowsFileWatcher::poll(std::stop_token stop_token)
  {
    if (!handle || handle->directory == INVALID_HANDLE_VALUE)
      return unavailableBatch();

    OVERLAPPED overlapped{};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);

    if (overlapped.hEvent == nullptr)
      return unavailableBatch();

    DWORD bytesReturned = 0;
    const auto started = ReadDirectoryChangesW(handle->directory,
                                               buffer.data(),
                                               static_cast<DWORD>(buffer.size()),
                                               TRUE,
                                               watchedChanges(),
                                               nullptr,
                                               &overlapped,
                                               nullptr);

    if (started == FALSE) {
      CloseHandle(overlapped.hEvent);

      return unavailableBatch();
    }

    const auto deadline = std::chrono::steady_clock::now() + pollWaitTimeout;

    while (!stop_token.stop_requested()) {
      const auto waitResult = WaitForSingleObject(overlapped.hEvent, static_cast<DWORD>(pollWaitStep.count()));

      if (waitResult == WAIT_OBJECT_0)
        break;

      if (waitResult != WAIT_TIMEOUT || std::chrono::steady_clock::now() >= deadline) {
        CancelIoEx(handle->directory, &overlapped);
        GetOverlappedResult(handle->directory, &overlapped, &bytesReturned, TRUE);
        CloseHandle(overlapped.hEvent);

        return {};
      }
    }

    if (stop_token.stop_requested()) {
      CancelIoEx(handle->directory, &overlapped);
      GetOverlappedResult(handle->directory, &overlapped, &bytesReturned, TRUE);
      CloseHandle(overlapped.hEvent);

      return FileChangeBatch{.events = {}, .eventsMayBeIncomplete = true, .requiresRescan = true};
    }

    if (GetOverlappedResult(handle->directory, &overlapped, &bytesReturned, FALSE) == FALSE) {
      CloseHandle(overlapped.hEvent);

      return unavailableBatch();
    }

    CloseHandle(overlapped.hEvent);

    FileChangeBatch batch;

    if (bytesReturned == 0) {
      batch.eventsMayBeIncomplete = true;
      batch.requiresRescan = true;

      return batch;
    }

    std::size_t offset = 0;

    while (offset < bytesReturned) {
      const auto* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(buffer.data() + offset);
      const std::wstring fileName(info->FileName, info->FileNameLength / sizeof(wchar_t));
      const auto absolutePath = root / std::filesystem::path(fileName);
      std::error_code error;

      batch.events.push_back(FileChangeEvent{.relativePath = relativeFromRoot(absolutePath),
                                             .kind = mapAction(info->Action),
                                             .directory = std::filesystem::is_directory(absolutePath, error)});

      if (info->NextEntryOffset == 0)
        break;

      offset += info->NextEntryOffset;
    }

    return batch;
  }

  FileChangeBatch WindowsFileWatcher::unavailableBatch() const
  {
    return FileChangeBatch{.events = {}, .eventsMayBeIncomplete = true, .requiresRescan = true};
  }

  std::filesystem::path WindowsFileWatcher::relativeFromRoot(const std::filesystem::path& path) const
  {
    std::error_code error;
    auto relativePath = std::filesystem::relative(path, root, error);

    if (error)
      return path.filename();

    return relativePath;
  }

} // namespace uburu::filesystem

#endif
