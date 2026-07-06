#include "core/filesystem/git-ignore-rules.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

namespace
{

  constexpr std::size_t maximumPathBytes = 128;
  constexpr std::size_t maximumInputBytes = 4096;
  constexpr unsigned char asciiRange = 95;
  constexpr unsigned char asciiFirstPrintable = 32;
  constexpr char pathSeparatorReplacement = '/';
  constexpr char nulReplacement = '_';

  std::filesystem::path fuzzIgnorePath()
  {
    return std::filesystem::temp_directory_path() / "uburu-git-ignore-fuzzer.gitignore";
  }

  std::string printablePath(std::string_view input)
  {
    std::string output;
    output.reserve(input.size());

    for (const auto value : input) {
      auto character = static_cast<char>((static_cast<unsigned char>(value) % asciiRange) + asciiFirstPrintable);

      if (character == '\0')
        character = nulReplacement;

      if (character == '\\')
        character = pathSeparatorReplacement;

      output.push_back(character);
    }

    if (output.empty())
      output = "file.txt";

    return output;
  }

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
  if (size == 0)
    return 0;

  const auto contentSize = std::min(maximumInputBytes, size);
  const std::string_view content(reinterpret_cast<const char*>(data), contentSize);
  const auto pathSize = std::min(maximumPathBytes, size);
  const std::string_view pathBytes(reinterpret_cast<const char*>(data), pathSize);
  const auto relativePath = std::filesystem::path(printablePath(pathBytes));
  const auto rules = uburu::filesystem::parseGitIgnore(content, "root");

  for (const auto& rule : rules)
    static_cast<void>(rule.pattern.size());

  const auto ignoreFile = fuzzIgnorePath();
  {
    std::ofstream file(ignoreFile, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(contentSize));
  }

  uburu::filesystem::GitIgnoreRules aggregate;
  aggregate.appendFile(ignoreFile, "root");
  static_cast<void>(aggregate.empty());
  static_cast<void>(aggregate.ignores(relativePath, false));
  static_cast<void>(aggregate.ignores(relativePath, true));
  std::filesystem::remove(ignoreFile);

  return 0;
}
