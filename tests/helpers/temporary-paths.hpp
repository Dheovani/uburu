#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace uburu::tests
{

  /**
   * Builds a process-local unique path under the system temporary directory.
   */
  [[nodiscard]]
  inline std::filesystem::path uniqueTemporaryPath(std::string name)
  {
    static std::atomic_size_t counter{0};

    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto threadHash = std::hash<std::thread::id>{}(std::this_thread::get_id());
    const auto sequence = counter.fetch_add(1, std::memory_order_relaxed);
    auto uniqueName = std::move(name);
    uniqueName += "-";
    uniqueName += std::to_string(timestamp);
    uniqueName += "-";
    uniqueName += std::to_string(threadHash);
    uniqueName += "-";
    uniqueName += std::to_string(sequence);

    return std::filesystem::temp_directory_path() / uniqueName;
  }

  /**
   * RAII temporary directory for isolated tests.
   */
  class TemporaryDirectory
  {
  public:
    explicit TemporaryDirectory(std::string name) : pathValue(uniqueTemporaryPath(std::move(name)))
    {
      std::error_code error;

      std::filesystem::remove_all(pathValue, error);
      std::filesystem::create_directories(pathValue);
    }

    ~TemporaryDirectory()
    {
      std::error_code error;

      std::filesystem::remove_all(pathValue, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    TemporaryDirectory(TemporaryDirectory&&) noexcept = delete;
    TemporaryDirectory& operator=(TemporaryDirectory&&) noexcept = delete;

    [[nodiscard]]
    const std::filesystem::path& path() const
    {
      return pathValue;
    }

  private:
    std::filesystem::path pathValue;
  };

  /**
   * RAII temporary file path that cleans up the file on destruction.
   */
  class TemporaryFile
  {
  public:
    explicit TemporaryFile(std::string name) : pathValue(uniqueTemporaryPath(std::move(name)))
    {
      std::filesystem::create_directories(pathValue.parent_path());
    }

    ~TemporaryFile()
    {
      std::error_code error;

      std::filesystem::remove(pathValue, error);
    }

    TemporaryFile(const TemporaryFile&) = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;
    TemporaryFile(TemporaryFile&&) noexcept = delete;
    TemporaryFile& operator=(TemporaryFile&&) noexcept = delete;

    [[nodiscard]]
    const std::filesystem::path& path() const
    {
      return pathValue;
    }

  private:
    std::filesystem::path pathValue;
  };

  inline void writeFile(const std::filesystem::path& path, std::string_view content)
  {
    std::filesystem::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::binary);
    file << content;
  }

  inline void writeBytes(const std::filesystem::path& path, const std::vector<unsigned char>& bytes)
  {
    std::filesystem::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::binary);

    for (const auto byte : bytes)
      file.put(static_cast<char>(byte));
  }

} // namespace uburu::tests
