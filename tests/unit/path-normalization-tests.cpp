#include "core/filesystem/path-normalization.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <stdexcept>

TEST_CASE("path normalization uses generic separators")
{
  CHECK(uburu::filesystem::normalize_path_separators(R"(src\core\file.cpp)") ==
        "src/core/file.cpp");
}

TEST_CASE("relative path normalization removes lexical dot segments")
{
  const auto path = std::filesystem::path("src") / "." / "core" / ".." / "main.cpp";

  CHECK(uburu::filesystem::normalized_relative_path(path) == "src/main.cpp");
}

TEST_CASE("relative path normalization rejects absolute paths")
{
  CHECK_THROWS_AS(uburu::filesystem::normalized_relative_path(std::filesystem::temp_directory_path()),
                  std::invalid_argument);
}

TEST_CASE("path keys follow platform case rules")
{
  const auto left = uburu::filesystem::normalized_path_key(std::filesystem::path("SRC") / "File.TXT");
  const auto right =
    uburu::filesystem::normalized_path_key(std::filesystem::path("src") / "file.txt");

#ifdef _WIN32
  CHECK(left == right);
#else
  CHECK(left != right);
#endif
}

#ifdef _WIN32
TEST_CASE("windows path normalization preserves extended length prefixes")
{
  CHECK(uburu::filesystem::normalized_path_key(R"(\\?\C:\Repo\Source\File.TXT)") ==
        "//?/c:/repo/source/file.txt");
}

TEST_CASE("windows path normalization preserves UNC roots")
{
  CHECK(uburu::filesystem::normalized_path_key(R"(\\Server\Share\Repo\File.TXT)") ==
        "//server/share/repo/file.txt");
}
#endif

TEST_CASE("normalized path containment respects path segment boundaries")
{
  CHECK(uburu::filesystem::normalized_path_is_same_or_inside("src/core/file.cpp", "src"));
  CHECK(uburu::filesystem::normalized_path_is_same_or_inside("src", "src"));
  CHECK_FALSE(uburu::filesystem::normalized_path_is_same_or_inside("src-generated/file.cpp", "src"));
}
