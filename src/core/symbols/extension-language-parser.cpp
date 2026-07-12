#include "core/symbols/extension-language-parser.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace uburu::symbols
{
  namespace
  {

    [[nodiscard]]
    std::string normalizedExtension(std::string extension)
    {
      if (!extension.empty() && extension.front() == '.')
        extension.erase(extension.begin());

      std::ranges::transform(extension, extension.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
      });

      return extension;
    }

  } // namespace

  ExtensionLanguageParser::ExtensionLanguageParser(std::vector<LanguageInfo> languages)
  {
    for (auto& language : languages) {
      for (auto extension : language.extensions)
        languagesByExtension.emplace(normalizedExtension(std::move(extension)), language);
    }
  }

  std::optional<LanguageInfo> ExtensionLanguageParser::detect(
    const std::filesystem::path& path,
    std::string_view) const
  {
    const auto extension = normalizedExtension(path.extension().generic_string());

    if (extension.empty())
      return std::nullopt;

    const auto iterator = languagesByExtension.find(extension);

    if (iterator == languagesByExtension.end())
      return std::nullopt;

    return iterator->second;
  }

} // namespace uburu::symbols
