#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace uburu::filesystem
{

  [[nodiscard]] std::string normalize_path_separators(std::string text);

  [[nodiscard]] std::string normalized_relative_path(const std::filesystem::path& path);

  [[nodiscard]] std::string normalized_absolute_path(const std::filesystem::path& path);

  [[nodiscard]] std::string normalized_path_key(const std::filesystem::path& path);

  [[nodiscard]] bool normalized_path_is_same_or_inside(std::string_view path, std::string_view base);

} // namespace uburu::filesystem
