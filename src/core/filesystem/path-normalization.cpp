#include "core/filesystem/path-normalization.hpp"

#include <algorithm>
#include <stdexcept>

namespace uburu::filesystem
{
  namespace
  {

    constexpr std::size_t windows_network_prefix_size = 2;
    constexpr char windows_path_separator = '\\';
    constexpr char generic_path_separator = '/';
    constexpr char ascii_uppercase_first = 'A';
    constexpr char ascii_uppercase_last = 'Z';
    constexpr char ascii_lowercase_offset = 'a' - 'A';

    bool starts_with_windows_network_prefix(std::string_view text)
    {
      if (text.size() < windows_network_prefix_size)
        return false;

      const auto first = text[0];
      const auto second = text[1];

      return (first == windows_path_separator || first == generic_path_separator) &&
             (second == windows_path_separator || second == generic_path_separator);
    }

    char normalized_key_character(char value)
    {
#ifdef _WIN32
      if (value >= ascii_uppercase_first && value <= ascii_uppercase_last)
        return static_cast<char>(value + ascii_lowercase_offset);
#endif
      return value;
    }

    std::string normalized_key_text(std::string text)
    {
      for (auto& character : text)
        character = normalized_key_character(character);

      return text;
    }

    std::string generic_normalized_path(const std::filesystem::path& path)
    {
      const auto original = path.string();
      auto normalized = normalize_path_separators(path.lexically_normal().generic_string());

#ifdef _WIN32
      if (starts_with_windows_network_prefix(original) &&
          !starts_with_windows_network_prefix(normalized) &&
          normalized.starts_with(generic_path_separator))
        normalized.insert(normalized.begin(), generic_path_separator);
#endif
      return normalized;
    }

  } // namespace

  std::string normalize_path_separators(std::string text)
  {
    std::ranges::replace(text, windows_path_separator, generic_path_separator);

    return text;
  }

  std::string normalized_relative_path(const std::filesystem::path& path)
  {
    const auto normalized = path.lexically_normal();

    if (normalized.is_absolute())
      throw std::invalid_argument("Expected a relative path, got an absolute path.");

    if (normalized == ".")
      return {};

    return normalize_path_separators(normalized.generic_string());
  }

  std::string normalized_absolute_path(const std::filesystem::path& path)
  {
    return generic_normalized_path(std::filesystem::absolute(path));
  }

  std::string normalized_path_key(const std::filesystem::path& path)
  {
    return normalized_key_text(generic_normalized_path(path));
  }

  bool normalized_path_is_same_or_inside(std::string_view path, std::string_view base)
  {
    if (base.empty())
      return true;

    if (path == base)
      return true;

    return path.size() > base.size()
        && path.starts_with(base)
        && path[base.size()] == generic_path_separator;
  }

} // namespace uburu::filesystem
