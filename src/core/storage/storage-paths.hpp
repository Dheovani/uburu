#pragma once

#include "shared/types/domain-types.hpp"

#include <filesystem>

namespace uburu::storage
{

  [[nodiscard]] std::filesystem::path defaultStorageDatabasePath();
  [[nodiscard]] StorageMigrationResult migrateStorageDatabase(const std::filesystem::path& sourceDatabase,
                                                              const std::filesystem::path& targetDatabase);

} // namespace uburu::storage
