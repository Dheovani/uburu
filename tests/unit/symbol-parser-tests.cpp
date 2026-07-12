#include "core/symbols/extension-language-parser.hpp"
#include "core/symbols/symbol-parser-registry.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

  class FixedSymbolParser final : public uburu::symbols::SymbolParser
  {
  public:
    explicit FixedSymbolParser(std::string languageId)
      : languageId(std::move(languageId))
    {}

    [[nodiscard]]
    bool supportsLanguage(std::string_view requestedLanguageId) const override
    {
      return requestedLanguageId == languageId;
    }

    [[nodiscard]]
    uburu::symbols::SymbolParseResult parse(
      const uburu::symbols::SymbolParseRequest& request,
      std::stop_token stopToken = {}) const override
    {
      if (stopToken.stop_requested()) {
        uburu::symbols::SymbolParseResult result;
        result.cancelled = true;

        return result;
      }

      uburu::symbols::SymbolInfo symbol;
      symbol.name = "main";
      symbol.qualifiedName = request.languageId + "::main";
      symbol.kind = uburu::symbols::SymbolKind::function;
      symbol.path = request.path;
      symbol.languageId = request.languageId;

      uburu::symbols::SymbolParseResult result;
      result.symbols.push_back(std::move(symbol));

      return result;
    }

  private:
    std::string languageId;
  };

  [[nodiscard]]
  uburu::symbols::LanguageInfo cppLanguage()
  {
    uburu::symbols::LanguageInfo language;
    language.id = "cpp";
    language.displayName = "C++";
    language.extensions = {"cpp", "hpp"};

    return language;
  }

} // namespace

TEST_CASE("extension language parser detects language by case-insensitive extension")
{
  uburu::symbols::ExtensionLanguageParser parser({cppLanguage()});

  const auto language = parser.detect("C:/repo/source.CPP");

  REQUIRE(language.has_value());
  CHECK(language->id == "cpp");
  CHECK(language->displayName == "C++");
}

TEST_CASE("symbol parser registry chooses parser for detected language")
{
  uburu::symbols::SymbolParserRegistry registry;
  registry.registerLanguageParser(std::make_shared<uburu::symbols::ExtensionLanguageParser>(
    std::vector<uburu::symbols::LanguageInfo>{cppLanguage()}));
  registry.registerSymbolParser(std::make_shared<FixedSymbolParser>("cpp"));

  const auto language = registry.detectLanguage("C:/repo/source.cpp");

  REQUIRE(language.has_value());

  uburu::symbols::SymbolParseRequest request;
  request.path = "C:/repo/source.cpp";
  request.languageId = language->id;
  request.text = "int main() {}";

  const auto result = registry.parse(request);

  REQUIRE(result.symbols.size() == 1);
  CHECK(result.symbols.front().name == "main");
  CHECK(result.symbols.front().languageId == "cpp");

  const auto parser = registry.parserForLanguage("cpp");

  REQUIRE(parser != nullptr);
  CHECK(parser->contract().name == "uburu.symbols.parser");
}

TEST_CASE("symbol parser registry reports cancellation before selecting parser")
{
  uburu::symbols::SymbolParserRegistry registry;
  registry.registerSymbolParser(std::make_shared<FixedSymbolParser>("cpp"));

  uburu::symbols::SymbolParseRequest request;
  request.path = "C:/repo/source.cpp";
  request.languageId = "cpp";
  request.text = "int main() {}";

  std::stop_source stopSource;
  stopSource.request_stop();

  const auto result = registry.parse(request, stopSource.get_token());

  CHECK(result.cancelled);
  CHECK(result.symbols.empty());
}
