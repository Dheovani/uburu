#pragma once

#include "core/filesystem/file-watcher.hpp"

#include <filesystem>
#include <memory>
#include <vector>

namespace uburu::filesystem
{

  class WindowsFileWatcher final : public FileWatcher
  {
  public:
    explicit WindowsFileWatcher(std::filesystem::path root);
    ~WindowsFileWatcher() override;

    WindowsFileWatcher(const WindowsFileWatcher&) = delete;
    WindowsFileWatcher& operator=(const WindowsFileWatcher&) = delete;

    [[nodiscard]] FileChangeBatch poll(std::stop_token stop_token = {}) override;

  private:
    struct NativeHandle;

    [[nodiscard]] FileChangeBatch unavailable_batch() const;
    [[nodiscard]] std::filesystem::path relative_from_root(const std::filesystem::path& path) const;

    std::filesystem::path root_;
    std::unique_ptr<NativeHandle> handle_;
    std::vector<unsigned char> buffer_;
  };

} // namespace uburu::filesystem
