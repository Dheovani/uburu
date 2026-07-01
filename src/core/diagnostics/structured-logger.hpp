#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

namespace uburu::diagnostics
{

  enum class LogLevel
  {
    trace,
    debug,
    info,
    warning,
    error
  };

  enum class LogCategory
  {
    search,
    indexing,
    storage,
    git,
    filesystem,
    diagnostics
  };

  struct LogField
  {
    std::string key;
    std::string value;
    bool sensitive{false};
  };

  struct LogEvent
  {
    LogLevel level{LogLevel::info};
    LogCategory category{LogCategory::diagnostics};
    std::string message;
    std::vector<LogField> fields;
    std::chrono::system_clock::time_point timestamp{};
  };

  struct StructuredLogOptions
  {
    bool includeSensitiveFields{false};
  };

  class StructuredLogger
  {
  public:
    virtual ~StructuredLogger() = default;
    virtual void write(LogEvent event) = 0;
  };

  class InMemoryStructuredLogger final : public StructuredLogger
  {
  public:
    explicit InMemoryStructuredLogger(StructuredLogOptions options = {});

    void write(LogEvent event) override;
    [[nodiscard]] const std::vector<LogEvent>& entries() const noexcept;
    void clear();

  private:
    StructuredLogOptions options;
    std::vector<LogEvent> recordedEntries;
  };

  [[nodiscard]] std::string_view logLevelName(LogLevel level);
  [[nodiscard]] std::string_view logCategoryName(LogCategory category);

} // namespace uburu::diagnostics
