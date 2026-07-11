#pragma once

#include "shared/types/domain-types.hpp"

#include <filesystem>

namespace uburu::storage
{

  /**
   * Ensures that local storage is private to the current user when the platform allows it.
   */
  void ensurePrivateStorageDirectory(const std::filesystem::path& directory);

  [[nodiscard]]
  std::filesystem::path defaultStorageDatabasePath();

  /**
   * Copies a database to the private storage location without deleting the source.
   */
  [[nodiscard]]
  StorageMigrationResult migrateStorageDatabase(
    const std::filesystem::path& sourceDatabase,
    const std::filesystem::path& targetDatabase);

} // namespace uburu::storage
