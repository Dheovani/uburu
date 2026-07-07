#pragma once

#include "core/storage/storage-service.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace uburu::app
{

  enum class ThemeMode
  {
    system,
    dark,
    light
  };

  enum class UiLanguage
  {
    ptBr,
    enUs
  };

  struct GlobalSettings
  {
    int schemaVersion{1};
    ThemeMode themeMode{ThemeMode::system};
    UiLanguage language{UiLanguage::ptBr};
    std::size_t maximumThreadCount{0};
    std::uintmax_t maximumFileSizeBytes{0};
    std::size_t resultLimit{10000};
    std::uintmax_t memoryBudgetBytes{0};
    std::uintmax_t diskBudgetBytes{0};
    bool telemetryEnabled{false};
  };

  [[nodiscard]] GlobalSettings defaultGlobalSettings();
  [[nodiscard]] GlobalSettings normalizedGlobalSettings(const GlobalSettings& settings);
  [[nodiscard]] std::string toPreferenceValue(ThemeMode mode);
  [[nodiscard]] std::string toPreferenceValue(UiLanguage language);
  [[nodiscard]] ThemeMode themeModeFromPreferenceValue(const std::string& value);
  [[nodiscard]] UiLanguage uiLanguageFromPreferenceValue(const std::string& value);

  class SettingsService
  {
  public:
    virtual ~SettingsService() = default;
    [[nodiscard]] virtual GlobalSettings loadGlobalSettings() const = 0;
    virtual void saveGlobalSettings(const GlobalSettings& settings) = 0;
  };

  class StorageSettingsService final : public SettingsService
  {
  public:
    explicit StorageSettingsService(storage::StorageService& storage);

    [[nodiscard]] GlobalSettings loadGlobalSettings() const override;
    void saveGlobalSettings(const GlobalSettings& settings) override;

  private:
    storage::StorageService* storageService{nullptr};
  };

} // namespace uburu::app
