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

    std::string normalizedExtension(std::filesystem::path path)
    {
      auto extension = path.extension().string();

      if (extension.starts_with('.'))
        extension.erase(0, 1);

      return normalizedPathKey(std::move(extension));
    }

    bool isHidden(const std::filesystem::path& path)
    {
      const auto name = path.filename().string();

      return !name.empty() && name.front() == '.';
    }

    bool isSparseFile(const std::filesystem::path& path)
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

      constexpr auto posixStatBlockSize = 512ULL;

      return status.st_size > 0 && status.st_blocks >= 0 &&
             static_cast<unsigned long long>(status.st_blocks) * posixStatBlockSize
              < static_cast<unsigned long long>(status.st_size);
#else
      static_cast<void>(path);

      return false;
#endif
    }

    bool isWindowsReparsePoint(const std::filesystem::path& path)
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

    bool isLinkLikeDirectory(const std::filesystem::directory_entry& entry)
    {
      std::error_code error;

      return entry.is_symlink(error) || isWindowsReparsePoint(entry.path());
    }

    std::string directoryIdentity(const std::filesystem::path& directory)
    {
      std::error_code error;
      auto canonical = std::filesystem::weakly_canonical(directory, error);

      if (error)
        canonical = std::filesystem::absolute(directory, error).lexically_normal();

      if (error)
        return normalizedPathKey(directory);

      return normalizedPathKey(canonical);
    }

    bool hasAllowedExtension(const std::filesystem::path& path, const std::vector<std::string>& extensions)
    {
      if (extensions.empty())
        return true;

      const auto extension = normalizedExtension(path);

      return std::ranges::any_of(extensions, [&](const std::string& allowed) {
        auto normalizedAllowed = normalizedPathKey(allowed);

        if (normalizedAllowed.starts_with('.'))
          normalizedAllowed.erase(0, 1);

        return extension == normalizedAllowed;
      });
    }

    bool isIncludedDirectory(const std::filesystem::path& relativePath,
                               const std::vector<std::filesystem::path>& includedDirectories)
    {
      if (includedDirectories.empty())
        return true;

      const auto relativeKey = normalizedPathKey(relativePath);

      return std::ranges::any_of(includedDirectories, [&](const std::filesystem::path& included) {
        return normalizedPathIsSameOrInside(relativeKey, normalizedPathKey(included));
      });
    }

    bool isExcludedDirectory(const std::filesystem::path& relativePath,
                               const std::vector<std::filesystem::path>& excludedDirectories)
    {
      const auto relativeKey = normalizedPathKey(relativePath);

      return std::ranges::any_of(excludedDirectories, [&](const std::filesystem::path& excluded) {
        return normalizedPathIsSameOrInside(relativeKey, normalizedPathKey(excluded));
      });
    }

    bool globMatches(std::string_view pattern, std::string_view text)
    {
      std::size_t patternIndex = 0;
      std::size_t textIndex = 0;
      std::optional<std::size_t> starPatternIndex;
      std::size_t starTextIndex = 0;

      while (textIndex < text.size()) {
        if (patternIndex < pattern.size() &&
            (pattern[patternIndex] == '?' || pattern[patternIndex] == text[textIndex])) {
          ++patternIndex;
          ++textIndex;

          continue;
        }

        if (patternIndex < pattern.size() && pattern[patternIndex] == '*') {
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

      while (patternIndex < pattern.size() && pattern[patternIndex] == '*')
        ++patternIndex;

      return patternIndex == pattern.size();
    }

    bool passesGlobs(const std::filesystem::path& relativePath, const SearchOptions& options)
    {
      const auto text = normalizedPathKey(relativePath);
      const auto matches = std::ranges::any_of(options.includedGlobs, [&](const std::string& glob) {
        return globMatches(normalizedPathKey(glob), text);
      });

      if (!options.includedGlobs.empty() && !matches)
        return false;

      return !std::ranges::any_of(options.excludedGlobs, [&](const std::string& glob) {
        return globMatches(normalizedPathKey(glob), text);
      });
    }

    std::vector<std::filesystem::directory_entry>
    sortedDirectoryEntries(const std::filesystem::path& directory,
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
        std::error_code leftError;
        std::error_code rightError;
        const auto leftIsDirectory = left.is_directory(leftError);
        const auto rightIsDirectory = right.is_directory(rightError);

        if (!leftError && !rightError && leftIsDirectory != rightIsDirectory)
          return leftIsDirectory;

        if (!leftIsDirectory && !rightIsDirectory) {
          const auto leftSize = left.file_size(leftError);
          const auto rightSize = right.file_size(rightError);

          if (!leftError && !rightError && leftSize != rightSize)
            return leftSize < rightSize;
        }

        return normalizedPathKey(left.path()) < normalizedPathKey(right.path());
      });

      return entries;
    }

    std::filesystem::path relativeDirectory(const std::filesystem::path& directory, const std::filesystem::path& root)
    {
      std::error_code error;
      auto relative = std::filesystem::relative(directory, root, error);
      if (error || relative == ".")
        return {};

      return relative;
    }

    GitIgnoreRules initialIgnoreRules(const std::filesystem::path& root, const SearchOptions& options)
    {
      GitIgnoreRules ignoreRules;

      if (!options.respectGitignore)
        return ignoreRules;

      for (const auto& globalIgnoreFile : options.globalGitIgnoreFiles)
        ignoreRules.appendFile(globalIgnoreFile, {});

      ignoreRules.appendFile(root / ".git" / "info" / "exclude", {});

      return ignoreRules;
    }

  } // namespace

  void RecursiveFileScanner::scan(const std::filesystem::path& root,
                                  const SearchOptions& options,
                                  FileSink sink,
                                  std::stop_token stop_token,
                                  diagnostics::SearchMetrics* metrics) const
  {
    auto flags = std::filesystem::directory_options::skip_permission_denied;

    if (options.followSymlinks)
      flags |= std::filesystem::directory_options::follow_directory_symlink;

    std::unordered_set<std::string> visitedDirectories;
    std::function<bool(const std::filesystem::path&, GitIgnoreRules)> scanDirectory;
    scanDirectory = [&](const std::filesystem::path& directory, GitIgnoreRules ignoreRules) {
      const auto identity = directoryIdentity(directory);

      if (!visitedDirectories.insert(identity).second)
        return true;

      if (options.respectGitignore)
        ignoreRules.appendFile(directory / ".gitignore", relativeDirectory(directory, root));

      for (const auto& item : sortedDirectoryEntries(directory, flags)) {
        if (stop_token.stop_requested())
          return false;

        std::error_code error;
        const auto path = item.path();
        const bool hidden = isHidden(path);
        const auto relativePath = std::filesystem::relative(path, root, error);

        if (error)
          continue;

        if (item.is_directory(error)) {
          const auto linkLikeDirectory = isLinkLikeDirectory(item);

          if (linkLikeDirectory && !options.followSymlinks)
            continue;

          if (options.respectGitignore && ignoreRules.ignores(relativePath, true))
            continue;

          if ((hidden && !options.includeHidden) ||
              isExcludedDirectory(relativePath, options.excludedDirectories))
            continue;

          if (!scanDirectory(path, ignoreRules))
            return false;

          continue;
        }

        if (options.respectGitignore && ignoreRules.ignores(relativePath, false)) {
          if (metrics != nullptr)
            ++metrics->ignoredFiles;

          continue;
        }

        if (!item.is_regular_file(error))
          continue;

        if (hidden && !options.includeHidden) {
          if (metrics != nullptr)
            ++metrics->hiddenFiles;

          continue;
        }

        if (isExcludedDirectory(relativePath, options.excludedDirectories) ||
            !isIncludedDirectory(relativePath, options.includedDirectories) ||
            !hasAllowedExtension(path, options.extensions) ||
            !passesGlobs(relativePath, options))
          continue;

        const auto size = item.file_size(error);

        if (error || size > options.maximumFileSize)
          continue;

        FileEntry entry{.absolutePath = path,
                        .relativePath = relativePath,
                        .size = size,
                        .modifiedAt = item.last_write_time(error),
                        .hidden = hidden,
                        .binary = false,
                        .symlink = item.is_symlink(error),
                        .sparse = isSparseFile(path),
                        .searchRoot = root};

        if (!error && !sink(std::move(entry)))
          return false;
      }

      return true;
    };

    scanDirectory(root, initialIgnoreRules(root, options));
  }

} // namespace uburu::filesystem
