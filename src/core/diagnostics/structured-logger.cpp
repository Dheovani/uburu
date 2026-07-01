#include "core/diagnostics/structured-logger.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

namespace uburu::diagnostics
{
  namespace
  {

    constexpr std::string_view redactedFieldValue = "<redacted>";

    [[nodiscard]] int logLevelPriority(LogLevel level)
    {
      switch (level) {
      case LogLevel::trace:
        return 0;
      case LogLevel::debug:
        return 1;
      case LogLevel::info:
        return 2;
      case LogLevel::warning:
        return 3;
      case LogLevel::error:
        return 4;
      }

      return 0;
    }

    [[nodiscard]] bool categoryIsEnabled(LogCategory category, const StructuredLogOptions& options)
    {
      if (options.enabledCategories.empty())
        return true;

      return std::ranges::find(options.enabledCategories, category) != options.enabledCategories.end();
    }

  } // namespace

  bool shouldRecordLogEvent(const LogEvent& event, const StructuredLogOptions& options)
  {
    return logLevelPriority(event.level) >= logLevelPriority(options.minimumLevel) &&
           categoryIsEnabled(event.category, options);
  }

  void sanitizeLogEvent(LogEvent& event, const StructuredLogOptions& options)
  {
    if (event.timestamp == std::chrono::system_clock::time_point{})
      event.timestamp = std::chrono::system_clock::now();

    if (options.includeSensitiveFields)
      return;

    for (auto& field : event.fields) {
      if (field.sensitive)
        field.value = redactedFieldValue;
    }
  }

  InMemoryStructuredLogger::InMemoryStructuredLogger(StructuredLogOptions options) : options(std::move(options)) {}

  void InMemoryStructuredLogger::write(LogEvent event)
  {
    if (!shouldRecordLogEvent(event, options))
      return;

    sanitizeLogEvent(event, options);
    recordedEntries.push_back(std::move(event));
  }

  const std::vector<LogEvent>& InMemoryStructuredLogger::entries() const noexcept
  {
    return recordedEntries;
  }

  void InMemoryStructuredLogger::clear()
  {
    recordedEntries.clear();
  }

  std::string_view logLevelName(LogLevel level)
  {
    switch (level) {
    case LogLevel::trace:
      return "[TRACE]";
    case LogLevel::debug:
      return "[DEBUG]";
    case LogLevel::info:
      return "[INFO]";
    case LogLevel::warning:
      return "[WARNING]";
    case LogLevel::error:
      return "[ERROR]";
    }

    return "unknown";
  }

  std::string_view logCategoryName(LogCategory category)
  {
    switch (category) {
    case LogCategory::search:
      return "<search>";
    case LogCategory::indexing:
      return "<indexing>";
    case LogCategory::storage:
      return "<storage>";
    case LogCategory::git:
      return "<git>";
    case LogCategory::filesystem:
      return "<filesystem>";
    case LogCategory::diagnostics:
      return "<diagnostics>";
    }

    return "unknown";
  }

} // namespace uburu::diagnostics
