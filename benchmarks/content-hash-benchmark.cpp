#include "core/index/content-hash.hpp"

#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace
{

  constexpr std::size_t kibibyte = 1024U;
  constexpr std::size_t mebibyte = kibibyte * kibibyte;
  constexpr std::size_t datasetByteCount = 16U * mebibyte;
  constexpr int repeatCount = 8;
  constexpr std::size_t datasetPatternMultiplier = 31U;
  constexpr std::size_t byteMask = 0xFFU;
  constexpr double bytesPerMebibyte = static_cast<double>(mebibyte);

  [[nodiscard]] std::vector<std::byte> makeDataset()
  {
    std::vector<std::byte> dataset(datasetByteCount);

    for (std::size_t index = 0; index < dataset.size(); ++index) {
      dataset[index] = static_cast<std::byte>((index * datasetPatternMultiplier) & byteMask);
    }

    return dataset;
  }

  [[nodiscard]] double seconds(std::chrono::steady_clock::duration duration)
  {
    return std::chrono::duration<double>(duration).count();
  }

} // namespace

int main()
{
  const auto dataset = makeDataset();
  const auto startedAt = std::chrono::steady_clock::now();

  uburu::index::ContentHash lastHash;

  for (int iteration = 0; iteration < repeatCount; ++iteration) {
    lastHash = uburu::index::computeContentHash(dataset);
  }

  const auto elapsed = seconds(std::chrono::steady_clock::now() - startedAt);
  const auto totalMebibytes = static_cast<double>(dataset.size() * repeatCount) / bytesPerMebibyte;
  const auto throughput = totalMebibytes / elapsed;

  std::cout << "content_hash_algorithm=sha256\n";
  std::cout << "dataset_mib=" << dataset.size() / mebibyte << '\n';
  std::cout << "repeat_count=" << repeatCount << '\n';
  std::cout << "elapsed_seconds=" << std::fixed << std::setprecision(6) << elapsed << '\n';
  std::cout << "throughput_mib_per_second=" << std::fixed << std::setprecision(2) << throughput << '\n';
  std::cout << "last_hash=" << lastHash.value << '\n';

  return 0;
}
