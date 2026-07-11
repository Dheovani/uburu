#pragma once

#include "core/storage/storage-service.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

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

  /**
   * Optional repository overrides that inherit any unset value from the global settings.
   */
  struct RepositorySettings
  {
    RepositoryId repositoryId;
    int schemaVersion{1};
    std::optional<std::string> friendlyName;
    std::optional<std::size_t> maximumThreadCount;
    std::optional<std::uintmax_t> maximumFileSizeBytes;
    std::optional<std::size_t> resultLimit;
    std::optional<std::uintmax_t> memoryBudgetBytes;
    std::optional<std::uintmax_t> diskBudgetBytes;
    std::optional<bool> respectGitignore;
    std::optional<bool> includeHiddenFiles;
    std::optional<std::string> relevantExtensions;
  };

  /**
   * Fully resolved settings snapshot used by repository-scoped services.
   */
  struct EffectiveRepositorySettings
  {
    RepositoryId repositoryId;
    int schemaVersion{1};
    std::string friendlyName;
    std::size_t maximumThreadCount{0};
    std::uintmax_t maximumFileSizeBytes{0};
    std::size_t resultLimit{10000};
    std::uintmax_t memoryBudgetBytes{0};
    std::uintmax_t diskBudgetBytes{0};
    bool respectGitignore{true};
    bool includeHiddenFiles{false};
    std::string relevantExtensions;
    bool telemetryEnabled{false};
  };

  [[nodiscard]]
  GlobalSettings defaultGlobalSettings();

  [[nodiscard]]
  GlobalSettings normalizedGlobalSettings(const GlobalSettings& settings);

  [[nodiscard]]
  RepositorySettings defaultRepositorySettings(RepositoryId repositoryId);

  [[nodiscard]]
  RepositorySettings normalizedRepositorySettings(const RepositorySettings& settings);

  [[nodiscard]]
  EffectiveRepositorySettings effectiveRepositorySettings(
    const GlobalSettings& globalSettings,
    const RepositorySettings& repositorySettings);

  [[nodiscard]]
  std::string toPreferenceValue(ThemeMode mode);

  [[nodiscard]]
  std::string toPreferenceValue(UiLanguage language);

  [[nodiscard]]
  ThemeMode themeModeFromPreferenceValue(const std::string& value);

  [[nodiscard]]
  UiLanguage uiLanguageFromPreferenceValue(const std::string& value);

  /**
   * Loads, saves, exports, and imports versioned settings independently from the UI.
   */
  class SettingsService
  {
  public:
    virtual ~SettingsService() = default;

    [[nodiscard]]
    virtual GlobalSettings loadGlobalSettings() const = 0;

    virtual void saveGlobalSettings(const GlobalSettings& settings) = 0;

    [[nodiscard]]
    virtual RepositorySettings loadRepositorySettings(const RepositoryId& repositoryId) const = 0;

    [[nodiscard]]
    virtual EffectiveRepositorySettings resolveRepositorySettings(const RepositoryId& repositoryId) const = 0;

    virtual void saveRepositorySettings(const RepositorySettings& settings) = 0;

    [[nodiscard]]
    virtual std::string exportSettingsAndSavedSearches() const = 0;

    virtual void importSettingsAndSavedSearches(std::string_view text) = 0;
  };

  /**
   * SQLite-backed settings service implementation.
   */
  class StorageSettingsService final : public SettingsService
  {
  public:
    explicit StorageSettingsService(storage::StorageService& storage);

    [[nodiscard]]
    GlobalSettings loadGlobalSettings() const override;

    void saveGlobalSettings(const GlobalSettings& settings) override;

    [[nodiscard]]
    RepositorySettings loadRepositorySettings(const RepositoryId& repositoryId) const override;

    [[nodiscard]]
    EffectiveRepositorySettings resolveRepositorySettings(const RepositoryId& repositoryId) const override;

    void saveRepositorySettings(const RepositorySettings& settings) override;

    [[nodiscard]]
    std::string exportSettingsAndSavedSearches() const override;

    void importSettingsAndSavedSearches(std::string_view text) override;

  private:
    storage::StorageService* storageService{nullptr};
  };

} // namespace uburu::app
