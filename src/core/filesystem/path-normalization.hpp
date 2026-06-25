#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace uburu::filesystem
{

  [[nodiscard]] std::string normalizePathSeparators(std::string text);

  [[nodiscard]] std::string normalizedRelativePath(const std::filesystem::path& path);

  [[nodiscard]] std::string normalizedAbsolutePath(const std::filesystem::path& path);

  [[nodiscard]] std::string normalizedPathKey(const std::filesystem::path& path);

  [[nodiscard]] bool normalizedPathIsSameOrInside(std::string_view path, std::string_view base);

} // namespace uburu::filesystem
