#pragma once

#include "core/diagnostics/metrics.hpp"
#include "core/diagnostics/search-tracing.hpp"
#include "core/diagnostics/structured-logger.hpp"

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace uburu::diagnostics
{

  /**
   * Portable diagnostic bundle intended for support and local troubleshooting.
   */
  struct DiagnosticReport
  {
    std::string productName{"Uburu"};
    std::chrono::system_clock::time_point generatedAt{};
    std::vector<LogEvent> logs;
    std::vector<SearchMetrics> searchMetrics;
    std::vector<SearchTraceEvent> traceEvents;
  };

  struct DiagnosticReportExportOptions
  {
    bool includeSensitiveFields{false};
  };

  /**
   * Crash-focused diagnostic bundle safe to write after a controlled failure path.
   */
  struct CrashReport
  {
    std::string productName{"Uburu"};
    std::string applicationVersion{"development"};
    std::string platform;
    std::string buildConfiguration;
    std::chrono::system_clock::time_point generatedAt{};
    std::string reason;
    std::string errorCategory;
    std::vector<LogField> fields;
    std::vector<LogEvent> recentErrors;
  };

  struct CrashReportExportOptions
  {
    bool includeSensitiveFields{false};
  };

  [[nodiscard]]
  std::string diagnosticReportJson(DiagnosticReport report, DiagnosticReportExportOptions options = {});

  void exportDiagnosticReport(
    const DiagnosticReport& report,
    const std::filesystem::path& path,
    DiagnosticReportExportOptions options = {});

  [[nodiscard]]
  CrashReport makeCrashReport(std::string reason, std::string errorCategory = {});

  [[nodiscard]]
  std::string crashReportJson(CrashReport report, CrashReportExportOptions options = {});

  void exportCrashReport(
    const CrashReport& report,
    const std::filesystem::path& path,
    CrashReportExportOptions options = {});

} // namespace uburu::diagnostics
