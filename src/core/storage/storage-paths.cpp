#include "core/storage/storage-paths.hpp"

#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace uburu::storage
{
  namespace
  {

    constexpr std::string_view applicationDirectoryName = "uburu";
    constexpr std::string_view databaseFileName = "uburu.db";
    constexpr std::string_view writeAheadLogSuffix = "-wal";
    constexpr std::string_view sharedMemorySuffix = "-shm";

    [[nodiscard]] std::optional<std::filesystem::path> environmentPath(const char* name)
    {
      const auto* value = std::getenv(name);

      if (value == nullptr || std::string_view{value}.empty())
        return std::nullopt;

      return std::filesystem::path{value};
    }

    [[nodiscard]] std::filesystem::path fallbackStorageRoot()
    {
      return std::filesystem::temp_directory_path() / applicationDirectoryName;
    }

    [[nodiscard]] std::filesystem::path storageRoot()
    {
#if defined(_WIN32)
      if (const auto localAppData = environmentPath("LOCALAPPDATA"))
        return *localAppData / applicationDirectoryName;

      if (const auto appData = environmentPath("APPDATA"))
        return *appData / applicationDirectoryName;

      if (const auto userProfile = environmentPath("USERPROFILE"))
        return *userProfile / "AppData" / "Local" / applicationDirectoryName;

      return fallbackStorageRoot();
#elif defined(__APPLE__)
      if (const auto home = environmentPath("HOME"))
        return *home / "Library" / "Application Support" / applicationDirectoryName;

      return fallbackStorageRoot();
#else
      if (const auto xdgDataHome = environmentPath("XDG_DATA_HOME"))
        return *xdgDataHome / applicationDirectoryName;

      if (const auto home = environmentPath("HOME"))
        return *home / ".local" / "share" / applicationDirectoryName;

      return fallbackStorageRoot();
#endif
    }

    [[nodiscard]] bool copyIfExists(const std::filesystem::path& source, const std::filesystem::path& target)
    {
      if (!std::filesystem::exists(source))
        return false;

      std::filesystem::create_directories(target.parent_path());
      std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing);

      return true;
    }

  } // namespace

  std::filesystem::path defaultStorageDatabasePath()
  {
    return storageRoot() / databaseFileName;
  }

  StorageMigrationResult migrateStorageDatabase(const std::filesystem::path& sourceDatabase,
                                                const std::filesystem::path& targetDatabase)
  {
    if (!std::filesystem::exists(sourceDatabase))
      throw std::runtime_error("source storage database does not exist");

    return StorageMigrationResult{
      .copiedDatabase = copyIfExists(sourceDatabase, targetDatabase),
      .copiedWriteAheadLog = copyIfExists(sourceDatabase.string() + std::string{writeAheadLogSuffix},
                                          targetDatabase.string() + std::string{writeAheadLogSuffix}),
      .copiedSharedMemory = copyIfExists(sourceDatabase.string() + std::string{sharedMemorySuffix},
                                         targetDatabase.string() + std::string{sharedMemorySuffix})};
  }

} // namespace uburu::storage
