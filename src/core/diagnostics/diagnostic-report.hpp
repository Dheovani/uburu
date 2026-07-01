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

  [[nodiscard]] std::string diagnosticReportJson(DiagnosticReport report, DiagnosticReportExportOptions options = {});
  void exportDiagnosticReport(const DiagnosticReport& report,
                              const std::filesystem::path& path,
                              DiagnosticReportExportOptions options = {});

} // namespace uburu::diagnostics
