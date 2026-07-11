#pragma once

#include "core/filesystem/file-watcher.hpp"

#include <filesystem>
#include <memory>
#include <vector>

namespace uburu::filesystem
{

  /**
   * Windows watcher backed by ReadDirectoryChangesW when native support is available.
   */
  class WindowsFileWatcher final : public FileWatcher
  {
  public:
    explicit WindowsFileWatcher(std::filesystem::path root);
    ~WindowsFileWatcher() override;

    WindowsFileWatcher(const WindowsFileWatcher&) = delete;
    WindowsFileWatcher& operator=(const WindowsFileWatcher&) = delete;

    [[nodiscard]]
    FileChangeBatch poll(std::stop_token stopToken = {}) override;

  private:
    struct NativeHandle;

    [[nodiscard]]
    FileChangeBatch unavailableBatch() const;

    [[nodiscard]]
    std::filesystem::path relativeFromRoot(const std::filesystem::path& path) const;

    std::filesystem::path root;
    std::unique_ptr<NativeHandle> handle;
    std::vector<unsigned char> buffer;
  };

} // namespace uburu::filesystem
