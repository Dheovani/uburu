#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace uburu::filesystem
{

  /**
   * Converts platform separators into the generic slash form used by internal keys.
   */
  [[nodiscard]]
  std::string normalizePathSeparators(std::string text);

  [[nodiscard]]
  std::string normalizedRelativePath(const std::filesystem::path& path);

  [[nodiscard]]
  std::string normalizedAbsolutePath(const std::filesystem::path& path);

  /**
   * Produces a stable comparison key for paths stored in catalogs and filters.
   */
  [[nodiscard]]
  std::string normalizedPathKey(const std::filesystem::path& path);

  /**
   * Checks containment using normalized path segment boundaries, not raw string prefixes.
   */
  [[nodiscard]]
  bool normalizedPathIsSameOrInside(std::string_view path, std::string_view base);

} // namespace uburu::filesystem
