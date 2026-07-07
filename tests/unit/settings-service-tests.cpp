#include "app/services/settings-service.hpp"
#include "core/storage/sqlite-storage-service.hpp"
#include "helpers/temporary-paths.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>

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
