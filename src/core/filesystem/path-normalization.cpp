#include "core/filesystem/path-normalization.hpp"

#include <algorithm>
#include <stdexcept>

namespace uburu::filesystem
{
  namespace
  {

    constexpr char windowsPathSeparator = '\\';
    constexpr char genericPathSeparator = '/';
    constexpr char asciiUppercaseFirst = 'A';
    constexpr char asciiUppercaseLast = 'Z';
    constexpr char asciiLowercaseOffset = 'a' - 'A';

#ifdef _WIN32
    constexpr std::size_t windowsNetworkPrefixSize = 2;

    bool startsWithWindowsNetworkPrefix(std::string_view text)
    {
      if (text.size() < windowsNetworkPrefixSize)
        return false;

      const auto first = text[0];
      const auto second = text[1];

      return (first == windowsPathSeparator || first == genericPathSeparator) &&
             (second == windowsPathSeparator || second == genericPathSeparator);
    }
#endif

    char normalizedKeyCharacter(char value)
    {
#ifdef _WIN32
      if (value >= asciiUppercaseFirst && value <= asciiUppercaseLast)
        return static_cast<char>(value + asciiLowercaseOffset);
#endif
      return value;
    }

    std::string normalizedKeyText(std::string text)
    {
      for (auto& character : text)
        character = normalizedKeyCharacter(character);

      return text;
    }

    std::string genericNormalizedPath(const std::filesystem::path& path)
    {
      const auto original = path.string();
      auto normalized = normalizePathSeparators(path.lexically_normal().generic_string());

#ifdef _WIN32
      if (startsWithWindowsNetworkPrefix(original) && !startsWithWindowsNetworkPrefix(normalized) &&
          normalized.starts_with(genericPathSeparator))
        normalized.insert(normalized.begin(), genericPathSeparator);
#endif
      return normalized;
    }

  } // namespace

  std::string normalizePathSeparators(std::string text)
  {
    std::ranges::replace(text, windowsPathSeparator, genericPathSeparator);

    return text;
  }

  std::string normalizedRelativePath(const std::filesystem::path& path)
  {
    const auto normalized = path.lexically_normal();

    if (normalized.is_absolute())
      throw std::invalid_argument("Expected a relative path, got an absolute path.");

    if (normalized == ".")
      return {};

    return normalizePathSeparators(normalized.generic_string());
  }

  std::string normalizedAbsolutePath(const std::filesystem::path& path)
  {
    return genericNormalizedPath(std::filesystem::absolute(path));
  }

  std::string normalizedPathKey(const std::filesystem::path& path)
  {
    return normalizedKeyText(genericNormalizedPath(path));
  }

  bool normalizedPathIsSameOrInside(std::string_view path, std::string_view base)
  {
    if (base.empty())
      return true;

    if (path == base)
      return true;

    return path.size() > base.size() && path.starts_with(base) && path[base.size()] == genericPathSeparator;
  }

} // namespace uburu::filesystem
