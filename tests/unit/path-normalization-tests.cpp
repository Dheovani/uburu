#include "core/filesystem/path-normalization.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <stdexcept>

TEST_CASE("path normalization uses generic separators")
{
  CHECK(uburu::filesystem::normalizePathSeparators(R"(src\core\file.cpp)") == "src/core/file.cpp");
}

TEST_CASE("absolute path normalization treats an empty path as empty")
{
  CHECK(uburu::filesystem::normalizedAbsolutePath({}).empty());
}

TEST_CASE("relative path normalization removes lexical dot segments")
{
  const auto path = std::filesystem::path("src") / "." / "core" / ".." / "main.cpp";

  CHECK(uburu::filesystem::normalizedRelativePath(path) == "src/main.cpp");
}

TEST_CASE("relative path normalization rejects absolute paths")
{
  CHECK_THROWS_AS(uburu::filesystem::normalizedRelativePath(std::filesystem::temp_directory_path()),
                  std::invalid_argument);
}

TEST_CASE("path keys follow platform case rules")
{
  const auto left = uburu::filesystem::normalizedPathKey(std::filesystem::path("SRC") / "File.TXT");
  const auto right = uburu::filesystem::normalizedPathKey(std::filesystem::path("src") / "file.txt");

#ifdef _WIN32
  CHECK(left == right);
#else
  CHECK(left != right);
#endif
}

#ifdef _WIN32
TEST_CASE("windows path normalization preserves extended length prefixes")
{
  CHECK(uburu::filesystem::normalizedPathKey(R"(\\?\C:\Repo\Source\File.TXT)") == "//?/c:/repo/source/file.txt");
}

TEST_CASE("windows path normalization preserves UNC roots")
{
  CHECK(uburu::filesystem::normalizedPathKey(R"(\\Server\Share\Repo\File.TXT)") == "//server/share/repo/file.txt");
}
#endif

TEST_CASE("normalized path containment respects path segment boundaries")
{
  CHECK(uburu::filesystem::normalizedPathIsSameOrInside("src/core/file.cpp", "src"));
  CHECK(uburu::filesystem::normalizedPathIsSameOrInside("src", "src"));
  CHECK_FALSE(uburu::filesystem::normalizedPathIsSameOrInside("src-generated/file.cpp", "src"));
}
