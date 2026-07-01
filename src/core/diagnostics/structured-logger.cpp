#include "core/diagnostics/structured-logger.hpp"

#include <string_view>
#include <utility>

namespace uburu::diagnostics
{
  namespace
  {

    constexpr std::string_view redactedFieldValue = "<redacted>";

    void sanitizeSensitiveFields(LogEvent& event, const StructuredLogOptions& options)
    {
      if (options.includeSensitiveFields)
        return;

      for (auto& field : event.fields) {
        if (field.sensitive)
          field.value = redactedFieldValue;
      }
    }

  } // namespace

  InMemoryStructuredLogger::InMemoryStructuredLogger(StructuredLogOptions options) : options(options) {}

  void InMemoryStructuredLogger::write(LogEvent event)
  {
    if (event.timestamp == std::chrono::system_clock::time_point{})
      event.timestamp = std::chrono::system_clock::now();

    sanitizeSensitiveFields(event, options);
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
