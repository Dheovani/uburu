#include "core/filesystem/recursive-file-scanner.hpp"

#include <algorithm>
#include <system_error>

namespace uburu::filesystem
{
  namespace
  {

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
      auto extension = path.extension().string();
      if (extension.starts_with('.'))
        extension.erase(0, 1);
      return std::ranges::find(extensions, extension) != extensions.end();
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
      if (iterator->is_directory(error)) {
        if (hidden && !options.include_hidden)
          iterator.disable_recursion_pending();
        iterator.increment(error);
        continue;
      }

      if (!iterator->is_regular_file(error) || (hidden && !options.include_hidden) ||
          !has_allowed_extension(path, options.extensions)) {
        iterator.increment(error);
        continue;
      }

      const auto size = iterator->file_size(error);
      if (!error && size <= options.maximum_file_size) {
        FileEntry entry{.absolute_path = path,
                        .relative_path = std::filesystem::relative(path, root, error),
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
