#include "core/filesystem/git-ignore-rules.hpp"
#include "helpers/temporary-paths.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>

TEST_CASE("git ignore rules support comments, basename patterns and negation")
{
  uburu::filesystem::GitIgnoreRules rules;
  const auto parsed = uburu::filesystem::parseGitIgnore("# comment\n*.log\n!important.log\n", std::filesystem::path{});

  REQUIRE(parsed.size() == 2);
  CHECK(parsed[0].pattern == "*.log");
  CHECK_FALSE(parsed[0].negated);
  CHECK(parsed[1].pattern == "important.log");
  CHECK(parsed[1].negated);
}

TEST_CASE("git ignore rules apply the last matching rule")
{
  uburu::filesystem::GitIgnoreRules rules;

  uburu::tests::TemporaryFile temporaryFile("uburu-git-ignore-rules-test.gitignore");
  uburu::tests::writeFile(temporaryFile.path(), "*.log\n!important.log\n");

  rules.appendFile(temporaryFile.path(), {});

  CHECK(rules.ignores("debug.log", false));
  CHECK_FALSE(rules.ignores("important.log", false));
}

TEST_CASE("git ignore rules support nested base directories")
{
  uburu::tests::TemporaryFile temporaryFile("uburu-git-ignore-nested-test.gitignore");
  uburu::tests::writeFile(temporaryFile.path(), "*.tmp\n");

  uburu::filesystem::GitIgnoreRules rules;
  rules.appendFile(temporaryFile.path(), std::filesystem::path("src"));

  CHECK(rules.ignores(std::filesystem::path("src") / "cache.tmp", false));
  CHECK_FALSE(rules.ignores(std::filesystem::path("tests") / "cache.tmp", false));
}

TEST_CASE("git ignore rules support directory-only patterns")
{
  uburu::tests::TemporaryFile temporaryFile("uburu-git-ignore-directory-test.gitignore");
  uburu::tests::writeFile(temporaryFile.path(), "build/\n");

  uburu::filesystem::GitIgnoreRules rules;
  rules.appendFile(temporaryFile.path(), {});

  CHECK(rules.ignores("build", true));
  CHECK(rules.ignores(std::filesystem::path("build") / "output.o", false));
  CHECK_FALSE(rules.ignores("build.txt", false));
}
