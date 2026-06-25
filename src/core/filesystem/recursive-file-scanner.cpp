#include "core/filesystem/recursive-file-scanner.hpp"

#include "core/filesystem/git-ignore-rules.hpp"

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace uburu::filesystem
{
  namespace
  {

    char normalized_ascii(char value)
    {
#ifdef _WIN32
      if (value >= 'A' && value <= 'Z')
        return static_cast<char>(value + ('a' - 'A'));
#endif

      return value;
    }

    std::string normalized_string(std::string value)
    {
      for (auto& character : value)
        character = normalized_ascii(character);

      return value;
    }

    std::string normalized_extension(std::filesystem::path path)
    {
      auto extension = path.extension().string();

      if (extension.starts_with('.'))
        extension.erase(0, 1);

      return normalized_string(std::move(extension));
    }

    bool is_hidden(const std::filesystem::path& path)
    {
      const auto name = path.filename().string();

      return !name.empty() && name.front() == '.';
    }

    bool has_allowed_extension(const std::filesystem::path& path,
                               const std::vector<std::string>& extensions)
    {
      if (extensions.empty())
        return true;

      const auto extension = normalized_extension(path);

      return std::ranges::any_of(extensions, [&](const std::string& allowed) {
        auto normalized_allowed = normalized_string(allowed);

        if (normalized_allowed.starts_with('.'))
          normalized_allowed.erase(0, 1);

        return extension == normalized_allowed;
      });
    }

    bool is_same_or_inside(const std::filesystem::path& path, const std::filesystem::path& base)
    {
      auto path_iterator = path.begin();
      auto base_iterator = base.begin();

      for (; base_iterator != base.end(); ++base_iterator, ++path_iterator) {
        if (path_iterator == path.end() || *path_iterator != *base_iterator)
          return false;
      }

      return true;
    }

    bool is_included_directory(const std::filesystem::path& relative_path,
                               const std::vector<std::filesystem::path>& included_directories)
    {
      if (included_directories.empty())
        return true;

      return std::ranges::any_of(included_directories, [&](const std::filesystem::path& included) {
        return is_same_or_inside(relative_path, included);
      });
    }

    bool is_excluded_directory(const std::filesystem::path& relative_path,
                               const std::vector<std::filesystem::path>& excluded_directories)
    {
      return std::ranges::any_of(excluded_directories, [&](const std::filesystem::path& excluded) {
        return is_same_or_inside(relative_path, excluded);
      });
    }

    bool glob_matches(std::string_view pattern, std::string_view text)
    {
      std::size_t pattern_index = 0;
      std::size_t text_index = 0;
      std::optional<std::size_t> star_pattern_index;
      std::size_t star_text_index = 0;

      while (text_index < text.size()) {
        if (pattern_index < pattern.size() &&
            (pattern[pattern_index] == '?' || pattern[pattern_index] == text[text_index])) {
          ++pattern_index;
          ++text_index;

          continue;
        }

        if (pattern_index < pattern.size() && pattern[pattern_index] == '*') {
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

      while (pattern_index < pattern.size() && pattern[pattern_index] == '*')
        ++pattern_index;

      return pattern_index == pattern.size();
    }

    bool passes_globs(const std::filesystem::path& relative_path, const SearchOptions& options)
    {
      const auto text = normalized_string(relative_path.generic_string());

      if (!options.included_globs.empty() &&
          !std::ranges::any_of(options.included_globs, [&](const std::string& glob) {
            return glob_matches(normalized_string(glob), text);
          }))
        return false;

      return !std::ranges::any_of(options.excluded_globs, [&](const std::string& glob) {
        return glob_matches(normalized_string(glob), text);
      });
    }

    std::vector<std::filesystem::directory_entry>
    sorted_directory_entries(const std::filesystem::path& directory,
                             std::filesystem::directory_options options)
    {
      std::error_code error;
      std::filesystem::directory_iterator iterator(directory, options, error);
      const std::filesystem::directory_iterator end;
      std::vector<std::filesystem::directory_entry> entries;

      while (!error && iterator != end) {
        entries.push_back(*iterator);
        iterator.increment(error);
      }

      std::ranges::sort(entries, [](const auto& left, const auto& right) {
        return normalized_string(left.path().generic_string()) <
               normalized_string(right.path().generic_string());
      });

      return entries;
    }

    std::filesystem::path relative_directory(const std::filesystem::path& directory,
                                             const std::filesystem::path& root)
    {
      std::error_code error;
      auto relative = std::filesystem::relative(directory, root, error);
      if (error || relative == ".")
        return {};

      return relative;
    }

  } // namespace

  void RecursiveFileScanner::scan(const std::filesystem::path& root, const SearchOptions& options,
                                  FileSink sink, std::stop_token stop_token) const
  {
    auto flags = std::filesystem::directory_options::skip_permission_denied;

    if (options.follow_symlinks)
      flags |= std::filesystem::directory_options::follow_directory_symlink;

    std::function<bool(const std::filesystem::path&, GitIgnoreRules)> scan_directory;
    scan_directory = [&](const std::filesystem::path& directory, GitIgnoreRules ignore_rules) {
      if (options.respect_gitignore)
        ignore_rules.append_file(directory / ".gitignore", relative_directory(directory, root));

      for (const auto& item : sorted_directory_entries(directory, flags)) {
        if (stop_token.stop_requested())
          return false;

        std::error_code error;
        const auto path = item.path();
        const bool hidden = is_hidden(path);
        const auto relative_path = std::filesystem::relative(path, root, error);

        if (error)
          continue;

        if (item.is_directory(error)) {
          if (options.respect_gitignore && ignore_rules.ignores(relative_path, true))
            continue;

          if ((hidden && !options.include_hidden) ||
              is_excluded_directory(relative_path, options.excluded_directories))
            continue;

          if (!scan_directory(path, ignore_rules))
            return false;

          continue;
        }

        if (options.respect_gitignore && ignore_rules.ignores(relative_path, false))
          continue;

        if (!item.is_regular_file(error) || (hidden && !options.include_hidden) ||
            !is_included_directory(relative_path, options.included_directories) ||
            is_excluded_directory(relative_path, options.excluded_directories) ||
            !has_allowed_extension(path, options.extensions) ||
            !passes_globs(relative_path, options))
          continue;

        const auto size = item.file_size(error);

        if (error || size > options.maximum_file_size)
          continue;

        FileEntry entry{.absolute_path = path,
                        .relative_path = relative_path,
                        .size = size,
                        .modified_at = item.last_write_time(error),
                        .hidden = hidden,
                        .binary = false,
                        .symlink = item.is_symlink(error)};

        if (!error && !sink(std::move(entry)))
          return false;
      }

      return true;
    };

    scan_directory(root, GitIgnoreRules{});
  }

} // namespace uburu::filesystem
