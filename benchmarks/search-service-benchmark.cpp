#include "benchmark-dataset.hpp"
#include "benchmark-output.hpp"
#include "index-benchmark-runner.hpp"
#include "search-benchmark-runner.hpp"

#include <benchmark/benchmark.h>

namespace
{

  constexpr std::size_t smallRenderBatchSize = 16;
  constexpr std::size_t largeRenderBatchSize = 256;
  constexpr std::size_t adaptiveMinimumBatchSize = 16;
  constexpr std::size_t adaptiveMaximumBatchSize = 512;
  constexpr std::uint32_t simulatedUiRenderPasses = 8;

  [[nodiscard]] uburu::benchmarks::SearchBenchmarkRunOptions fixedBatchRunOptions(std::size_t batchSize)
  {
    return uburu::benchmarks::SearchBenchmarkRunOptions{
      .executionOptions = uburu::app::SearchExecutionOptions{.resultBatchSize = batchSize, .adaptiveBatching = false},
      .simulatedUiRenderPasses = simulatedUiRenderPasses,
    };
  }

  [[nodiscard]] uburu::benchmarks::SearchBenchmarkRunOptions adaptiveBatchRunOptions()
  {
    return uburu::benchmarks::SearchBenchmarkRunOptions{
      .executionOptions = uburu::app::SearchExecutionOptions{.resultBatchSize = smallRenderBatchSize,
                                                             .adaptiveBatching = true,
                                                             .minimumResultBatchSize = adaptiveMinimumBatchSize,
                                                             .maximumResultBatchSize = adaptiveMaximumBatchSize},
      .simulatedUiRenderPasses = simulatedUiRenderPasses,
    };
  }

  template <typename DatasetFactory> void runSearchServiceScenario(benchmark::State& state, DatasetFactory factory)
  {
    const auto dataset = factory();

    for (auto _ : state) {
      const auto result = uburu::benchmarks::runDefaultSearchServiceBenchmark(dataset.get());
      auto consumedResults = result.consumedResults;
      auto consumedBytes = result.consumedBytes;

      benchmark::DoNotOptimize(consumedResults);
      benchmark::DoNotOptimize(consumedBytes);
      uburu::benchmarks::publishSearchCounters(state, dataset.get(), result);
    }
  }

  template <typename DatasetFactory>
  void runSearchServiceScenario(benchmark::State& state,
                                DatasetFactory factory,
                                uburu::benchmarks::SearchBenchmarkRunOptions options)
  {
    const auto dataset = factory();

    for (auto _ : state) {
      const auto result = uburu::benchmarks::runDefaultSearchServiceBenchmark(dataset.get(), options);
      auto consumedResults = result.consumedResults;
      auto consumedBytes = result.consumedBytes + result.simulatedUiRenderBytes;
      auto renderChecksum = result.simulatedUiRenderChecksum;

      benchmark::DoNotOptimize(consumedResults);
      benchmark::DoNotOptimize(consumedBytes);
      benchmark::DoNotOptimize(renderChecksum);
      uburu::benchmarks::publishSearchCounters(state, dataset.get(), result);
    }
  }

  template <typename DatasetFactory>
  void runRepeatedSearchServiceScenario(benchmark::State& state, DatasetFactory factory)
  {
    const auto dataset = factory();

    for (auto _ : state) {
      const auto firstPass = uburu::benchmarks::runDefaultSearchServiceBenchmark(dataset.get());
      const auto secondPass = uburu::benchmarks::runDefaultSearchServiceBenchmark(dataset.get());
      auto consumedResults = firstPass.consumedResults + secondPass.consumedResults;
      auto consumedBytes = firstPass.consumedBytes + secondPass.consumedBytes;

      benchmark::DoNotOptimize(consumedResults);
      benchmark::DoNotOptimize(consumedBytes);
      uburu::benchmarks::publishRepeatedSearchCounters(state, dataset.get(), firstPass, secondPass);
    }
  }

  void BM_SearchService_Direct_ManySmallFiles_Literal(benchmark::State& state)
  {
    runSearchServiceScenario(state, uburu::benchmarks::makeManySmallFilesDataset);
  }

  void BM_SearchService_Direct_FewLargeFiles_Literal(benchmark::State& state)
  {
    runSearchServiceScenario(state, uburu::benchmarks::makeFewLargeFilesDataset);
  }

  void BM_SearchService_Direct_ManySmallFiles_Regex(benchmark::State& state)
  {
    runSearchServiceScenario(state, uburu::benchmarks::makeRegexHeavyContentDataset);
  }

  void BM_SearchService_Direct_LiteralCaseInsensitive(benchmark::State& state)
  {
    runSearchServiceScenario(state, uburu::benchmarks::makeCaseInsensitiveLiteralDataset);
  }

  void BM_SearchService_Direct_LiteralCaseSensitive(benchmark::State& state)
  {
    runSearchServiceScenario(state, uburu::benchmarks::makeCaseSensitiveLiteralDataset);
  }

  void BM_SearchService_Direct_LiteralWholeWord(benchmark::State& state)
  {
    runSearchServiceScenario(state, uburu::benchmarks::makeWholeWordLiteralDataset);
  }

  void BM_SearchService_Direct_UnicodeNormalization(benchmark::State& state)
  {
    runSearchServiceScenario(state, uburu::benchmarks::makeUnicodeNormalizationDataset);
  }

  void BM_SearchService_Direct_GitignoreHeavy_Literal(benchmark::State& state)
  {
    runSearchServiceScenario(state, uburu::benchmarks::makeGitignoreHeavyDataset);
  }

  void BM_SearchService_Direct_BinaryAndHiddenFiltering(benchmark::State& state)
  {
    runSearchServiceScenario(state, uburu::benchmarks::makeMixedTextAndBinaryDataset);
  }

  void BM_SearchService_Direct_RepeatedManySmallFiles_CacheEffect(benchmark::State& state)
  {
    runRepeatedSearchServiceScenario(state, uburu::benchmarks::makeManySmallFilesDataset);
  }

  void BM_SearchService_Batching_FixedSmallBatch_RenderCost(benchmark::State& state)
  {
    auto datasetFactory = uburu::benchmarks::makeFewLargeFilesDataset;

    runSearchServiceScenario(state, datasetFactory, fixedBatchRunOptions(smallRenderBatchSize));
  }

  void BM_SearchService_Batching_FixedLargeBatch_RenderCost(benchmark::State& state)
  {
    auto datasetFactory = uburu::benchmarks::makeFewLargeFilesDataset;

    runSearchServiceScenario(state, datasetFactory, fixedBatchRunOptions(largeRenderBatchSize));
  }

  void BM_SearchService_Batching_Adaptive_RenderCost(benchmark::State& state)
  {
    auto datasetFactory = uburu::benchmarks::makeFewLargeFilesDataset;

    runSearchServiceScenario(state, datasetFactory, adaptiveBatchRunOptions());
  }

  void BM_IndexService_Initial_ManySmallFiles(benchmark::State& state)
  {
    const auto dataset = uburu::benchmarks::makeManySmallFilesDataset();

    for (auto _ : state) {
      auto context = uburu::benchmarks::makeIndexBenchmarkContext(dataset.get());
      const auto result = context->update(context->worktree());

      uburu::benchmarks::publishIndexCounters(state, dataset.get(), result);
    }
  }

  void BM_IndexService_Incremental_ManySmallFiles(benchmark::State& state)
  {
    const auto dataset = uburu::benchmarks::makeManySmallFilesDataset();

    for (auto _ : state) {
      state.PauseTiming();
      auto context = uburu::benchmarks::makeIndexBenchmarkContext(dataset.get());
      static_cast<void>(context->update(context->worktree()));
      state.ResumeTiming();

      const auto result = context->update(context->worktree());

      uburu::benchmarks::publishIndexCounters(state, dataset.get(), result);
    }
  }

  void BM_IndexService_BranchSwitch_ManySmallFiles(benchmark::State& state)
  {
    const auto dataset = uburu::benchmarks::makeManySmallFilesDataset();

    for (auto _ : state) {
      state.PauseTiming();
      auto context = uburu::benchmarks::makeIndexBenchmarkContext(dataset.get());
      static_cast<void>(context->update(context->worktree()));
      state.ResumeTiming();

      const auto result = context->updateAfterBranchSwitch();

      uburu::benchmarks::publishIndexCounters(state, dataset.get(), result);
    }
  }

  void BM_IndexService_ContentHashReuse_ManySmallFiles(benchmark::State& state)
  {
    const auto dataset = uburu::benchmarks::makeManySmallFilesDataset();

    for (auto _ : state) {
      auto context = uburu::benchmarks::makeIndexBenchmarkContext(dataset.get());
      const auto result = context->update(context->worktree());

      uburu::benchmarks::publishIndexCounters(state, dataset.get(), result);
    }
  }

  void BM_IndexService_BlobHashReuse_RenamedFile(benchmark::State& state)
  {
    const auto dataset = uburu::benchmarks::makeManySmallFilesDataset();

    for (auto _ : state) {
      auto context = uburu::benchmarks::makeIndexBenchmarkContext(dataset.get());
      const auto result = context->updateWithBlobReuseCandidate();

      uburu::benchmarks::publishIndexCounters(state, dataset.get(), result);
    }
  }

} // namespace

BENCHMARK(BM_SearchService_Direct_ManySmallFiles_Literal);
BENCHMARK(BM_SearchService_Direct_FewLargeFiles_Literal);
BENCHMARK(BM_SearchService_Direct_ManySmallFiles_Regex);
BENCHMARK(BM_SearchService_Direct_LiteralCaseInsensitive);
BENCHMARK(BM_SearchService_Direct_LiteralCaseSensitive);
BENCHMARK(BM_SearchService_Direct_LiteralWholeWord);
BENCHMARK(BM_SearchService_Direct_UnicodeNormalization);
BENCHMARK(BM_SearchService_Direct_GitignoreHeavy_Literal);
BENCHMARK(BM_SearchService_Direct_BinaryAndHiddenFiltering);
BENCHMARK(BM_SearchService_Direct_RepeatedManySmallFiles_CacheEffect);
BENCHMARK(BM_SearchService_Batching_FixedSmallBatch_RenderCost);
BENCHMARK(BM_SearchService_Batching_FixedLargeBatch_RenderCost);
BENCHMARK(BM_SearchService_Batching_Adaptive_RenderCost);
BENCHMARK(BM_IndexService_Initial_ManySmallFiles);
BENCHMARK(BM_IndexService_Incremental_ManySmallFiles);
BENCHMARK(BM_IndexService_BranchSwitch_ManySmallFiles);
BENCHMARK(BM_IndexService_ContentHashReuse_ManySmallFiles);
BENCHMARK(BM_IndexService_BlobHashReuse_RenamedFile);
