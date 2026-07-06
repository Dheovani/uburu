#include "core/filesystem/path-normalization.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace
{

  constexpr std::size_t maximumPathBytes = 512;
  constexpr unsigned char asciiRange = 95;
  constexpr unsigned char asciiFirstPrintable = 32;
  constexpr char pathSeparatorReplacement = '/';
  constexpr char nulReplacement = '_';

  std::string pathText(const std::uint8_t* data, std::size_t size)
  {
    std::string output;
    output.reserve(size);

    for (std::size_t index = 0; index < size; ++index) {
      auto character = static_cast<char>((data[index] % asciiRange) + asciiFirstPrintable);

      if (character == '\0')
        character = nulReplacement;

      if (character == '\\')
        character = pathSeparatorReplacement;

      output.push_back(character);
    }

    return output;
  }

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
  const auto pathSize = std::min(maximumPathBytes, size);
  auto text = pathText(data, pathSize);
  const auto path = std::filesystem::path(text);
  const auto base = std::filesystem::path("base");

  static_cast<void>(uburu::filesystem::normalizePathSeparators(std::move(text)));
  static_cast<void>(uburu::filesystem::normalizedPathKey(path));
  static_cast<void>(uburu::filesystem::normalizedAbsolutePath(path));
  static_cast<void>(uburu::filesystem::normalizedPathIsSameOrInside(uburu::filesystem::normalizedPathKey(path),
                                                                    uburu::filesystem::normalizedPathKey(base)));

  try {
    static_cast<void>(uburu::filesystem::normalizedRelativePath(path));
  } catch (const std::invalid_argument&) {}

  return 0;
}
