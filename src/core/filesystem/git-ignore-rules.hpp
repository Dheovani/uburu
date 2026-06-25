#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace uburu::filesystem
{

  struct GitIgnoreRule
  {
    std::filesystem::path base_directory;
    std::string pattern;
    bool negated{false};
    bool directory_only{false};
    bool anchored{false};
    bool basename_only{false};
  };

  class GitIgnoreRules
  {
  public:
    void append_file(const std::filesystem::path& ignore_file,
                     const std::filesystem::path& base_directory);

    [[nodiscard]] bool ignores(const std::filesystem::path& relative_path, bool is_directory) const;

    [[nodiscard]] bool empty() const;

  private:
    std::vector<GitIgnoreRule> rules_;
  };

  [[nodiscard]] std::vector<GitIgnoreRule>
  parse_git_ignore(std::string_view content, const std::filesystem::path& base_directory);

} // namespace uburu::filesystem
