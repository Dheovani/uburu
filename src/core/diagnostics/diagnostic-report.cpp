#include "core/diagnostics/diagnostic-report.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace uburu::diagnostics
{
  namespace
  {

    constexpr std::string_view redactedDiagnosticFieldValue = "<redacted>";

    [[nodiscard]] std::string defaultPlatformName()
    {
#if defined(_WIN32)
      return "windows";
#elif defined(__APPLE__)
      return "macos";
#elif defined(__linux__)
      return "linux";
#else
      return "unknown";
#endif
    }

    [[nodiscard]] std::string defaultBuildConfiguration()
    {
#if defined(NDEBUG)
      return "release";
#else
      return "debug";
#endif
    }

    [[nodiscard]] std::string escapedJsonString(std::string_view value)
    {
      std::string escaped;
      escaped.reserve(value.size());

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
          escaped += character;
          break;
        }
      }

      return escaped;
    }

    [[nodiscard]] std::int64_t timestampMilliseconds(std::chrono::system_clock::time_point timestamp)
    {
      return std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();
    }

    [[nodiscard]] std::int64_t elapsedNanoseconds(std::chrono::nanoseconds elapsed)
    {
      return elapsed.count();
    }

    [[nodiscard]] LogField sanitizedField(LogField field, bool includeSensitiveFields)
    {
      if (field.sensitive && !includeSensitiveFields)
        field.value = redactedDiagnosticFieldValue;

      return field;
    }

    void appendJsonFieldMap(std::string& json, const std::vector<LogField>& fields, bool includeSensitiveFields)
    {
      json += "{";

      for (std::size_t index = 0; index < fields.size(); ++index) {
        if (index > 0)
          json += ",";

        const auto field = sanitizedField(fields[index], includeSensitiveFields);

        json += "\"";
        json += escapedJsonString(field.key);
        json += "\":\"";
        json += escapedJsonString(field.value);
        json += "\"";
      }

      json += "}";
    }

    void appendLogEvent(std::string& json, LogEvent event, bool includeSensitiveFields)
    {
      if (event.timestamp == std::chrono::system_clock::time_point{})
        event.timestamp = std::chrono::system_clock::now();

      json += "{\"timestamp_ms\":";
      json += std::to_string(timestampMilliseconds(event.timestamp));
      json += ",\"level\":\"";
      json += logLevelName(event.level);
      json += "\",\"category\":\"";
      json += logCategoryName(event.category);
      json += "\",\"message\":\"";
      json += escapedJsonString(event.message);
      json += "\",\"fields\":";
      appendJsonFieldMap(json, event.fields, includeSensitiveFields);
      json += "}";
    }

    void appendSearchMetrics(std::string& json, const SearchMetrics& metrics)
    {
      json += "{\"time_to_first_result_ns\":";
      json += std::to_string(elapsedNanoseconds(metrics.timeToFirstResult));
      json += ",\"total_time_ns\":";
      json += std::to_string(elapsedNanoseconds(metrics.totalTime));
      json += ",\"files_processed\":";
      json += std::to_string(metrics.filesProcessed);
      json += ",\"bytes_processed\":";
      json += std::to_string(metrics.bytesProcessed);
      json += ",\"results_emitted\":";
      json += std::to_string(metrics.resultsEmitted);
      json += ",\"cache_hits\":";
      json += std::to_string(metrics.cacheHits);
      json += ",\"cache_misses\":";
      json += std::to_string(metrics.cacheMisses);
      json += ",\"reused_by_hash\":";
      json += std::to_string(metrics.reusedByHash);
      json += ",\"approximate_memory_bytes\":";
      json += std::to_string(metrics.approximateMemoryBytes);
      json += ",\"memory_growth_bytes\":";
      json += std::to_string(metrics.memoryGrowthBytes);
      json += ",\"memory_increased\":";
      json += metrics.memoryIncreased ? "true" : "false";
      json += "}";
    }

    void appendTraceEvent(std::string& json, const SearchTraceEvent& event, bool includeSensitiveFields)
    {
      json += "{\"name\":\"";
      json += escapedJsonString(event.name);
      json += "\",\"category\":\"";
      json += logCategoryName(event.category);
      json += "\",\"elapsed_ns\":";
      json += std::to_string(elapsedNanoseconds(event.elapsed));
      json += ",\"fields\":";
      appendJsonFieldMap(json, event.fields, includeSensitiveFields);
      json += "}";
    }

    template <typename T, typename Append>
    void appendArray(std::string& json, const std::vector<T>& values, Append append)
    {
      json += "[";

      for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0)
          json += ",";

        append(json, values[index]);
      }

      json += "]";
    }

    void exportJsonToFile(std::string_view json, const std::filesystem::path& path, std::string_view artifactName)
    {
      if (path.empty())
        throw std::invalid_argument(std::string(artifactName) + " export path is required");

      const auto parentPath = path.parent_path();

      if (!parentPath.empty())
        std::filesystem::create_directories(parentPath);

      std::ofstream stream(path, std::ios::binary | std::ios::trunc);
      if (!stream)
        throw std::runtime_error("Could not open " + std::string(artifactName) + " file");

      stream << json;
    }

  } // namespace

  std::string diagnosticReportJson(DiagnosticReport report, DiagnosticReportExportOptions options)
  {
    if (report.generatedAt == std::chrono::system_clock::time_point{})
      report.generatedAt = std::chrono::system_clock::now();

    std::string json;
    json += "{\"product\":\"";
    json += escapedJsonString(report.productName);
    json += "\",\"generated_at_ms\":";
    json += std::to_string(timestampMilliseconds(report.generatedAt));
    json += ",\"logs\":";
    appendArray(json, report.logs, [&](std::string& output, const LogEvent& event) {
      appendLogEvent(output, event, options.includeSensitiveFields);
    });
    json += ",\"search_metrics\":";
    appendArray(json, report.searchMetrics, appendSearchMetrics);
    json += ",\"trace_events\":";
    appendArray(json, report.traceEvents, [&](std::string& output, const SearchTraceEvent& event) {
      appendTraceEvent(output, event, options.includeSensitiveFields);
    });
    json += "}\n";

    return json;
  }

  void exportDiagnosticReport(const DiagnosticReport& report,
                              const std::filesystem::path& path,
                              DiagnosticReportExportOptions options)
  {
    exportJsonToFile(diagnosticReportJson(report, options), path, "diagnostic report");
  }

  CrashReport makeCrashReport(std::string reason, std::string errorCategory)
  {
    CrashReport report;
    report.platform = defaultPlatformName();
    report.buildConfiguration = defaultBuildConfiguration();
    report.reason = std::move(reason);
    report.errorCategory = std::move(errorCategory);

    return report;
  }

  std::string crashReportJson(CrashReport report, CrashReportExportOptions options)
  {
    if (report.generatedAt == std::chrono::system_clock::time_point{})
      report.generatedAt = std::chrono::system_clock::now();

    if (report.platform.empty())
      report.platform = defaultPlatformName();

    if (report.buildConfiguration.empty())
      report.buildConfiguration = defaultBuildConfiguration();

    std::string json;
    json += "{\"product\":\"";
    json += escapedJsonString(report.productName);
    json += "\",\"application_version\":\"";
    json += escapedJsonString(report.applicationVersion);
    json += "\",\"platform\":\"";
    json += escapedJsonString(report.platform);
    json += "\",\"build_configuration\":\"";
    json += escapedJsonString(report.buildConfiguration);
    json += "\",\"generated_at_ms\":";
    json += std::to_string(timestampMilliseconds(report.generatedAt));
    json += ",\"reason\":\"";
    json += escapedJsonString(report.reason);
    json += "\",\"error_category\":\"";
    json += escapedJsonString(report.errorCategory);
    json += "\",\"fields\":";
    appendJsonFieldMap(json, report.fields, options.includeSensitiveFields);
    json += ",\"recent_errors\":";
    appendArray(json, report.recentErrors, [&](std::string& output, const LogEvent& event) {
      appendLogEvent(output, event, options.includeSensitiveFields);
    });
    json += "}\n";

    return json;
  }

  void exportCrashReport(const CrashReport& report, const std::filesystem::path& path, CrashReportExportOptions options)
  {
    exportJsonToFile(crashReportJson(report, options), path, "crash report");
  }

} // namespace uburu::diagnostics
