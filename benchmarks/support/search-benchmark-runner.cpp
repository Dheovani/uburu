#include "search-benchmark-runner.hpp"

#include "app/services/search-service.hpp"
#include "core/filesystem/recursive-file-scanner.hpp"
#include "core/search/direct-search-engine.hpp"

#include <algorithm>
#include <memory>
#include <utility>

namespace uburu::benchmarks
{

  SearchBenchmarkResult runDefaultSearchServiceBenchmark(const BenchmarkDataset& dataset)
  {
    auto engine = std::make_shared<search::DirectSearchEngine>(std::make_shared<filesystem::RecursiveFileScanner>());
    app::DefaultSearchService service(std::move(engine));
    SearchBenchmarkResult result;
    const auto query = makeSearchQuery(dataset);

    const auto summary = service.searchWithEvents(
      query,
      [&](const app::SearchEventDto& event) {
        if (event.kind == app::SearchEventKind::resultBatch) {
          ++result.resultBatches;
          result.consumedResults += static_cast<std::uint64_t>(event.results.size());

          std::uint64_t batchBytes = 0;

          for (const auto& emittedResult : event.results) {
            batchBytes += static_cast<std::uint64_t>(emittedResult.lineText.size());
            batchBytes += static_cast<std::uint64_t>(emittedResult.path.native().size());
          }

          result.consumedBytes += batchBytes;
          const auto currentBatchResults = static_cast<std::uint64_t>(event.results.size());
          result.peakBatchResults = std::max(result.peakBatchResults, currentBatchResults);
          result.peakBatchBytes = std::max(result.peakBatchBytes, batchBytes);
        }

        if (event.kind == app::SearchEventKind::completed || event.kind == app::SearchEventKind::failed ||
            event.kind == app::SearchEventKind::cancelled)
          result.summary = event.summary;

        return true;
      },
      app::SearchExecutionOptions{.adaptiveBatching = false});

    if (result.summary.metrics.totalTime.count() == 0)
      result.summary = app::toSearchSummaryDto(summary);

    return result;
  }

} // namespace uburu::benchmarks
