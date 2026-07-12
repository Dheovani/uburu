#include "core/symbols/symbol-parser-registry.hpp"

#include <utility>

namespace uburu::symbols
{

  void SymbolParserRegistry::registerLanguageParser(std::shared_ptr<const LanguageParser> parser)
  {
    if (!parser)
      return;

    languageParsers.push_back(std::move(parser));
  }

  void SymbolParserRegistry::registerSymbolParser(std::shared_ptr<const SymbolParser> parser)
  {
    if (!parser)
      return;

    symbolParsers.push_back(std::move(parser));
  }

  std::optional<LanguageInfo> SymbolParserRegistry::detectLanguage(
    const std::filesystem::path& path,
    std::string_view contentSample) const
  {
    for (const auto& parser : languageParsers) {
      auto language = parser->detect(path, contentSample);

      if (language)
        return language;
    }

    return std::nullopt;
  }

  std::shared_ptr<const SymbolParser> SymbolParserRegistry::parserForLanguage(std::string_view languageId) const
  {
    for (const auto& parser : symbolParsers) {
      if (parser->supportsLanguage(languageId))
        return parser;
    }

    return nullptr;
  }

  SymbolParseResult SymbolParserRegistry::parse(
    const SymbolParseRequest& request,
    std::stop_token stopToken) const
  {
    if (stopToken.stop_requested()) {
      SymbolParseResult result;
      result.cancelled = true;

      return result;
    }

    const auto parser = parserForLanguage(request.languageId);

    if (!parser)
      return SymbolParseResult{};

    return parser->parse(request, stopToken);
  }

} // namespace uburu::symbols
