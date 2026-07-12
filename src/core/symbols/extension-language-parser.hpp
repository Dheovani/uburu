#pragma once

#include "core/symbols/symbol-parser.hpp"

#include <unordered_map>

namespace uburu::symbols
{

  class ExtensionLanguageParser final : public LanguageParser
  {
  public:
    explicit ExtensionLanguageParser(std::vector<LanguageInfo> languages);

    [[nodiscard]]
    std::optional<LanguageInfo> detect(
      const std::filesystem::path& path,
      std::string_view contentSample = {}) const override;

  private:
    std::unordered_map<std::string, LanguageInfo> languagesByExtension;
  };

} // namespace uburu::symbols
