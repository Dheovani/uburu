#pragma once

#include "shared/types/domain-types.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace uburu::benchmarks
{

  struct BenchmarkDataset
  {
    std::filesystem::path root;
    std::string name;
    std::string expression;
    SearchOptions options;
    std::uint64_t fileCount{0};
    std::uint64_t byteCount{0};
    std::uint64_t expectedMatches{0};
    std::uint64_t expectedMatchingFiles{0};
    std::uint64_t expectedIgnoredFiles{0};
    std::uint64_t expectedHiddenFiles{0};
    std::uint64_t expectedBinaryFiles{0};
  };

  class TemporaryBenchmarkDataset
  {
  public:
    TemporaryBenchmarkDataset(std::string name, BenchmarkDataset dataset);
    ~TemporaryBenchmarkDataset();

    TemporaryBenchmarkDataset(const TemporaryBenchmarkDataset&) = delete;
    TemporaryBenchmarkDataset& operator=(const TemporaryBenchmarkDataset&) = delete;
    TemporaryBenchmarkDataset(TemporaryBenchmarkDataset&&) noexcept = delete;
    TemporaryBenchmarkDataset& operator=(TemporaryBenchmarkDataset&&) noexcept = delete;

    [[nodiscard]] const BenchmarkDataset& get() const;

  private:
    BenchmarkDataset dataset;
  };

  [[nodiscard]] TemporaryBenchmarkDataset makeManySmallFilesDataset();
  [[nodiscard]] TemporaryBenchmarkDataset makeFewLargeFilesDataset();
  [[nodiscard]] TemporaryBenchmarkDataset makeMixedTextAndBinaryDataset();
  [[nodiscard]] TemporaryBenchmarkDataset makeGitignoreHeavyDataset();
  [[nodiscard]] TemporaryBenchmarkDataset makeUnicodeContentDataset();
  [[nodiscard]] TemporaryBenchmarkDataset makeRegexHeavyContentDataset();

  [[nodiscard]] SearchQuery makeSearchQuery(const BenchmarkDataset& dataset);

} // namespace uburu::benchmarks
