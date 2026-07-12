#pragma once

#include "core/symbols/symbol-parser.hpp"

#include <memory>
#include <vector>

namespace uburu::symbols
{

  class SymbolParserRegistry final
  {
  public:
    void registerLanguageParser(std::shared_ptr<const LanguageParser> parser);
    void registerSymbolParser(std::shared_ptr<const SymbolParser> parser);

    [[nodiscard]]
    std::optional<LanguageInfo> detectLanguage(
      const std::filesystem::path& path,
      std::string_view contentSample = {}) const;

    [[nodiscard]]
    std::shared_ptr<const SymbolParser> parserForLanguage(std::string_view languageId) const;

    [[nodiscard]]
    SymbolParseResult parse(
      const SymbolParseRequest& request,
      std::stop_token stopToken = {}) const;

  private:
    std::vector<std::shared_ptr<const LanguageParser>> languageParsers;
    std::vector<std::shared_ptr<const SymbolParser>> symbolParsers;
  };

} // namespace uburu::symbols
