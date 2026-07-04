#pragma once

#include "helpers/temporary-paths.hpp"

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace uburu::tests::fixtures
{

  inline constexpr std::string_view literalNeedle = "needle";
  inline constexpr std::string_view rootGitIgnoreContent = "*.log\n!important.log\nbuild/\n";
  inline constexpr std::string_view nestedGitIgnoreContent = "*.generated.cpp\n";

  inline std::string portuguesePrecomposedText()
  {
    return "a gera\xC3\xA7\xC3\xA3o e a corrup\xC3\xA7\xC3\xA3o da mat\xC3\xA9ria";
  }

  inline std::string portugueseDecomposedText()
  {
    return "a gerac\xCC\xA7"
           "a\xCC\x83"
           "o e a corrupc\xCC\xA7"
           "a\xCC\x83"
           "o da mate\xCC\x81"
           "ria";
  }

  [[nodiscard]] inline std::vector<unsigned char> utf8BomMixedLineEndingBytes()
  {
    return {0xEFU, 0xBBU, 0xBFU, 'o', 'n', 'e', '\n', 't', 'w', 'o', '\r',
            '\n',  't',   'h',   'r', 'e', 'e', '\r', 'f', 'o', 'u', 'r'};
  }

  [[nodiscard]] inline std::vector<unsigned char> utf16LittleEndianPortugueseBytes()
  {
    return {0xFFU, 0xFEU, 'a', 0x00U, 0xE7U, 0x00U, 0xE3U, 0x00U, 'o', 0x00U};
  }

  [[nodiscard]] inline std::vector<unsigned char> utf16BigEndianPortugueseBytes()
  {
    return {0xFEU, 0xFFU, 0x00U, 'a', 0x00U, 0xE7U, 0x00U, 0xE3U, 0x00U, 'o'};
  }

  [[nodiscard]] inline std::vector<unsigned char> latin1PortugueseBytes()
  {
    return {'a', 0xE7U, 0xE3U, 'o'};
  }

  [[nodiscard]] inline std::vector<unsigned char> binaryTextLikeBytes()
  {
    return {'t', 'e', 'x', 't', 0x00U, 'm', 'o', 'r', 'e'};
  }

  inline void writeRootGitIgnoreFixture(const std::filesystem::path& root)
  {
    writeFile(root / ".gitignore", rootGitIgnoreContent);
    writeFile(root / "debug.log", "ignored\n");
    writeFile(root / "important.log", "kept\n");
    writeFile(root / "build" / "output.txt", "ignored\n");
    writeFile(root / "src" / "main.cpp", "kept\n");
  }

  inline void writeNestedGitIgnoreFixture(const std::filesystem::path& root)
  {
    writeFile(root / "src" / ".gitignore", nestedGitIgnoreContent);
    writeFile(root / "src" / "main.cpp", "kept\n");
    writeFile(root / "src" / "main.generated.cpp", "ignored\n");
    writeFile(root / "tests" / "main.generated.cpp", "kept\n");
  }

  inline void writeBasicGitWorkingTreeFixture(const std::filesystem::path& repositoryRoot)
  {
    writeFile(repositoryRoot / "tracked.txt", "tracked\n");
    writeFile(repositoryRoot / "modify-me.txt", "modify me\n");
    writeFile(repositoryRoot / "delete-me.txt", "delete me\n");
    writeFile(repositoryRoot / ".gitignore", "*.ignored\n");
  }

} // namespace uburu::tests::fixtures
