#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace uburu::filesystem
{

  struct GitIgnoreRule
  {
    std::filesystem::path baseDirectory;
    std::string pattern;
    bool negated{false};
    bool directoryOnly{false};
    bool anchored{false};
    bool basenameOnly{false};
  };

  class GitIgnoreRules
  {
  public:
    void appendFile(const std::filesystem::path& ignoreFile,
                     const std::filesystem::path& baseDirectory);

    [[nodiscard]] bool ignores(const std::filesystem::path& relativePath, bool is_directory) const;

    [[nodiscard]] bool empty() const;

  private:
    std::vector<GitIgnoreRule> rules;
  };

  [[nodiscard]] std::vector<GitIgnoreRule>
  parseGitIgnore(std::string_view content, const std::filesystem::path& baseDirectory);

} // namespace uburu::filesystem
