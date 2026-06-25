#include "core/filesystem/recursive-file-scanner.hpp"

#include "core/filesystem/git-ignore-rules.hpp"
#include "core/filesystem/path-normalization.hpp"

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#endif

namespace uburu::filesystem
{
  namespace
  {

    std::string normalized_extension(std::filesystem::path path)
    {
      auto extension = path.extension().string();

      if (extension.starts_with('.'))
        extension.erase(0, 1);

      return normalized_path_key(std::move(extension));
    }

    bool is_hidden(const std::filesystem::path& path)
    {
      const auto name = path.filename().string();

      return !name.empty() && name.front() == '.';
    }

    bool is_sparse_file(const std::filesystem::path& path)
    {
#ifdef _WIN32
      const auto attributes = GetFileAttributesW(path.wstring().c_str());

      if (attributes == INVALID_FILE_ATTRIBUTES)
        return false;

      return (attributes & FILE_ATTRIBUTE_SPARSE_FILE) != 0;
#elif defined(__unix__) || defined(__APPLE__)
      struct stat status;

      if (stat(path.c_str(), &status) != 0)
        return false;

      if (!S_ISREG(status.st_mode))
        return false;

      constexpr auto posix_stat_block_size = 512ULL;

      return status.st_size > 0 && status.st_blocks >= 0 &&
             static_cast<unsigned long long>(status.st_blocks) * posix_stat_block_size
              < static_cast<unsigned long long>(status.st_size);
#else
      static_cast<void>(path);

      return false;
#endif
    }

    bool is_windows_reparse_point(const std::filesystem::path& path)
    {
#ifdef _WIN32
      const auto attributes = GetFileAttributesW(path.wstring().c_str());

      if (attributes == INVALID_FILE_ATTRIBUTES)
        return false;

      return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
      static_cast<void>(path);

      return false;
#endif
    }

    bool is_link_like_directory(const std::filesystem::directory_entry& entry)
    {
      std::error_code error;

      return entry.is_symlink(error) || is_windows_reparse_point(entry.path());
    }

    std::string directory_identity(const std::filesystem::path& directory)
    {
      std::error_code error;
      auto canonical = std::filesystem::weakly_canonical(directory, error);

      if (error)
        canonical = std::filesystem::absolute(directory, error).lexically_normal();

      if (error)
        return normalized_path_key(directory);

      return normalized_path_key(canonical);
    }

    bool has_allowed_extension(const std::filesystem::path& path, const std::vector<std::string>& extensions)
    {
      if (extensions.empty())
        return true;

      const auto extension = normalized_extension(path);

      return std::ranges::any_of(extensions, [&](const std::string& allowed) {
        auto normalized_allowed = normalized_path_key(allowed);

        if (normalized_allowed.starts_with('.'))
          normalized_allowed.erase(0, 1);

        return extension == normalized_allowed;
      });
    }

    bool is_included_directory(const std::filesystem::path& relative_path,
                               const std::vector<std::filesystem::path>& included_directories)
    {
      if (included_directories.empty())
        return true;

      const auto relative_key = normalized_path_key(relative_path);

      return std::ranges::any_of(included_directories, [&](const std::filesystem::path& included) {
        return normalized_path_is_same_or_inside(relative_key, normalized_path_key(included));
      });
    }

    bool is_excluded_directory(const std::filesystem::path& relative_path,
                               const std::vector<std::filesystem::path>& excluded_directories)
    {
      const auto relative_key = normalized_path_key(relative_path);

      return std::ranges::any_of(excluded_directories, [&](const std::filesystem::path& excluded) {
        return normalized_path_is_same_or_inside(relative_key, normalized_path_key(excluded));
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
      const auto text = normalized_path_key(relative_path);
      const auto matches = std::ranges::any_of(options.included_globs, [&](const std::string& glob) {
        return glob_matches(normalized_path_key(glob), text);
      });

      if (!options.included_globs.empty() && !matches)
        return false;

      return !std::ranges::any_of(options.excluded_globs, [&](const std::string& glob) {
        return glob_matches(normalized_path_key(glob), text);
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
        std::error_code left_error;
        std::error_code right_error;
        const auto left_is_directory = left.is_directory(left_error);
        const auto right_is_directory = right.is_directory(right_error);

        if (!left_error && !right_error && left_is_directory != right_is_directory)
          return left_is_directory;

        if (!left_is_directory && !right_is_directory) {
          const auto left_size = left.file_size(left_error);
          const auto right_size = right.file_size(right_error);

          if (!left_error && !right_error && left_size != right_size)
            return left_size < right_size;
        }

        return normalized_path_key(left.path()) < normalized_path_key(right.path());
      });

      return entries;
    }

    std::filesystem::path relative_directory(const std::filesystem::path& directory, const std::filesystem::path& root)
    {
      std::error_code error;
      auto relative = std::filesystem::relative(directory, root, error);
      if (error || relative == ".")
        return {};

      return relative;
    }

    GitIgnoreRules initial_ignore_rules(const std::filesystem::path& root, const SearchOptions& options)
    {
      GitIgnoreRules ignore_rules;

      if (!options.respect_gitignore)
        return ignore_rules;

      for (const auto& global_ignore_file : options.global_git_ignore_files)
        ignore_rules.append_file(global_ignore_file, {});

      ignore_rules.append_file(root / ".git" / "info" / "exclude", {});

      return ignore_rules;
    }

  } // namespace

  void RecursiveFileScanner::scan(const std::filesystem::path& root,
                                  const SearchOptions& options,
                                  FileSink sink,
                                  std::stop_token stop_token,
                                  diagnostics::SearchMetrics* metrics) const
  {
    auto flags = std::filesystem::directory_options::skip_permission_denied;

    if (options.follow_symlinks)
      flags |= std::filesystem::directory_options::follow_directory_symlink;

    std::unordered_set<std::string> visited_directories;
    std::function<bool(const std::filesystem::path&, GitIgnoreRules)> scan_directory;
    scan_directory = [&](const std::filesystem::path& directory, GitIgnoreRules ignore_rules) {
      const auto identity = directory_identity(directory);

      if (!visited_directories.insert(identity).second)
        return true;

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
          const auto link_like_directory = is_link_like_directory(item);

          if (link_like_directory && !options.follow_symlinks)
            continue;

          if (options.respect_gitignore && ignore_rules.ignores(relative_path, true))
            continue;

          if ((hidden && !options.include_hidden) ||
              is_excluded_directory(relative_path, options.excluded_directories))
            continue;

          if (!scan_directory(path, ignore_rules))
            return false;

          continue;
        }

        if (options.respect_gitignore && ignore_rules.ignores(relative_path, false)) {
          if (metrics != nullptr)
            ++metrics->ignored_files;

          continue;
        }

        if (!item.is_regular_file(error))
          continue;

        if (hidden && !options.include_hidden) {
          if (metrics != nullptr)
            ++metrics->hidden_files;

          continue;
        }

        if (is_excluded_directory(relative_path, options.excluded_directories) ||
            !is_included_directory(relative_path, options.included_directories) ||
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
                        .symlink = item.is_symlink(error),
                        .sparse = is_sparse_file(path)};

        if (!error && !sink(std::move(entry)))
          return false;
      }

      return true;
    };

    scan_directory(root, initial_ignore_rules(root, options));
  }

} // namespace uburu::filesystem
