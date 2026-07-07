#include "app/services/settings-service.hpp"

#include <charconv>
#include <optional>
#include <string_view>

namespace uburu::app
{
  namespace
  {

    constexpr int currentGlobalSettingsSchemaVersion = 1;
    constexpr std::string_view schemaVersionKey = "global.schemaVersion";
    constexpr std::string_view themeModeKey = "global.themeMode";
    constexpr std::string_view languageKey = "global.language";
    constexpr std::string_view maximumThreadCountKey = "global.maximumThreadCount";
    constexpr std::string_view maximumFileSizeBytesKey = "global.maximumFileSizeBytes";
    constexpr std::string_view resultLimitKey = "global.resultLimit";
    constexpr std::string_view memoryBudgetBytesKey = "global.memoryBudgetBytes";
    constexpr std::string_view diskBudgetBytesKey = "global.diskBudgetBytes";
    constexpr std::string_view telemetryEnabledKey = "global.telemetryEnabled";
    constexpr std::size_t defaultResultLimit = 10000;

    [[nodiscard]] std::string key(std::string_view value)
    {
      return std::string(value);
    }

    [[nodiscard]] std::optional<std::size_t> parseSize(std::string_view value)
    {
      std::size_t parsedValue{0};

      const auto* first = value.data();
      const auto* last = value.data() + value.size();
      const auto [position, error] = std::from_chars(first, last, parsedValue);

      if (error != std::errc{} || position != last)
        return std::nullopt;

      return parsedValue;
    }

    [[nodiscard]] std::optional<std::uintmax_t> parseUnsignedMax(std::string_view value)
    {
      std::uintmax_t parsedValue{0};

      const auto* first = value.data();
      const auto* last = value.data() + value.size();
      const auto [position, error] = std::from_chars(first, last, parsedValue);

      if (error != std::errc{} || position != last)
        return std::nullopt;

      return parsedValue;
    }

    [[nodiscard]] std::optional<int> parseInt(std::string_view value)
    {
      int parsedValue{0};

      const auto* first = value.data();
      const auto* last = value.data() + value.size();
      const auto [position, error] = std::from_chars(first, last, parsedValue);

      if (error != std::errc{} || position != last)
        return std::nullopt;

      return parsedValue;
    }

    [[nodiscard]] bool parseBool(std::string_view value)
    {
      return value == "true" || value == "1";
    }

    [[nodiscard]] std::optional<std::string> preference(const storage::StorageService& storage,
                                                        std::string_view keyValue)
    {
      return storage.preference(std::nullopt, key(keyValue));
    }

    void setPreference(storage::StorageService& storage, std::string_view keyValue, std::string value)
    {
      storage.setPreference(std::nullopt, key(keyValue), value);
    }

  } // namespace

  GlobalSettings defaultGlobalSettings()
  {
    return GlobalSettings{.schemaVersion = currentGlobalSettingsSchemaVersion,
                          .themeMode = ThemeMode::system,
                          .language = UiLanguage::ptBr,
                          .maximumThreadCount = 0,
                          .maximumFileSizeBytes = 0,
                          .resultLimit = defaultResultLimit,
                          .memoryBudgetBytes = 0,
                          .diskBudgetBytes = 0,
                          .telemetryEnabled = false};
  }

  GlobalSettings normalizedGlobalSettings(const GlobalSettings& settings)
  {
    auto normalized = settings;

    normalized.schemaVersion = currentGlobalSettingsSchemaVersion;

    if (normalized.resultLimit == 0)
      normalized.resultLimit = defaultResultLimit;

    return normalized;
  }

  std::string toPreferenceValue(ThemeMode mode)
  {
    switch (mode) {
    case ThemeMode::dark:
      return "dark";
    case ThemeMode::light:
      return "light";
    case ThemeMode::system:
      return "system";
    }

    return "system";
  }

  std::string toPreferenceValue(UiLanguage language)
  {
    switch (language) {
    case UiLanguage::enUs:
      return "en-US";
    case UiLanguage::ptBr:
      return "pt-BR";
    }

    return "pt-BR";
  }

  ThemeMode themeModeFromPreferenceValue(const std::string& value)
  {
    if (value == "dark")
      return ThemeMode::dark;

    if (value == "light")
      return ThemeMode::light;

    return ThemeMode::system;
  }

  UiLanguage uiLanguageFromPreferenceValue(const std::string& value)
  {
    if (value == "en-US")
      return UiLanguage::enUs;

    return UiLanguage::ptBr;
  }

  StorageSettingsService::StorageSettingsService(storage::StorageService& storage) : storageService(&storage) {}

  GlobalSettings StorageSettingsService::loadGlobalSettings() const
  {
    auto settings = defaultGlobalSettings();

    if (const auto value = preference(*storageService, schemaVersionKey))
      settings.schemaVersion = parseInt(*value).value_or(currentGlobalSettingsSchemaVersion);

    if (const auto value = preference(*storageService, themeModeKey))
      settings.themeMode = themeModeFromPreferenceValue(*value);

    if (const auto value = preference(*storageService, languageKey))
      settings.language = uiLanguageFromPreferenceValue(*value);

    if (const auto value = preference(*storageService, maximumThreadCountKey))
      settings.maximumThreadCount = parseSize(*value).value_or(settings.maximumThreadCount);

    if (const auto value = preference(*storageService, maximumFileSizeBytesKey))
      settings.maximumFileSizeBytes = parseUnsignedMax(*value).value_or(settings.maximumFileSizeBytes);

    if (const auto value = preference(*storageService, resultLimitKey))
      settings.resultLimit = parseSize(*value).value_or(settings.resultLimit);

    if (const auto value = preference(*storageService, memoryBudgetBytesKey))
      settings.memoryBudgetBytes = parseUnsignedMax(*value).value_or(settings.memoryBudgetBytes);

    if (const auto value = preference(*storageService, diskBudgetBytesKey))
      settings.diskBudgetBytes = parseUnsignedMax(*value).value_or(settings.diskBudgetBytes);

    if (const auto value = preference(*storageService, telemetryEnabledKey))
      settings.telemetryEnabled = parseBool(*value);

    return normalizedGlobalSettings(settings);
  }

  void StorageSettingsService::saveGlobalSettings(const GlobalSettings& settings)
  {
    const auto normalized = normalizedGlobalSettings(settings);

    setPreference(*storageService, schemaVersionKey, std::to_string(normalized.schemaVersion));
    setPreference(*storageService, themeModeKey, toPreferenceValue(normalized.themeMode));
    setPreference(*storageService, languageKey, toPreferenceValue(normalized.language));
    setPreference(*storageService, maximumThreadCountKey, std::to_string(normalized.maximumThreadCount));
    setPreference(*storageService, maximumFileSizeBytesKey, std::to_string(normalized.maximumFileSizeBytes));
    setPreference(*storageService, resultLimitKey, std::to_string(normalized.resultLimit));
    setPreference(*storageService, memoryBudgetBytesKey, std::to_string(normalized.memoryBudgetBytes));
    setPreference(*storageService, diskBudgetBytesKey, std::to_string(normalized.diskBudgetBytes));
    setPreference(*storageService, telemetryEnabledKey, normalized.telemetryEnabled ? "true" : "false");
  }

} // namespace uburu::app
