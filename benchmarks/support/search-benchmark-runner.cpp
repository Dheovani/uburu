#include "search-benchmark-runner.hpp"

#include "app/services/search-service.hpp"
#include "core/filesystem/recursive-file-scanner.hpp"
#include "core/search/direct-search-engine.hpp"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

namespace uburu::benchmarks
{
  namespace
  {

    [[nodiscard]] std::uint64_t renderTextChecksum(std::string_view text)
    {
      std::uint64_t checksum = 0;

      for (const auto character : text)
        checksum += static_cast<unsigned char>(character);

      return checksum;
    }

    void simulateUiRender(const app::SearchResultDto& result,
                          SearchBenchmarkRunOptions options,
                          SearchBenchmarkResult& output)
    {
      for (std::uint32_t pass = 0; pass < options.simulatedUiRenderPasses; ++pass) {
        output.simulatedUiRenderBytes += static_cast<std::uint64_t>(result.lineText.size());
        output.simulatedUiRenderBytes += static_cast<std::uint64_t>(result.path.native().size());
        output.simulatedUiRenderChecksum += renderTextChecksum(result.lineText);
        output.simulatedUiRenderChecksum += renderTextChecksum(result.path.generic_string());
      }
    }

  } // namespace

  SearchBenchmarkResult runDefaultSearchServiceBenchmark(const BenchmarkDataset& dataset)
  {
    return runDefaultSearchServiceBenchmark(dataset, SearchBenchmarkRunOptions{});
  }

  SearchBenchmarkResult runDefaultSearchServiceBenchmark(const BenchmarkDataset& dataset,
                                                         SearchBenchmarkRunOptions options)
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
            simulateUiRender(emittedResult, options, result);
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
      options.executionOptions);

    if (result.summary.metrics.totalTime.count() == 0)
      result.summary = app::toSearchSummaryDto(summary);

    return result;
  }

} // namespace uburu::benchmarks
