#include "core/filesystem/recursive-file-scanner.hpp"

#include <algorithm>
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

  } // namespace

  void RecursiveFileScanner::scan(const std::filesystem::path& root, const SearchOptions& options,
                                  FileSink sink, std::stop_token stop_token) const
  {
    std::error_code error;
    auto flags = std::filesystem::directory_options::skip_permission_denied;
    if (options.follow_symlinks)
      flags |= std::filesystem::directory_options::follow_directory_symlink;

    std::filesystem::recursive_directory_iterator iterator(root, flags, error);
    const std::filesystem::recursive_directory_iterator end;
    while (!error && iterator != end && !stop_token.stop_requested()) {
      const auto path = iterator->path();
      const bool hidden = is_hidden(path);
      const auto relative_path = std::filesystem::relative(path, root, error);

      if (error) {
        error.clear();
        iterator.increment(error);

        continue;
      }

      if (iterator->is_directory(error)) {
        if ((hidden && !options.include_hidden) ||
            is_excluded_directory(relative_path, options.excluded_directories))
          iterator.disable_recursion_pending();

        iterator.increment(error);

        continue;
      }

      if (!iterator->is_regular_file(error) || (hidden && !options.include_hidden) ||
          !is_included_directory(relative_path, options.included_directories) ||
          is_excluded_directory(relative_path, options.excluded_directories) ||
          !has_allowed_extension(path, options.extensions) ||
          !passes_globs(relative_path, options)) {
        iterator.increment(error);

        continue;
      }

      const auto size = iterator->file_size(error);
      if (!error && size <= options.maximum_file_size) {
        FileEntry entry{.absolute_path = path,
                        .relative_path = relative_path,
                        .size = size,
                        .modified_at = iterator->last_write_time(error),
                        .hidden = hidden,
                        .binary = false,
                        .symlink = iterator->is_symlink(error)};
        if (!error && !sink(std::move(entry)))
          return;
      }

      error.clear();
      iterator.increment(error);
    }
  }

} // namespace uburu::filesystem
