#pragma once

#include "core/diagnostics/structured-logger.hpp"

#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace uburu::diagnostics
{

  /**
   * Controls whether short-lived search trace events are recorded.
   */
  struct SearchTracingOptions
  {
    bool enabled{false};
    bool includeSensitiveFields{false};
    std::size_t maximumEvents{1024};
  };

  /**
   * One measured search operation with optional structured fields.
   */
  struct SearchTraceEvent
  {
    std::string name;
    LogCategory category{LogCategory::search};
    std::chrono::steady_clock::time_point startedAt{};
    std::chrono::nanoseconds elapsed{};
    std::vector<LogField> fields;
  };

  /**
   * Collects bounded trace events for diagnostics and tests.
   */
  class SearchTraceRecorder final
  {
  public:
    explicit SearchTraceRecorder(SearchTracingOptions options = {});

    [[nodiscard]]
    bool enabled() const noexcept;

    void record(
      std::string name,
      LogCategory category,
      std::chrono::nanoseconds elapsed,
      std::vector<LogField> fields = {});

    [[nodiscard]]
    const std::vector<SearchTraceEvent>& events() const noexcept;

    void clear();

  private:
    SearchTracingOptions options;
    std::vector<SearchTraceEvent> recordedEvents;
  };

  /**
   * RAII helper that records elapsed time when leaving scope.
   */
  class SearchTraceScope final
  {
  public:
    SearchTraceScope(
      SearchTraceRecorder& recorder,
      std::string name,
      LogCategory category,
      std::vector<LogField> fields = {});
    SearchTraceScope(const SearchTraceScope&) = delete;
    SearchTraceScope& operator=(const SearchTraceScope&) = delete;
    SearchTraceScope(SearchTraceScope&& other) noexcept;
    SearchTraceScope& operator=(SearchTraceScope&& other) noexcept;
    ~SearchTraceScope();

  private:
    SearchTraceRecorder* recorder{nullptr};
    std::string name;
    LogCategory category{LogCategory::search};
    std::vector<LogField> fields;
    std::chrono::steady_clock::time_point startedAt{};
  };

  [[nodiscard]]
  SearchTraceScope traceSearchScope(
    SearchTraceRecorder& recorder,
    std::string name,
    LogCategory category = LogCategory::search,
    std::vector<LogField> fields = {});

} // namespace uburu::diagnostics
