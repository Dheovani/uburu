#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace uburu::filesystem
{

  /**
   * Represents one parsed .gitignore rule with enough metadata for ordered matching.
   */
  struct GitIgnoreRule
  {
    std::filesystem::path baseDirectory;
    std::string pattern;
    bool negated{false};
    bool directoryOnly{false};
    bool anchored{false};
    bool basenameOnly{false};
  };

  /**
   * Maintains ordered .gitignore rules and evaluates the last matching rule.
   */
  class GitIgnoreRules
  {
  public:
    void appendFile(const std::filesystem::path& ignoreFile, const std::filesystem::path& baseDirectory);

    [[nodiscard]]
    bool ignores(const std::filesystem::path& relativePath, bool isDirectory) const;

    [[nodiscard]]
    bool empty() const;

  private:
    std::vector<GitIgnoreRule> rules;
  };

  /**
   * Parses .gitignore content relative to the directory that owns that ignore file.
   */
  [[nodiscard]]
  std::vector<GitIgnoreRule> parseGitIgnore(
    std::string_view content,
    const std::filesystem::path& baseDirectory);

} // namespace uburu::filesystem
