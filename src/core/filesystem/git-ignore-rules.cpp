#include "core/filesystem/git-ignore-rules.hpp"

#include <algorithm>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace uburu::filesystem
{
  namespace
  {

    constexpr char comment_marker = '#';
    constexpr char escape_marker = '\\';
    constexpr char negation_marker = '!';
    constexpr char path_separator = '/';
    constexpr char wildcard_any_sequence = '*';
    constexpr char wildcard_single_character = '?';
    constexpr char carriage_return = '\r';
    constexpr char space = ' ';
    constexpr char horizontal_tab = '\t';

    bool is_space(char value)
    {
      return value == space || value == horizontal_tab;
    }

    void trim_unescaped_trailing_spaces(std::string& text)
    {
      while (!text.empty() && is_space(text.back())) {
        std::size_t slash_count = 0;
        for (std::size_t index = text.size() - 1; index > 0 && text[index - 1] == escape_marker;
             --index)
          ++slash_count;

        if (slash_count % 2 != 0)
          break;

        text.pop_back();
      }
    }

    std::string unescape_leading_marker(std::string text)
    {
      if (text.size() >= 2 && text.front() == escape_marker &&
         (text[1] == comment_marker || text[1] == negation_marker))
        text.erase(0, 1);

      return text;
    }

    std::string normalize_pattern_path(std::string text)
    {
      std::ranges::replace(text, '\\', path_separator);

      return text;
    }

    std::string normalize_relative_path(const std::filesystem::path& path)
    {
      std::filesystem::path normalized = path.lexically_normal();

      if (normalized.is_absolute())
        throw std::invalid_argument("Expected a relative path, got an absolute path.");

      return normalize_pattern_path(normalized.generic_string());
    }

    bool glob_matches(std::string_view pattern, std::string_view text)
    {
      std::size_t pattern_index = 0;
      std::size_t text_index = 0;
      std::optional<std::size_t> star_pattern_index;
      std::size_t star_text_index = 0;

      while (text_index < text.size()) {
        if (pattern_index < pattern.size() &&
            (pattern[pattern_index] == wildcard_single_character ||
             pattern[pattern_index] == text[text_index])) {
          ++pattern_index;
          ++text_index;

          continue;
        }

        if (pattern_index < pattern.size() && pattern[pattern_index] == wildcard_any_sequence) {
          star_pattern_index = pattern_index++;
          star_text_index = text_index;

          continue;
        }

        if (star_pattern_index) {
          pattern_index = *star_pattern_index + 1;
          text_index = ++star_text_index;

          continue;
        }

        return false;
      }

      while (pattern_index < pattern.size() && pattern[pattern_index] == wildcard_any_sequence)
        ++pattern_index;

      return pattern_index == pattern.size();
    }

    bool path_is_same_or_inside(std::string_view path, std::string_view base)
    {
      if (base.empty())
        return true;

      if (path == base)
        return true;

      return path.size() > base.size() && path.starts_with(base) && path[base.size()] == '/';
    }

    std::string path_relative_to_base(std::string_view path, std::string_view base)
    {
      if (base.empty())
        return std::string{path};

      if (path == base)
        return {};

      if (!path_is_same_or_inside(path, base))
        return {};

      return std::string{path.substr(base.size() + 1)};
    }

    bool basename_rule_matches(const GitIgnoreRule& rule, std::string_view relative_to_base, bool is_directory)
    {
      if (rule.directory_only && !is_directory) {
        std::size_t search_start = 0;
        while (search_start < relative_to_base.size()) {
          const auto separator = relative_to_base.find(path_separator, search_start);
          const auto component =
            separator == std::string_view::npos
              ? relative_to_base.substr(search_start)
              : relative_to_base.substr(search_start, separator - search_start);

          if (glob_matches(rule.pattern, component))
            return true;

          if (separator == std::string_view::npos)
            break;

          search_start = separator + 1;
        }

        return false;
      }

      const auto separator = relative_to_base.find_last_of(path_separator);
      const auto basename = separator == std::string_view::npos
        ? relative_to_base
        : relative_to_base.substr(separator + 1);

      return glob_matches(rule.pattern, basename);
    }

    bool path_rule_matches(const GitIgnoreRule& rule, std::string_view relative_to_base, bool is_directory)
    {
      if (rule.directory_only && !is_directory && !relative_to_base.starts_with(rule.pattern + "/"))
        return false;

      if (rule.directory_only)
        return relative_to_base == rule.pattern || relative_to_base.starts_with(rule.pattern + "/");

      if (rule.anchored)
        return glob_matches(rule.pattern, relative_to_base);

      if (glob_matches(rule.pattern, relative_to_base))
        return true;

      const auto suffix = std::string{"/"} + rule.pattern;

      return relative_to_base.ends_with(suffix);
    }

    bool rule_matches(const GitIgnoreRule& rule, const std::filesystem::path& relative_path, bool is_directory)
    {
      const auto path = normalize_relative_path(relative_path);
      const auto base = normalize_relative_path(rule.base_directory);

      if (!path_is_same_or_inside(path, base))
        return false;

      const auto relative_to_base = path_relative_to_base(path, base);
      if (relative_to_base.empty())
        return false;

      if (rule.basename_only)
        return basename_rule_matches(rule, relative_to_base, is_directory);

      return path_rule_matches(rule, relative_to_base, is_directory);
    }

    std::optional<GitIgnoreRule> parse_line(std::string line, const std::filesystem::path& base_directory)
    {
      if (!line.empty() && line.back() == carriage_return)
        line.pop_back();

      trim_unescaped_trailing_spaces(line);

      if (line.empty())
        return std::nullopt;

      if (line.front() == comment_marker)
        return std::nullopt;

      line = unescape_leading_marker(std::move(line));

      bool negated = false;
      if (!line.empty() && line.front() == negation_marker) {
        negated = true;
        line.erase(0, 1);
      }

      line = normalize_pattern_path(std::move(line));

      bool directory_only = false;
      while (!line.empty() && line.back() == path_separator) {
        directory_only = true;
        line.pop_back();
      }

      bool anchored = false;
      while (!line.empty() && line.front() == path_separator) {
        anchored = true;
        line.erase(0, 1);
      }

      if (line.empty())
        return std::nullopt;

      const bool basename_only = line.find(path_separator) == std::string::npos;

      return GitIgnoreRule{.base_directory = base_directory,
                           .pattern = std::move(line),
                           .negated = negated,
                           .directory_only = directory_only,
                           .anchored = anchored,
                           .basename_only = basename_only};
    }

  } // namespace

  std::vector<GitIgnoreRule> parse_git_ignore(std::string_view content,
                                              const std::filesystem::path& base_directory)
  {
    std::istringstream stream{std::string{content}};
    std::vector<GitIgnoreRule> rules;
    std::string line;

    while (std::getline(stream, line)) {
      auto rule = parse_line(std::move(line), base_directory);
      if (rule)
        rules.push_back(std::move(*rule));
    }

    return rules;
  }

  void GitIgnoreRules::append_file(const std::filesystem::path& ignore_file,
                                   const std::filesystem::path& base_directory)
  {
    std::ifstream stream(ignore_file, std::ios::binary);
    if (!stream)
      return;

    const std::string content{std::istreambuf_iterator<char>{stream},
                              std::istreambuf_iterator<char>{}};
    auto parsed_rules = parse_git_ignore(content, base_directory);
    rules_.insert(rules_.end(), std::make_move_iterator(parsed_rules.begin()),
                  std::make_move_iterator(parsed_rules.end()));
  }

  bool GitIgnoreRules::ignores(const std::filesystem::path& relative_path, bool is_directory) const
  {
    std::optional<bool> ignored;

    for (const auto& rule : rules_) {
      if (rule_matches(rule, relative_path, is_directory))
        ignored = !rule.negated;
    }

    return ignored.value_or(false);
  }

  bool GitIgnoreRules::empty() const
  {
    return rules_.empty();
  }

} // namespace uburu::filesystem
