#pragma once

#include "benchmark-dataset.hpp"
#include "search-benchmark-runner.hpp"

#include <benchmark/benchmark.h>

namespace uburu::benchmarks
{

  void
  publishSearchCounters(benchmark::State& state, const BenchmarkDataset& dataset, const SearchBenchmarkResult& result);

  void publishRepeatedSearchCounters(benchmark::State& state,
                                     const BenchmarkDataset& dataset,
                                     const SearchBenchmarkResult& firstPass,
                                     const SearchBenchmarkResult& secondPass);

} // namespace uburu::benchmarks
