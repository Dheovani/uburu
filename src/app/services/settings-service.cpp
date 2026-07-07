#include "app/services/settings-service.hpp"

#include <charconv>
#include <chrono>
#include <filesystem>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

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
    constexpr std::string_view repositorySchemaVersionKey = "repository.schemaVersion";
    constexpr std::string_view repositoryFriendlyNameKey = "repository.friendlyName";
    constexpr std::string_view repositoryMaximumThreadCountKey = "repository.maximumThreadCount";
    constexpr std::string_view repositoryMaximumFileSizeBytesKey = "repository.maximumFileSizeBytes";
    constexpr std::string_view repositoryResultLimitKey = "repository.resultLimit";
    constexpr std::string_view repositoryMemoryBudgetBytesKey = "repository.memoryBudgetBytes";
    constexpr std::string_view repositoryDiskBudgetBytesKey = "repository.diskBudgetBytes";
    constexpr std::string_view repositoryRespectGitignoreKey = "repository.respectGitignore";
    constexpr std::string_view repositoryIncludeHiddenFilesKey = "repository.includeHiddenFiles";
    constexpr std::string_view repositoryRelevantExtensionsKey = "repository.relevantExtensions";
    constexpr std::size_t defaultResultLimit = 10000;
    constexpr std::size_t maximumThreadCountLimit = 256;
    constexpr std::uintmax_t maximumFileSizeLimitBytes = 1024ULL * 1024ULL * 1024ULL * 1024ULL;
    constexpr std::size_t maximumResultLimit = 1000000;
    constexpr std::uintmax_t maximumMemoryBudgetLimitBytes = 1024ULL * 1024ULL * 1024ULL * 1024ULL;
    constexpr std::uintmax_t maximumDiskBudgetLimitBytes = 16ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL;
    constexpr bool defaultRespectGitignore = true;
    constexpr bool defaultIncludeHiddenFiles = false;
    constexpr std::string_view exportFormatName = "uburu-settings-export";
    constexpr int exportFormatVersion = 1;

    struct SettingsExportPayload
    {
      GlobalSettings globalSettings;
      std::vector<SavedSearch> savedSearches;
    };

    class JsonReader
    {
    public:
      explicit JsonReader(std::string_view source) : text(source) {}

      void expect(char expected)
      {
        skipWhitespace();

        if (position >= text.size() || text[position] != expected)
          throw std::invalid_argument("Invalid settings export format");

        ++position;
      }

      [[nodiscard]]
      bool consume(char expected)
      {
        skipWhitespace();

        if (position >= text.size() || text[position] != expected)
          return false;

        ++position;

        return true;
      }

      void expectEnd()
      {
        skipWhitespace();

        if (position != text.size())
          throw std::invalid_argument("Invalid trailing data in settings export");
      }

      void expectKey(std::string_view expected)
      {
        const auto actual = readString();

        if (actual != expected)
          throw std::invalid_argument("Unexpected settings export key");

        expect(':');
      }

      [[nodiscard]]
      std::string readString()
      {
        expect('"');

        std::string value;

        while (position < text.size()) {
          const auto current = text[position++];

          if (current == '"')
            return value;

          if (current != '\\') {
            value.push_back(current);
            continue;
          }

          if (position >= text.size())
            throw std::invalid_argument("Invalid escaped string in settings export");

          const auto escaped = text[position++];

          switch (escaped) {
          case '"':
          case '\\':
          case '/':
            value.push_back(escaped);
            break;
          case 'n':
            value.push_back('\n');
            break;
          case 'r':
            value.push_back('\r');
            break;
          case 't':
            value.push_back('\t');
            break;
          default:
            throw std::invalid_argument("Unsupported escaped string in settings export");
          }
        }

        throw std::invalid_argument("Unterminated string in settings export");
      }

      [[nodiscard]]
      std::uintmax_t readUnsignedMax()
      {
        skipWhitespace();

        const auto begin = position;

        while (position < text.size() && text[position] >= '0' && text[position] <= '9')
          ++position;

        if (begin == position)
          throw std::invalid_argument("Expected unsigned integer in settings export");

        std::uintmax_t value{0};
        const auto* first = text.data() + begin;
        const auto* last = text.data() + position;
        const auto [parsedPosition, error] = std::from_chars(first, last, value);

        if (error != std::errc{} || parsedPosition != last)
          throw std::invalid_argument("Invalid unsigned integer in settings export");

        return value;
      }

      [[nodiscard]]
      int readInt()
      {
        const auto value = readUnsignedMax();

        if (value > static_cast<std::uintmax_t>(std::numeric_limits<int>::max()))
          throw std::invalid_argument("Integer is too large in settings export");

        return static_cast<int>(value);
      }

      [[nodiscard]]
      bool readBool()
      {
        skipWhitespace();

        if (startsWith("true")) {
          position += std::string_view("true").size();
          return true;
        }

        if (startsWith("false")) {
          position += std::string_view("false").size();
          return false;
        }

        throw std::invalid_argument("Expected boolean in settings export");
      }

    private:
      [[nodiscard]]
      bool startsWith(std::string_view value) const
      {
        return text.substr(position, value.size()) == value;
      }

      void skipWhitespace()
      {
        while (position < text.size() &&
               (text[position] == ' ' || text[position] == '\n' || text[position] == '\r' || text[position] == '\t'))
          ++position;
      }

      std::string_view text;
      std::size_t position{0};
    };

    [[nodiscard]]
    std::string key(std::string_view value)
    {
      return std::string(value);
    }

    [[nodiscard]]
    std::optional<std::size_t> parseSize(std::string_view value)
    {
      std::size_t parsedValue{0};

      const auto* first = value.data();
      const auto* last = value.data() + value.size();
      const auto [position, error] = std::from_chars(first, last, parsedValue);

      if (error != std::errc{} || position != last)
        return std::nullopt;

      return parsedValue;
    }

    [[nodiscard]]
    std::optional<std::uintmax_t> parseUnsignedMax(std::string_view value)
    {
      std::uintmax_t parsedValue{0};

      const auto* first = value.data();
      const auto* last = value.data() + value.size();
      const auto [position, error] = std::from_chars(first, last, parsedValue);

      if (error != std::errc{} || position != last)
        return std::nullopt;

      return parsedValue;
    }

    [[nodiscard]]
    std::optional<int> parseInt(std::string_view value)
    {
      int parsedValue{0};

      const auto* first = value.data();
      const auto* last = value.data() + value.size();
      const auto [position, error] = std::from_chars(first, last, parsedValue);

      if (error != std::errc{} || position != last)
        return std::nullopt;

      return parsedValue;
    }

    [[nodiscard]]
    bool parseBool(std::string_view value)
    {
      return value == "true" || value == "1";
    }

    [[nodiscard]]
    std::optional<bool> parseOptionalBool(std::string_view value)
    {
      if (value == "true" || value == "1")
        return true;

      if (value == "false" || value == "0")
        return false;

      return std::nullopt;
    }

    [[nodiscard]]
    std::optional<std::string> preference(const storage::StorageService& storage, std::string_view keyValue)
    {
      return storage.preference(std::nullopt, key(keyValue));
    }

    void setPreference(storage::StorageService& storage, std::string_view keyValue, std::string value)
    {
      storage.setPreference(std::nullopt, key(keyValue), value);
    }

    [[nodiscard]]
    std::optional<std::string> repositoryPreference(const storage::StorageService& storage,
                                                    const RepositoryId& repositoryId,
                                                    std::string_view keyValue)
    {
      return storage.preference(repositoryId, key(keyValue));
    }

    void setRepositoryPreference(storage::StorageService& storage,
                                 const RepositoryId& repositoryId,
                                 std::string_view keyValue,
                                 std::string value)
    {
      storage.setPreference(repositoryId, key(keyValue), value);
    }

    [[nodiscard]]
    std::string valueOrEmpty(const std::optional<std::string>& value)
    {
      return value.value_or("");
    }

    [[nodiscard]]
    std::size_t limitedSize(std::size_t value, std::size_t maximumValue)
    {
      if (value > maximumValue)
        return maximumValue;

      return value;
    }

    [[nodiscard]]
    std::optional<std::size_t> limitedOptionalSize(std::optional<std::size_t> value, std::size_t maximumValue)
    {
      if (!value)
        return std::nullopt;

      return limitedSize(*value, maximumValue);
    }

    [[nodiscard]]
    std::uintmax_t limitedUnsignedMax(std::uintmax_t value, std::uintmax_t maximumValue)
    {
      if (value > maximumValue)
        return maximumValue;

      return value;
    }

    [[nodiscard]]
    std::optional<std::uintmax_t> limitedOptionalUnsignedMax(std::optional<std::uintmax_t> value,
                                                             std::uintmax_t maximumValue)
    {
      if (!value)
        return std::nullopt;

      return limitedUnsignedMax(*value, maximumValue);
    }

    [[nodiscard]]
    std::uintmax_t toUnixMilliseconds(std::chrono::system_clock::time_point timePoint)
    {
      const auto duration = timePoint.time_since_epoch();
      const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
      const auto count = milliseconds.count();

      if (count < 0)
        return 0;

      return static_cast<std::uintmax_t>(count);
    }

    [[nodiscard]]
    std::chrono::system_clock::time_point fromUnixMilliseconds(std::uintmax_t milliseconds)
    {
      using MillisecondsRep = std::chrono::milliseconds::rep;

      constexpr auto maximumMilliseconds = static_cast<std::uintmax_t>(std::numeric_limits<MillisecondsRep>::max());
      const auto safeMilliseconds = milliseconds > maximumMilliseconds ? maximumMilliseconds : milliseconds;

      return std::chrono::system_clock::time_point(
        std::chrono::milliseconds(static_cast<MillisecondsRep>(safeMilliseconds)));
    }

    [[nodiscard]]
    std::string pathToUtf8(const std::filesystem::path& path)
    {
      const auto text = path.generic_u8string();

      return std::string(reinterpret_cast<const char*>(text.data()), text.size());
    }

    [[nodiscard]]
    std::filesystem::path pathFromUtf8(std::string_view value)
    {
      std::u8string text;

      text.reserve(value.size());

      for (const auto character : value)
        text.push_back(static_cast<char8_t>(static_cast<unsigned char>(character)));

      return std::filesystem::path(text);
    }

    [[nodiscard]]
    std::string escapedJsonString(std::string_view value)
    {
      std::string escaped;

      escaped.reserve(value.size() + 2);
      escaped.push_back('"');

      for (const auto character : value) {
        switch (character) {
        case '"':
          escaped += "\\\"";
          break;
        case '\\':
          escaped += "\\\\";
          break;
        case '\n':
          escaped += "\\n";
          break;
        case '\r':
          escaped += "\\r";
          break;
        case '\t':
          escaped += "\\t";
          break;
        default:
          escaped.push_back(character);
          break;
        }
      }

      escaped.push_back('"');

      return escaped;
    }

    void appendJsonStringField(std::string& output, std::string_view keyValue, std::string_view value)
    {
      output += escapedJsonString(keyValue);
      output += ':';
      output += escapedJsonString(value);
    }

    void appendJsonUnsignedField(std::string& output, std::string_view keyValue, std::uintmax_t value)
    {
      output += escapedJsonString(keyValue);
      output += ':';
      output += std::to_string(value);
    }

    void appendJsonBoolField(std::string& output, std::string_view keyValue, bool value)
    {
      output += escapedJsonString(keyValue);
      output += ':';
      output += value ? std::string_view("true") : std::string_view("false");
    }

    [[nodiscard]]
    GlobalSettings readGlobalSettings(JsonReader& reader)
    {
      GlobalSettings settings;

      reader.expect('{');
      reader.expectKey("schemaVersion");
      settings.schemaVersion = reader.readInt();
      reader.expect(',');
      reader.expectKey("themeMode");
      settings.themeMode = themeModeFromPreferenceValue(reader.readString());
      reader.expect(',');
      reader.expectKey("language");
      settings.language = uiLanguageFromPreferenceValue(reader.readString());
      reader.expect(',');
      reader.expectKey("maximumThreadCount");
      settings.maximumThreadCount = static_cast<std::size_t>(reader.readUnsignedMax());
      reader.expect(',');
      reader.expectKey("maximumFileSizeBytes");
      settings.maximumFileSizeBytes = reader.readUnsignedMax();
      reader.expect(',');
      reader.expectKey("resultLimit");
      settings.resultLimit = static_cast<std::size_t>(reader.readUnsignedMax());
      reader.expect(',');
      reader.expectKey("memoryBudgetBytes");
      settings.memoryBudgetBytes = reader.readUnsignedMax();
      reader.expect(',');
      reader.expectKey("diskBudgetBytes");
      settings.diskBudgetBytes = reader.readUnsignedMax();
      reader.expect(',');
      reader.expectKey("telemetryEnabled");
      settings.telemetryEnabled = reader.readBool();
      reader.expect('}');

      return normalizedGlobalSettings(settings);
    }

    [[nodiscard]]
    SavedSearch readSavedSearch(JsonReader& reader)
    {
      SavedSearch search;

      reader.expect('{');
      reader.expectKey("name");
      search.name = reader.readString();
      reader.expect(',');
      reader.expectKey("root");
      search.root = pathFromUtf8(reader.readString());
      reader.expect(',');
      reader.expectKey("expression");
      search.expression = reader.readString();
      reader.expect(',');
      reader.expectKey("savedAtUnixMs");
      search.savedAt = fromUnixMilliseconds(reader.readUnsignedMax());
      reader.expect('}');

      return search;
    }

    [[nodiscard]]
    std::vector<SavedSearch> readSavedSearches(JsonReader& reader)
    {
      std::vector<SavedSearch> searches;

      reader.expect('[');

      if (reader.consume(']'))
        return searches;

      do {
        searches.push_back(readSavedSearch(reader));
      } while (reader.consume(','));

      reader.expect(']');

      return searches;
    }

    [[nodiscard]]
    SettingsExportPayload readSettingsExport(std::string_view text)
    {
      JsonReader reader(text);
      SettingsExportPayload payload;

      reader.expect('{');
      reader.expectKey("format");

      if (reader.readString() != exportFormatName)
        throw std::invalid_argument("Unsupported settings export format");

      reader.expect(',');
      reader.expectKey("version");

      if (reader.readInt() != exportFormatVersion)
        throw std::invalid_argument("Unsupported settings export version");

      reader.expect(',');
      reader.expectKey("global");
      payload.globalSettings = readGlobalSettings(reader);
      reader.expect(',');
      reader.expectKey("savedSearches");
      payload.savedSearches = readSavedSearches(reader);
      reader.expect('}');
      reader.expectEnd();

      return payload;
    }

    [[nodiscard]]
    std::string writeSettingsExport(const GlobalSettings& settings, const std::vector<SavedSearch>& savedSearches)
    {
      const auto normalized = normalizedGlobalSettings(settings);
      std::string output;

      output.reserve(512 + savedSearches.size() * 128);
      output += "{\n";
      output += "  ";
      appendJsonStringField(output, "format", exportFormatName);
      output += ",\n  ";
      appendJsonUnsignedField(output, "version", static_cast<std::uintmax_t>(exportFormatVersion));
      output += ",\n  \"global\": {\n";
      output += "    ";
      appendJsonUnsignedField(output, "schemaVersion", static_cast<std::uintmax_t>(normalized.schemaVersion));
      output += ",\n    ";
      appendJsonStringField(output, "themeMode", toPreferenceValue(normalized.themeMode));
      output += ",\n    ";
      appendJsonStringField(output, "language", toPreferenceValue(normalized.language));
      output += ",\n    ";
      appendJsonUnsignedField(output, "maximumThreadCount", normalized.maximumThreadCount);
      output += ",\n    ";
      appendJsonUnsignedField(output, "maximumFileSizeBytes", normalized.maximumFileSizeBytes);
      output += ",\n    ";
      appendJsonUnsignedField(output, "resultLimit", normalized.resultLimit);
      output += ",\n    ";
      appendJsonUnsignedField(output, "memoryBudgetBytes", normalized.memoryBudgetBytes);
      output += ",\n    ";
      appendJsonUnsignedField(output, "diskBudgetBytes", normalized.diskBudgetBytes);
      output += ",\n    ";
      appendJsonBoolField(output, "telemetryEnabled", normalized.telemetryEnabled);
      output += "\n  },\n  \"savedSearches\": [";

      for (std::size_t index = 0; index < savedSearches.size(); ++index) {
        const auto& search = savedSearches[index];

        if (index > 0)
          output += ',';

        output += "\n    {";
        appendJsonStringField(output, "name", search.name);
        output += ", ";
        appendJsonStringField(output, "root", pathToUtf8(search.root));
        output += ", ";
        appendJsonStringField(output, "expression", search.expression);
        output += ", ";
        appendJsonUnsignedField(output, "savedAtUnixMs", toUnixMilliseconds(search.savedAt));
        output += '}';
      }

      if (!savedSearches.empty())
        output += '\n';

      output += "  ]\n";
      output += "}\n";

      return output;
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

    normalized.maximumThreadCount = limitedSize(normalized.maximumThreadCount, maximumThreadCountLimit);
    normalized.maximumFileSizeBytes = limitedUnsignedMax(normalized.maximumFileSizeBytes, maximumFileSizeLimitBytes);
    normalized.memoryBudgetBytes = limitedUnsignedMax(normalized.memoryBudgetBytes, maximumMemoryBudgetLimitBytes);
    normalized.diskBudgetBytes = limitedUnsignedMax(normalized.diskBudgetBytes, maximumDiskBudgetLimitBytes);

    if (normalized.resultLimit == 0)
      normalized.resultLimit = defaultResultLimit;

    normalized.resultLimit = limitedSize(normalized.resultLimit, maximumResultLimit);

    return normalized;
  }

  RepositorySettings defaultRepositorySettings(RepositoryId repositoryId)
  {
    RepositorySettings settings;

    settings.repositoryId = std::move(repositoryId);
    settings.schemaVersion = currentGlobalSettingsSchemaVersion;

    return settings;
  }

  RepositorySettings normalizedRepositorySettings(const RepositorySettings& settings)
  {
    auto normalized = settings;

    normalized.schemaVersion = currentGlobalSettingsSchemaVersion;
    normalized.maximumThreadCount = limitedOptionalSize(normalized.maximumThreadCount, maximumThreadCountLimit);
    normalized.maximumFileSizeBytes =
      limitedOptionalUnsignedMax(normalized.maximumFileSizeBytes, maximumFileSizeLimitBytes);
    normalized.memoryBudgetBytes =
      limitedOptionalUnsignedMax(normalized.memoryBudgetBytes, maximumMemoryBudgetLimitBytes);
    normalized.diskBudgetBytes = limitedOptionalUnsignedMax(normalized.diskBudgetBytes, maximumDiskBudgetLimitBytes);

    if (normalized.resultLimit == 0)
      normalized.resultLimit.reset();

    normalized.resultLimit = limitedOptionalSize(normalized.resultLimit, maximumResultLimit);

    return normalized;
  }

  EffectiveRepositorySettings effectiveRepositorySettings(const GlobalSettings& globalSettings,
                                                          const RepositorySettings& repositorySettings)
  {
    const auto normalizedGlobal = normalizedGlobalSettings(globalSettings);
    const auto normalizedRepository = normalizedRepositorySettings(repositorySettings);

    return EffectiveRepositorySettings{
      .repositoryId = normalizedRepository.repositoryId,
      .schemaVersion = normalizedRepository.schemaVersion,
      .friendlyName = valueOrEmpty(normalizedRepository.friendlyName),
      .maximumThreadCount = normalizedRepository.maximumThreadCount.value_or(normalizedGlobal.maximumThreadCount),
      .maximumFileSizeBytes = normalizedRepository.maximumFileSizeBytes.value_or(normalizedGlobal.maximumFileSizeBytes),
      .resultLimit = normalizedRepository.resultLimit.value_or(normalizedGlobal.resultLimit),
      .memoryBudgetBytes = normalizedRepository.memoryBudgetBytes.value_or(normalizedGlobal.memoryBudgetBytes),
      .diskBudgetBytes = normalizedRepository.diskBudgetBytes.value_or(normalizedGlobal.diskBudgetBytes),
      .respectGitignore = normalizedRepository.respectGitignore.value_or(defaultRespectGitignore),
      .includeHiddenFiles = normalizedRepository.includeHiddenFiles.value_or(defaultIncludeHiddenFiles),
      .relevantExtensions = valueOrEmpty(normalizedRepository.relevantExtensions),
      .telemetryEnabled = normalizedGlobal.telemetryEnabled};
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

  RepositorySettings StorageSettingsService::loadRepositorySettings(const RepositoryId& repositoryId) const
  {
    auto settings = defaultRepositorySettings(repositoryId);

    if (const auto value = repositoryPreference(*storageService, repositoryId, repositorySchemaVersionKey))
      settings.schemaVersion = parseInt(*value).value_or(currentGlobalSettingsSchemaVersion);

    settings.friendlyName = repositoryPreference(*storageService, repositoryId, repositoryFriendlyNameKey);
    settings.relevantExtensions = repositoryPreference(*storageService, repositoryId, repositoryRelevantExtensionsKey);

    if (const auto value = repositoryPreference(*storageService, repositoryId, repositoryMaximumThreadCountKey))
      settings.maximumThreadCount = parseSize(*value);

    if (const auto value = repositoryPreference(*storageService, repositoryId, repositoryMaximumFileSizeBytesKey))
      settings.maximumFileSizeBytes = parseUnsignedMax(*value);

    if (const auto value = repositoryPreference(*storageService, repositoryId, repositoryResultLimitKey))
      settings.resultLimit = parseSize(*value);

    if (const auto value = repositoryPreference(*storageService, repositoryId, repositoryMemoryBudgetBytesKey))
      settings.memoryBudgetBytes = parseUnsignedMax(*value);

    if (const auto value = repositoryPreference(*storageService, repositoryId, repositoryDiskBudgetBytesKey))
      settings.diskBudgetBytes = parseUnsignedMax(*value);

    if (const auto value = repositoryPreference(*storageService, repositoryId, repositoryRespectGitignoreKey))
      settings.respectGitignore = parseOptionalBool(*value);

    if (const auto value = repositoryPreference(*storageService, repositoryId, repositoryIncludeHiddenFilesKey))
      settings.includeHiddenFiles = parseOptionalBool(*value);

    return normalizedRepositorySettings(settings);
  }

  EffectiveRepositorySettings StorageSettingsService::resolveRepositorySettings(const RepositoryId& repositoryId) const
  {
    return effectiveRepositorySettings(loadGlobalSettings(), loadRepositorySettings(repositoryId));
  }

  void StorageSettingsService::saveRepositorySettings(const RepositorySettings& settings)
  {
    const auto normalized = normalizedRepositorySettings(settings);

    setRepositoryPreference(
      *storageService, normalized.repositoryId, repositorySchemaVersionKey, std::to_string(normalized.schemaVersion));

    if (normalized.friendlyName)
      setRepositoryPreference(
        *storageService, normalized.repositoryId, repositoryFriendlyNameKey, *normalized.friendlyName);

    if (normalized.maximumThreadCount)
      setRepositoryPreference(*storageService,
                              normalized.repositoryId,
                              repositoryMaximumThreadCountKey,
                              std::to_string(*normalized.maximumThreadCount));

    if (normalized.maximumFileSizeBytes)
      setRepositoryPreference(*storageService,
                              normalized.repositoryId,
                              repositoryMaximumFileSizeBytesKey,
                              std::to_string(*normalized.maximumFileSizeBytes));

    if (normalized.resultLimit)
      setRepositoryPreference(
        *storageService, normalized.repositoryId, repositoryResultLimitKey, std::to_string(*normalized.resultLimit));

    if (normalized.memoryBudgetBytes)
      setRepositoryPreference(*storageService,
                              normalized.repositoryId,
                              repositoryMemoryBudgetBytesKey,
                              std::to_string(*normalized.memoryBudgetBytes));

    if (normalized.diskBudgetBytes)
      setRepositoryPreference(*storageService,
                              normalized.repositoryId,
                              repositoryDiskBudgetBytesKey,
                              std::to_string(*normalized.diskBudgetBytes));

    if (normalized.respectGitignore)
      setRepositoryPreference(*storageService,
                              normalized.repositoryId,
                              repositoryRespectGitignoreKey,
                              *normalized.respectGitignore ? "true" : "false");

    if (normalized.includeHiddenFiles)
      setRepositoryPreference(*storageService,
                              normalized.repositoryId,
                              repositoryIncludeHiddenFilesKey,
                              *normalized.includeHiddenFiles ? "true" : "false");

    if (normalized.relevantExtensions)
      setRepositoryPreference(
        *storageService, normalized.repositoryId, repositoryRelevantExtensionsKey, *normalized.relevantExtensions);
  }

  std::string StorageSettingsService::exportSettingsAndSavedSearches() const
  {
    return writeSettingsExport(loadGlobalSettings(), storageService->savedSearches());
  }

  void StorageSettingsService::importSettingsAndSavedSearches(std::string_view text)
  {
    const auto payload = readSettingsExport(text);

    saveGlobalSettings(payload.globalSettings);

    for (const auto& savedSearch : payload.savedSearches)
      storageService->saveSearch(savedSearch);
  }

} // namespace uburu::app
