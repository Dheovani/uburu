#include "app/services/settings-service.hpp"
#include "core/storage/sqlite-storage-service.hpp"
#include "helpers/temporary-paths.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

TEST_CASE("settings service returns versioned defaults when storage is empty")
{
#if defined(UBURU_HAS_SQLITE)
  uburu::tests::TemporaryDirectory directory("uburu-settings-service-defaults-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  storage.initialize();

  uburu::app::StorageSettingsService settingsService(storage);
  const auto settings = settingsService.loadGlobalSettings();

  CHECK(settings.schemaVersion == 1);
  CHECK(settings.themeMode == uburu::app::ThemeMode::system);
  CHECK(settings.language == uburu::app::UiLanguage::ptBr);
  CHECK(settings.maximumThreadCount == 0);
  CHECK(settings.maximumFileSizeBytes == 0);
  CHECK(settings.resultLimit == 10000);
  CHECK(settings.memoryBudgetBytes == 0);
  CHECK(settings.diskBudgetBytes == 0);
  CHECK_FALSE(settings.telemetryEnabled);
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("settings service persists typed global settings")
{
#if defined(UBURU_HAS_SQLITE)
  uburu::tests::TemporaryDirectory directory("uburu-settings-service-save-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  storage.initialize();

  uburu::app::StorageSettingsService settingsService(storage);
  auto settings = uburu::app::defaultGlobalSettings();

  settings.schemaVersion = 99;
  settings.themeMode = uburu::app::ThemeMode::dark;
  settings.language = uburu::app::UiLanguage::enUs;
  settings.maximumThreadCount = 6;
  settings.maximumFileSizeBytes = 1024U;
  settings.resultLimit = 250;
  settings.memoryBudgetBytes = 2048U;
  settings.diskBudgetBytes = 4096U;
  settings.telemetryEnabled = true;

  settingsService.saveGlobalSettings(settings);

  const auto loaded = settingsService.loadGlobalSettings();

  CHECK(loaded.schemaVersion == 1);
  CHECK(loaded.themeMode == uburu::app::ThemeMode::dark);
  CHECK(loaded.language == uburu::app::UiLanguage::enUs);
  CHECK(loaded.maximumThreadCount == 6);
  CHECK(loaded.maximumFileSizeBytes == 1024U);
  CHECK(loaded.resultLimit == 250);
  CHECK(loaded.memoryBudgetBytes == 2048U);
  CHECK(loaded.diskBudgetBytes == 4096U);
  CHECK(loaded.telemetryEnabled);
  CHECK(storage.preference(std::nullopt, "global.schemaVersion") == "1");
  CHECK(storage.preference(std::nullopt, "global.themeMode") == "dark");
  CHECK(storage.preference(std::nullopt, "global.language") == "en-US");
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("settings service ignores invalid persisted values")
{
#if defined(UBURU_HAS_SQLITE)
  uburu::tests::TemporaryDirectory directory("uburu-settings-service-invalid-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  storage.initialize();
  storage.setPreference(std::nullopt, "global.schemaVersion", "future");
  storage.setPreference(std::nullopt, "global.themeMode", "neon");
  storage.setPreference(std::nullopt, "global.language", "fr-FR");
  storage.setPreference(std::nullopt, "global.maximumThreadCount", "many");
  storage.setPreference(std::nullopt, "global.maximumFileSizeBytes", "-1");
  storage.setPreference(std::nullopt, "global.resultLimit", "0");
  storage.setPreference(std::nullopt, "global.memoryBudgetBytes", "huge");
  storage.setPreference(std::nullopt, "global.diskBudgetBytes", "large");
  storage.setPreference(std::nullopt, "global.telemetryEnabled", "maybe");

  uburu::app::StorageSettingsService settingsService(storage);
  const auto settings = settingsService.loadGlobalSettings();

  CHECK(settings.schemaVersion == 1);
  CHECK(settings.themeMode == uburu::app::ThemeMode::system);
  CHECK(settings.language == uburu::app::UiLanguage::ptBr);
  CHECK(settings.maximumThreadCount == 0);
  CHECK(settings.maximumFileSizeBytes == 0);
  CHECK(settings.resultLimit == 10000);
  CHECK(settings.memoryBudgetBytes == 0);
  CHECK(settings.diskBudgetBytes == 0);
  CHECK_FALSE(settings.telemetryEnabled);
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("settings service clamps excessive global limits")
{
#if defined(UBURU_HAS_SQLITE)
  uburu::tests::TemporaryDirectory directory("uburu-settings-service-global-limits-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  storage.initialize();

  uburu::app::StorageSettingsService settingsService(storage);
  auto settings = uburu::app::defaultGlobalSettings();

  settings.maximumThreadCount = 1000;
  settings.maximumFileSizeBytes = 2ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL;
  settings.resultLimit = 2000000;
  settings.memoryBudgetBytes = 2ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL;
  settings.diskBudgetBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL;

  settingsService.saveGlobalSettings(settings);

  const auto loaded = settingsService.loadGlobalSettings();

  CHECK(loaded.maximumThreadCount == 256);
  CHECK(loaded.maximumFileSizeBytes == 1024ULL * 1024ULL * 1024ULL * 1024ULL);
  CHECK(loaded.resultLimit == 1000000);
  CHECK(loaded.memoryBudgetBytes == 1024ULL * 1024ULL * 1024ULL * 1024ULL);
  CHECK(loaded.diskBudgetBytes == 16ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL);
  CHECK(storage.preference(std::nullopt, "global.maximumThreadCount") == "256");
  CHECK(storage.preference(std::nullopt, "global.resultLimit") == "1000000");
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("settings service resolves repository settings from global settings")
{
#if defined(UBURU_HAS_SQLITE)
  uburu::tests::TemporaryDirectory directory("uburu-settings-service-repository-inheritance-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  storage.initialize();

  uburu::app::StorageSettingsService settingsService(storage);
  auto globalSettings = uburu::app::defaultGlobalSettings();

  globalSettings.maximumThreadCount = 8;
  globalSettings.maximumFileSizeBytes = 4096U;
  globalSettings.resultLimit = 500;
  globalSettings.memoryBudgetBytes = 8192U;
  globalSettings.diskBudgetBytes = 16384U;
  globalSettings.telemetryEnabled = true;

  settingsService.saveGlobalSettings(globalSettings);

  const auto settings = settingsService.resolveRepositorySettings("repository-id");

  CHECK(settings.repositoryId == "repository-id");
  CHECK(settings.schemaVersion == 1);
  CHECK(settings.friendlyName.empty());
  CHECK(settings.maximumThreadCount == 8);
  CHECK(settings.maximumFileSizeBytes == 4096U);
  CHECK(settings.resultLimit == 500);
  CHECK(settings.memoryBudgetBytes == 8192U);
  CHECK(settings.diskBudgetBytes == 16384U);
  CHECK(settings.respectGitignore);
  CHECK_FALSE(settings.includeHiddenFiles);
  CHECK(settings.relevantExtensions.empty());
  CHECK(settings.telemetryEnabled);
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("settings service applies repository overrides over global settings")
{
#if defined(UBURU_HAS_SQLITE)
  uburu::tests::TemporaryDirectory directory("uburu-settings-service-repository-override-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  storage.initialize();

  uburu::app::StorageSettingsService settingsService(storage);
  auto globalSettings = uburu::app::defaultGlobalSettings();
  auto repositorySettings = uburu::app::defaultRepositorySettings("repository-id");

  globalSettings.maximumThreadCount = 8;
  globalSettings.maximumFileSizeBytes = 4096U;
  globalSettings.resultLimit = 500;
  globalSettings.memoryBudgetBytes = 8192U;
  globalSettings.diskBudgetBytes = 16384U;
  globalSettings.telemetryEnabled = true;

  repositorySettings.friendlyName = "Product repository";
  repositorySettings.maximumThreadCount = 2;
  repositorySettings.maximumFileSizeBytes = 1024U;
  repositorySettings.resultLimit = 120;
  repositorySettings.memoryBudgetBytes = 2048U;
  repositorySettings.diskBudgetBytes = 3072U;
  repositorySettings.respectGitignore = false;
  repositorySettings.includeHiddenFiles = true;
  repositorySettings.relevantExtensions = "cpp,hpp,qml";

  settingsService.saveGlobalSettings(globalSettings);
  settingsService.saveRepositorySettings(repositorySettings);

  const auto loaded = settingsService.loadRepositorySettings("repository-id");
  const auto effective = settingsService.resolveRepositorySettings("repository-id");

  REQUIRE(loaded.friendlyName.has_value());
  REQUIRE(loaded.maximumThreadCount.has_value());
  REQUIRE(loaded.respectGitignore.has_value());
  CHECK(*loaded.friendlyName == "Product repository");
  CHECK(*loaded.maximumThreadCount == 2);
  CHECK_FALSE(*loaded.respectGitignore);
  CHECK(effective.friendlyName == "Product repository");
  CHECK(effective.maximumThreadCount == 2);
  CHECK(effective.maximumFileSizeBytes == 1024U);
  CHECK(effective.resultLimit == 120);
  CHECK(effective.memoryBudgetBytes == 2048U);
  CHECK(effective.diskBudgetBytes == 3072U);
  CHECK_FALSE(effective.respectGitignore);
  CHECK(effective.includeHiddenFiles);
  CHECK(effective.relevantExtensions == "cpp,hpp,qml");
  CHECK(effective.telemetryEnabled);
  CHECK(storage.preference("repository-id", "repository.maximumThreadCount") == "2");
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("settings service clamps excessive repository overrides")
{
#if defined(UBURU_HAS_SQLITE)
  uburu::tests::TemporaryDirectory directory("uburu-settings-service-repository-limits-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  storage.initialize();

  uburu::app::StorageSettingsService settingsService(storage);
  auto repositorySettings = uburu::app::defaultRepositorySettings("repository-id");

  repositorySettings.maximumThreadCount = 1000;
  repositorySettings.maximumFileSizeBytes = 2ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL;
  repositorySettings.resultLimit = 2000000;
  repositorySettings.memoryBudgetBytes = 2ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL;
  repositorySettings.diskBudgetBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL;

  settingsService.saveRepositorySettings(repositorySettings);

  const auto loaded = settingsService.loadRepositorySettings("repository-id");

  REQUIRE(loaded.maximumThreadCount.has_value());
  REQUIRE(loaded.maximumFileSizeBytes.has_value());
  REQUIRE(loaded.resultLimit.has_value());
  REQUIRE(loaded.memoryBudgetBytes.has_value());
  REQUIRE(loaded.diskBudgetBytes.has_value());
  CHECK(*loaded.maximumThreadCount == 256);
  CHECK(*loaded.maximumFileSizeBytes == 1024ULL * 1024ULL * 1024ULL * 1024ULL);
  CHECK(*loaded.resultLimit == 1000000);
  CHECK(*loaded.memoryBudgetBytes == 1024ULL * 1024ULL * 1024ULL * 1024ULL);
  CHECK(*loaded.diskBudgetBytes == 16ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL);
  CHECK(storage.preference("repository-id", "repository.maximumThreadCount") == "256");
  CHECK(storage.preference("repository-id", "repository.resultLimit") == "1000000");
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("settings service ignores invalid repository overrides")
{
#if defined(UBURU_HAS_SQLITE)
  uburu::tests::TemporaryDirectory directory("uburu-settings-service-repository-invalid-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  storage.initialize();

  uburu::app::StorageSettingsService settingsService(storage);
  auto globalSettings = uburu::app::defaultGlobalSettings();

  globalSettings.maximumThreadCount = 4;
  globalSettings.resultLimit = 300;

  settingsService.saveGlobalSettings(globalSettings);
  storage.setPreference("repository-id", "repository.maximumThreadCount", "many");
  storage.setPreference("repository-id", "repository.resultLimit", "0");
  storage.setPreference("repository-id", "repository.respectGitignore", "sometimes");
  storage.setPreference("repository-id", "repository.includeHiddenFiles", "unknown");

  const auto loaded = settingsService.loadRepositorySettings("repository-id");
  const auto effective = settingsService.resolveRepositorySettings("repository-id");

  CHECK_FALSE(loaded.maximumThreadCount.has_value());
  CHECK_FALSE(loaded.resultLimit.has_value());
  CHECK_FALSE(loaded.respectGitignore.has_value());
  CHECK_FALSE(loaded.includeHiddenFiles.has_value());
  CHECK(effective.maximumThreadCount == 4);
  CHECK(effective.resultLimit == 300);
  CHECK(effective.respectGitignore);
  CHECK_FALSE(effective.includeHiddenFiles);
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("settings service exports and imports global settings with saved searches")
{
#if defined(UBURU_HAS_SQLITE)
  uburu::tests::TemporaryDirectory sourceDirectory("uburu-settings-service-export-source-test");
  uburu::tests::TemporaryDirectory targetDirectory("uburu-settings-service-export-target-test");
  uburu::storage::SQLiteStorageService sourceStorage(sourceDirectory.path() / "uburu.db");
  uburu::storage::SQLiteStorageService targetStorage(targetDirectory.path() / "uburu.db");

  sourceStorage.initialize();
  targetStorage.initialize();

  uburu::app::StorageSettingsService sourceSettingsService(sourceStorage);
  uburu::app::StorageSettingsService targetSettingsService(targetStorage);
  auto settings = uburu::app::defaultGlobalSettings();

  settings.themeMode = uburu::app::ThemeMode::dark;
  settings.language = uburu::app::UiLanguage::enUs;
  settings.maximumThreadCount = 4;
  settings.resultLimit = 512;
  settings.telemetryEnabled = true;

  sourceSettingsService.saveGlobalSettings(settings);
  sourceStorage.saveSearch(
    uburu::SavedSearch{.name = "code search",
                       .root = sourceDirectory.path(),
                       .expression = "needle \"with quotes\"",
                       .savedAt = std::chrono::system_clock::time_point{std::chrono::milliseconds{42}}});

  const auto exportedSettings = sourceSettingsService.exportSettingsAndSavedSearches();

  REQUIRE(exportedSettings.find("uburu-settings-export") != std::string::npos);

  targetSettingsService.importSettingsAndSavedSearches(exportedSettings);

  const auto importedSettings = targetSettingsService.loadGlobalSettings();
  const auto importedSearches = targetStorage.savedSearches();

  CHECK(importedSettings.themeMode == uburu::app::ThemeMode::dark);
  CHECK(importedSettings.language == uburu::app::UiLanguage::enUs);
  CHECK(importedSettings.maximumThreadCount == 4);
  CHECK(importedSettings.resultLimit == 512);
  CHECK(importedSettings.telemetryEnabled);
  REQUIRE(importedSearches.size() == 1);
  CHECK(importedSearches.front().name == "code search");
  CHECK(importedSearches.front().root == sourceDirectory.path());
  CHECK(importedSearches.front().expression == "needle \"with quotes\"");
  CHECK(importedSearches.front().savedAt == std::chrono::system_clock::time_point{std::chrono::milliseconds{42}});
#else
  SUCCEED("SQLite is not available in this build");
#endif
}

TEST_CASE("settings service rejects unsupported settings exports")
{
#if defined(UBURU_HAS_SQLITE)
  uburu::tests::TemporaryDirectory directory("uburu-settings-service-import-invalid-test");
  uburu::storage::SQLiteStorageService storage(directory.path() / "uburu.db");

  storage.initialize();

  uburu::app::StorageSettingsService settingsService(storage);

  CHECK_THROWS(settingsService.importSettingsAndSavedSearches("{\"format\":\"other\",\"version\":1}"));
#else
  SUCCEED("SQLite is not available in this build");
#endif
}
