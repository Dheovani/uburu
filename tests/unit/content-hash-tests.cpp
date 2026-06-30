#include "core/index/content-hash.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace
{

  class TemporaryDirectory
  {
  public:
    explicit TemporaryDirectory(std::string name)
      : pathValue(std::filesystem::temp_directory_path() / uniqueName(std::move(name)))
    {
      std::error_code error;

      std::filesystem::remove_all(pathValue, error);
      std::filesystem::create_directories(pathValue);
    }

    ~TemporaryDirectory()
    {
      std::error_code error;

      std::filesystem::remove_all(pathValue, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const
    {
      return pathValue;
    }

  private:
    [[nodiscard]] static std::string uniqueName(std::string name)
    {
      const auto now = std::chrono::steady_clock::now().time_since_epoch().count();

      return name + "-" + std::to_string(now);
    }

    std::filesystem::path pathValue;
  };

  [[nodiscard]] std::span<const std::byte> asBytes(std::string_view text)
  {
    return std::as_bytes(std::span{text.data(), text.size()});
  }

  void writeFile(const std::filesystem::path& path, std::string_view content)
  {
    std::ofstream file(path, std::ios::binary);
    file << content;
  }

} // namespace

TEST_CASE("content hash computes SHA-256 for known byte vectors")
{
  const auto emptyHash = uburu::index::computeContentHash(asBytes(""));
  const auto abcHash = uburu::index::computeContentHash(asBytes("abc"));

  CHECK(emptyHash.algorithm == uburu::ContentHashAlgorithm::sha256);
  CHECK(emptyHash.value == "e3b0c44298fc1c149afbf4c8996fb924"
                           "27ae41e4649b934ca495991b7852b855");
  CHECK(abcHash.algorithm == uburu::ContentHashAlgorithm::sha256);
  CHECK(abcHash.value == "ba7816bf8f01cfea414140de5dae2223"
                         "b00361a396177a9cb410ff61f20015ad");
}

TEST_CASE("content hash streams files without changing the digest")
{
  TemporaryDirectory directory("uburu-content-hash-file-test");
  const auto path = directory.path() / "sample.txt";

  writeFile(path, "abc");

  const auto hash = uburu::index::computeFileContentHash(path);

  REQUIRE(hash.has_value());
  CHECK(hash->algorithm == uburu::ContentHashAlgorithm::sha256);
  CHECK(hash->value == "ba7816bf8f01cfea414140de5dae2223"
                       "b00361a396177a9cb410ff61f20015ad");
}

TEST_CASE("content hash observes pre-cancelled file hashing")
{
  TemporaryDirectory directory("uburu-content-hash-cancel-test");
  const auto path = directory.path() / "sample.txt";
  std::stop_source stopSource;

  writeFile(path, "abc");
  stopSource.request_stop();

  const auto hash = uburu::index::computeFileContentHash(path, stopSource.get_token());

  CHECK_FALSE(hash.has_value());
}
