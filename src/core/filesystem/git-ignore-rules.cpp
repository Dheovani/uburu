#include "core/filesystem/git-ignore-rules.hpp"
#include "core/filesystem/path-normalization.hpp"

#include <algorithm>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

namespace uburu::filesystem
{
  namespace
  {

    constexpr char commentMarker = '#';
    constexpr char escapeMarker = '\\';
    constexpr char negationMarker = '!';
    constexpr char pathSeparator = '/';
    constexpr char wildcardAnySequence = '*';
    constexpr char wildcardSingleCharacter = '?';
    constexpr char carriageReturn = '\r';
    constexpr char space = ' ';
    constexpr char horizontalTab = '\t';

    bool isSpace(char value)
    {
      return value == space || value == horizontalTab;
    }

    void trimUnescapedTrailingSpaces(std::string& text)
    {
      while (!text.empty() && isSpace(text.back())) {
        std::size_t slashCount = 0;
        for (std::size_t index = text.size() - 1; index > 0 && text[index - 1] == escapeMarker;
             --index)
          ++slashCount;

        if (slashCount % 2 != 0)
          break;

        text.pop_back();
      }
    }

    std::string unescapeLeadingMarker(std::string text)
    {
      if (text.size() >= 2 && text.front() == escapeMarker &&
         (text[1] == commentMarker || text[1] == negationMarker))
        text.erase(0, 1);

      return text;
    }

    bool globMatches(std::string_view pattern, std::string_view text)
    {
      std::size_t patternIndex = 0;
      std::size_t textIndex = 0;
      std::optional<std::size_t> starPatternIndex;
      std::size_t starTextIndex = 0;

      while (textIndex < text.size()) {
        if (patternIndex < pattern.size() &&
            (pattern[patternIndex] == wildcardSingleCharacter ||
             pattern[patternIndex] == text[textIndex])) {
          ++patternIndex;
          ++textIndex;

          continue;
        }

        if (patternIndex < pattern.size() && pattern[patternIndex] == wildcardAnySequence) {
          starPatternIndex = patternIndex++;
          starTextIndex = textIndex;

          continue;
        }

        if (starPatternIndex) {
          patternIndex = *starPatternIndex + 1;
          textIndex = ++starTextIndex;

          continue;
        }

        return false;
      }

      while (patternIndex < pattern.size() && pattern[patternIndex] == wildcardAnySequence)
        ++patternIndex;

      return patternIndex == pattern.size();
    }

    std::string pathRelativeToBase(std::string_view path, std::string_view base)
    {
      if (base.empty())
        return std::string{path};

      if (path == base)
        return {};

      if (!normalizedPathIsSameOrInside(path, base))
        return {};

      return std::string{path.substr(base.size() + 1)};
    }

    bool basenameRuleMatches(const GitIgnoreRule& rule, std::string_view relativeToBase, bool is_directory)
    {
      if (rule.directoryOnly && !is_directory) {
        std::size_t searchStart = 0;
        while (searchStart < relativeToBase.size()) {
          const auto separator = relativeToBase.find(pathSeparator, searchStart);
          const auto component =
            separator == std::string_view::npos
              ? relativeToBase.substr(searchStart)
              : relativeToBase.substr(searchStart, separator - searchStart);

          if (globMatches(rule.pattern, component))
            return true;

          if (separator == std::string_view::npos)
            break;

          searchStart = separator + 1;
        }

        return false;
      }

      const auto separator = relativeToBase.find_last_of(pathSeparator);
      const auto basename = separator == std::string_view::npos
        ? relativeToBase
        : relativeToBase.substr(separator + 1);

      return globMatches(rule.pattern, basename);
    }

    bool pathRuleMatches(const GitIgnoreRule& rule, std::string_view relativeToBase, bool is_directory)
    {
      if (rule.directoryOnly && !is_directory && !relativeToBase.starts_with(rule.pattern + "/"))
        return false;

      if (rule.directoryOnly)
        return relativeToBase == rule.pattern || relativeToBase.starts_with(rule.pattern + "/");

      if (rule.anchored)
        return globMatches(rule.pattern, relativeToBase);

      if (globMatches(rule.pattern, relativeToBase))
        return true;

      const auto suffix = std::string{"/"} + rule.pattern;

      return relativeToBase.ends_with(suffix);
    }

    bool ruleMatches(const GitIgnoreRule& rule, const std::filesystem::path& relativePath, bool is_directory)
    {
      const auto path = normalizedRelativePath(relativePath);
      const auto base = normalizedRelativePath(rule.baseDirectory);

      if (!normalizedPathIsSameOrInside(path, base))
        return false;

      const auto relativeToBase = pathRelativeToBase(path, base);
      if (relativeToBase.empty())
        return false;

      if (rule.basenameOnly)
        return basenameRuleMatches(rule, relativeToBase, is_directory);

      return pathRuleMatches(rule, relativeToBase, is_directory);
    }

    std::optional<GitIgnoreRule> parseLine(std::string line, const std::filesystem::path& baseDirectory)
    {
      if (!line.empty() && line.back() == carriageReturn)
        line.pop_back();

      trimUnescapedTrailingSpaces(line);

      if (line.empty())
        return std::nullopt;

      if (line.front() == commentMarker)
        return std::nullopt;

      line = unescapeLeadingMarker(std::move(line));

      bool negated = false;
      if (!line.empty() && line.front() == negationMarker) {
        negated = true;
        line.erase(0, 1);
      }

      line = normalizePathSeparators(std::move(line));

      bool directoryOnly = false;
      while (!line.empty() && line.back() == pathSeparator) {
        directoryOnly = true;
        line.pop_back();
      }

      bool anchored = false;
      while (!line.empty() && line.front() == pathSeparator) {
        anchored = true;
        line.erase(0, 1);
      }

      if (line.empty())
        return std::nullopt;

      const bool basenameOnly = line.find(pathSeparator) == std::string::npos;

      return GitIgnoreRule{.baseDirectory = baseDirectory,
                           .pattern = std::move(line),
                           .negated = negated,
                           .directoryOnly = directoryOnly,
                           .anchored = anchored,
                           .basenameOnly = basenameOnly};
    }

  } // namespace

  std::vector<GitIgnoreRule> parseGitIgnore(std::string_view content,
                                              const std::filesystem::path& baseDirectory)
  {
    std::istringstream stream{std::string{content}};
    std::vector<GitIgnoreRule> rules;
    std::string line;

    while (std::getline(stream, line)) {
      auto rule = parseLine(std::move(line), baseDirectory);
      if (rule)
        rules.push_back(std::move(*rule));
    }

    return rules;
  }

  void GitIgnoreRules::appendFile(const std::filesystem::path& ignoreFile,
                                   const std::filesystem::path& baseDirectory)
  {
    std::ifstream stream(ignoreFile, std::ios::binary);
    if (!stream)
      return;

    const std::string content{std::istreambuf_iterator<char>{stream},
                              std::istreambuf_iterator<char>{}};
    auto parsedRules = parseGitIgnore(content, baseDirectory);
    rules.insert(rules.end(), std::make_move_iterator(parsedRules.begin()),
                  std::make_move_iterator(parsedRules.end()));
  }

  bool GitIgnoreRules::ignores(const std::filesystem::path& relativePath, bool is_directory) const
  {
    std::optional<bool> ignored;

    for (const auto& rule : rules) {
      if (ruleMatches(rule, relativePath, is_directory))
        ignored = !rule.negated;
    }

    return ignored.value_or(false);
  }

  bool GitIgnoreRules::empty() const
  {
    return rules.empty();
  }

} // namespace uburu::filesystem
