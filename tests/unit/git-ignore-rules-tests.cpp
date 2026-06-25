#include "core/filesystem/git-ignore-rules.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

TEST_CASE("git ignore rules support comments, basename patterns and negation")
{
  uburu::filesystem::GitIgnoreRules rules;
  const auto parsed = uburu::filesystem::parse_git_ignore("# comment\n*.log\n!important.log\n",
                                                          std::filesystem::path{});

  REQUIRE(parsed.size() == 2);
  CHECK(parsed[0].pattern == "*.log");
  CHECK_FALSE(parsed[0].negated);
  CHECK(parsed[1].pattern == "important.log");
  CHECK(parsed[1].negated);
}

TEST_CASE("git ignore rules apply the last matching rule")
{
  uburu::filesystem::GitIgnoreRules rules;

  const auto temporary_file =
      std::filesystem::temp_directory_path() / "uburu-git-ignore-rules-test.gitignore";
  {
    std::ofstream file(temporary_file, std::ios::binary);
    file << "*.log\n!important.log\n";
  }

  rules.append_file(temporary_file, {});
  std::filesystem::remove(temporary_file);

  CHECK(rules.ignores("debug.log", false));
  CHECK_FALSE(rules.ignores("important.log", false));
}

TEST_CASE("git ignore rules support nested base directories")
{
  const auto temporary_file =
      std::filesystem::temp_directory_path() / "uburu-git-ignore-nested-test.gitignore";
  {
    std::ofstream file(temporary_file, std::ios::binary);
    file << "*.tmp\n";
  }

  uburu::filesystem::GitIgnoreRules rules;
  rules.append_file(temporary_file, std::filesystem::path("src"));
  std::filesystem::remove(temporary_file);

  CHECK(rules.ignores(std::filesystem::path("src") / "cache.tmp", false));
  CHECK_FALSE(rules.ignores(std::filesystem::path("tests") / "cache.tmp", false));
}

TEST_CASE("git ignore rules support directory-only patterns")
{
  const auto temporary_file =
      std::filesystem::temp_directory_path() / "uburu-git-ignore-directory-test.gitignore";
  {
    std::ofstream file(temporary_file, std::ios::binary);
    file << "build/\n";
  }

  uburu::filesystem::GitIgnoreRules rules;
  rules.append_file(temporary_file, {});
  std::filesystem::remove(temporary_file);

  CHECK(rules.ignores("build", true));
  CHECK(rules.ignores(std::filesystem::path("build") / "output.o", false));
  CHECK_FALSE(rules.ignores("build.txt", false));
}
