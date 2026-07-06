#include "benchmark-dataset.hpp"
#include "benchmark-output.hpp"
#include "index-benchmark-runner.hpp"
#include "search-benchmark-runner.hpp"

#include <benchmark/benchmark.h>

namespace
{

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
BENCHMARK(BM_IndexService_Initial_ManySmallFiles);
BENCHMARK(BM_IndexService_Incremental_ManySmallFiles);
BENCHMARK(BM_IndexService_BranchSwitch_ManySmallFiles);
