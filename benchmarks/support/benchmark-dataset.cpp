#include "benchmark-dataset.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <string_view>
#include <system_error>
#include <utility>

namespace uburu::benchmarks
{
  namespace
  {

    constexpr std::size_t smallFileCount = 256;
    constexpr std::size_t smallFileMatchModulo = 4;
    constexpr std::size_t largeFileCount = 4;
    constexpr std::size_t largeFileLineCount = 4096;
    constexpr std::size_t largeFileMatchModulo = 16;
    constexpr std::size_t binaryFileCount = 24;
    constexpr std::size_t visibleTextFileCount = 64;
    constexpr std::size_t hiddenTextFileCount = 12;
    constexpr std::size_t gitignoreDirectoryCount = 16;
    constexpr std::size_t gitignoreFilesPerDirectory = 12;
    constexpr std::size_t regexFileCount = 96;
    constexpr std::uint64_t uniqueNameMultiplier = 1'315'423'911U;
    constexpr unsigned char binaryPayloadByte = 0x00U;

    [[nodiscard]] std::filesystem::path uniqueDatasetRoot(std::string_view name)
    {
      const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
      auto directoryName = std::string{name};
      directoryName += "-";
      directoryName += std::to_string(static_cast<std::uint64_t>(stamp) * uniqueNameMultiplier);

      return std::filesystem::temp_directory_path() / directoryName;
    }

    void writeTextFile(const std::filesystem::path& path, std::string_view content, BenchmarkDataset& dataset)
    {
      std::filesystem::create_directories(path.parent_path());

      std::ofstream file(path, std::ios::binary);
      file << content;

      ++dataset.fileCount;
      dataset.byteCount += static_cast<std::uint64_t>(content.size());
    }

    void writeBinaryFile(const std::filesystem::path& path, std::size_t byteCount, BenchmarkDataset& dataset)
    {
      std::filesystem::create_directories(path.parent_path());

      std::ofstream file(path, std::ios::binary);

      for (std::size_t index = 0; index < byteCount; ++index)
        file.put(static_cast<char>(binaryPayloadByte));

      ++dataset.fileCount;
      dataset.byteCount += static_cast<std::uint64_t>(byteCount);
    }

    [[nodiscard]] SearchOptions defaultContentOptions()
    {
      SearchOptions options;
      options.target = SearchTarget::content;
      options.contextBeforeLines = 0;
      options.contextAfterLines = 0;
      options.resultLimit = 1'000'000;
      options.perFileResultLimit = 10'000;

      return options;
    }

    void addMatchingLine(std::string& content, std::string_view expression)
    {
      content += "prefix ";
      content += expression;
      content += " suffix with deterministic benchmark text\n";
    }

    void addNonMatchingLine(std::string& content)
    {
      content += "ordinary deterministic benchmark text without the target token\n";
    }

  } // namespace

  TemporaryBenchmarkDataset::TemporaryBenchmarkDataset(std::string name, BenchmarkDataset dataset)
    : dataset(std::move(dataset))
  {
    this->dataset.name = std::move(name);
  }

  TemporaryBenchmarkDataset::~TemporaryBenchmarkDataset()
  {
    std::error_code error;
    std::filesystem::remove_all(dataset.root, error);
  }

  const BenchmarkDataset& TemporaryBenchmarkDataset::get() const
  {
    return dataset;
  }

  TemporaryBenchmarkDataset makeManySmallFilesDataset()
  {
    BenchmarkDataset dataset;
    dataset.name = "many-small-files";
    dataset.root = uniqueDatasetRoot("uburu-benchmark-many-small-files");
    dataset.expression = "needle";
    dataset.options = defaultContentOptions();
    dataset.options.extensions = {"txt"};

    for (std::size_t index = 0; index < smallFileCount; ++index) {
      std::string content;

      if (index % smallFileMatchModulo == 0) {
        addMatchingLine(content, dataset.expression);
        ++dataset.expectedMatches;
        ++dataset.expectedMatchingFiles;
      } else {
        addNonMatchingLine(content);
      }

      writeTextFile(dataset.root / "src" / ("file-" + std::to_string(index) + ".txt"), content, dataset);
    }

    return TemporaryBenchmarkDataset(dataset.name, std::move(dataset));
  }

  TemporaryBenchmarkDataset makeFewLargeFilesDataset()
  {
    BenchmarkDataset dataset;
    dataset.name = "few-large-files";
    dataset.root = uniqueDatasetRoot("uburu-benchmark-few-large-files");
    dataset.expression = "needle";
    dataset.options = defaultContentOptions();
    dataset.options.extensions = {"txt"};

    for (std::size_t fileIndex = 0; fileIndex < largeFileCount; ++fileIndex) {
      std::string content;
      bool matchedFile = false;

      for (std::size_t lineIndex = 0; lineIndex < largeFileLineCount; ++lineIndex) {
        if ((lineIndex + fileIndex) % largeFileMatchModulo == 0) {
          addMatchingLine(content, dataset.expression);
          ++dataset.expectedMatches;
          matchedFile = true;
        } else {
          addNonMatchingLine(content);
        }
      }

      if (matchedFile)
        ++dataset.expectedMatchingFiles;

      writeTextFile(dataset.root / ("large-" + std::to_string(fileIndex) + ".txt"), content, dataset);
    }

    return TemporaryBenchmarkDataset(dataset.name, std::move(dataset));
  }

  TemporaryBenchmarkDataset makeMixedTextAndBinaryDataset()
  {
    BenchmarkDataset dataset;
    dataset.name = "mixed-text-and-binary";
    dataset.root = uniqueDatasetRoot("uburu-benchmark-mixed-text-and-binary");
    dataset.expression = "needle";
    dataset.options = defaultContentOptions();
    dataset.options.extensions = {"txt", "bin"};
    dataset.options.includeBinary = false;

    for (std::size_t index = 0; index < visibleTextFileCount; ++index) {
      std::string content;
      addMatchingLine(content, dataset.expression);
      ++dataset.expectedMatches;
      ++dataset.expectedMatchingFiles;
      writeTextFile(dataset.root / "text" / ("visible-" + std::to_string(index) + ".txt"), content, dataset);
    }

    for (std::size_t index = 0; index < hiddenTextFileCount; ++index) {
      std::string content;
      addMatchingLine(content, dataset.expression);
      writeTextFile(dataset.root / "text" / (".hidden-" + std::to_string(index) + ".txt"), content, dataset);
      ++dataset.expectedHiddenFiles;
    }

    for (std::size_t index = 0; index < binaryFileCount; ++index) {
      writeBinaryFile(dataset.root / "binary" / ("payload-" + std::to_string(index) + ".bin"), 1024, dataset);
      ++dataset.expectedBinaryFiles;
    }

    return TemporaryBenchmarkDataset(dataset.name, std::move(dataset));
  }

  TemporaryBenchmarkDataset makeGitignoreHeavyDataset()
  {
    BenchmarkDataset dataset;
    dataset.name = "gitignore-heavy-tree";
    dataset.root = uniqueDatasetRoot("uburu-benchmark-gitignore-heavy-tree");
    dataset.expression = "needle";
    dataset.options = defaultContentOptions();
    dataset.options.extensions = {"txt"};
    dataset.options.respectGitignore = true;

    writeTextFile(dataset.root / ".gitignore", "ignored-*/\n*.tmp\n!important.tmp\n", dataset);

    for (std::size_t directoryIndex = 0; directoryIndex < gitignoreDirectoryCount; ++directoryIndex) {
      for (std::size_t fileIndex = 0; fileIndex < gitignoreFilesPerDirectory; ++fileIndex) {
        std::string content;
        addMatchingLine(content, dataset.expression);
        writeTextFile(dataset.root / ("ignored-" + std::to_string(directoryIndex)) /
                        ("file-" + std::to_string(fileIndex) + ".txt"),
                      content,
                      dataset);
        ++dataset.expectedIgnoredFiles;
      }
    }

    for (std::size_t index = 0; index < visibleTextFileCount; ++index) {
      std::string content;
      addMatchingLine(content, dataset.expression);
      ++dataset.expectedMatches;
      ++dataset.expectedMatchingFiles;
      writeTextFile(dataset.root / "visible" / ("kept-" + std::to_string(index) + ".txt"), content, dataset);
    }

    return TemporaryBenchmarkDataset(dataset.name, std::move(dataset));
  }

  TemporaryBenchmarkDataset makeUnicodeContentDataset()
  {
    BenchmarkDataset dataset;
    dataset.name = "unicode-content";
    dataset.root = uniqueDatasetRoot("uburu-benchmark-unicode-content");
    dataset.expression = "geração";
    dataset.options = defaultContentOptions();
    dataset.options.extensions = {"txt"};

    constexpr std::array<std::string_view, 4> samples{
      "A geração e a corrupção da matéria aparecem neste arquivo.\n",
      "Outra linha com GERAÇÃO para medir case folding Unicode.\n",
      "Texto sem correspondência relevante.\n",
      "geração geração geração em sequência para múltiplas ocorrências.\n",
    };

    for (std::size_t index = 0; index < samples.size(); ++index) {
      writeTextFile(dataset.root / ("unicode-" + std::to_string(index) + ".txt"), samples[index], dataset);

      if (index != 2) {
        ++dataset.expectedMatchingFiles;
        dataset.expectedMatches += index == 3 ? 3 : 1;
      }
    }

    return TemporaryBenchmarkDataset(dataset.name, std::move(dataset));
  }

  TemporaryBenchmarkDataset makeRegexHeavyContentDataset()
  {
    BenchmarkDataset dataset;
    dataset.name = "regex-heavy-content";
    dataset.root = uniqueDatasetRoot("uburu-benchmark-regex-heavy-content");
    dataset.expression = R"(user-[0-9]{4}@example\.(com|org))";
    dataset.options = defaultContentOptions();
    dataset.options.mode = SearchMode::regex;
    dataset.options.extensions = {"txt"};

    for (std::size_t index = 0; index < regexFileCount; ++index) {
      std::string content;
      content += "contact=user-";
      content += std::to_string(1000 + index);
      content += index % 2 == 0 ? "@example.com\n" : "@example.org\n";
      content += "noise user-abcd@example.invalid\n";
      ++dataset.expectedMatches;
      ++dataset.expectedMatchingFiles;
      writeTextFile(dataset.root / "regex" / ("contact-" + std::to_string(index) + ".txt"), content, dataset);
    }

    return TemporaryBenchmarkDataset(dataset.name, std::move(dataset));
  }

  SearchQuery makeSearchQuery(const BenchmarkDataset& dataset)
  {
    return SearchQuery{.root = dataset.root, .scope = {}, .expression = dataset.expression, .options = dataset.options};
  }

} // namespace uburu::benchmarks
