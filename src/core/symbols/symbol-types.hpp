#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace uburu::symbols
{

  enum class SymbolKind
  {
    unknown,
    module,
    namespaceSymbol,
    classSymbol,
    structSymbol,
    interfaceSymbol,
    enumSymbol,
    function,
    method,
    constructor,
    property,
    field,
    variable,
    constant
  };

  struct SourcePosition
  {
    std::size_t line{0};
    std::size_t column{0};
  };

  struct SourceRange
  {
    SourcePosition begin;
    SourcePosition end;
  };

  struct LanguageInfo
  {
    std::string id;
    std::string displayName;
    std::vector<std::string> extensions;
  };

  struct SymbolInfo
  {
    std::string name;
    std::string qualifiedName;
    SymbolKind kind{SymbolKind::unknown};
    SourceRange range;
    SourceRange selectionRange;
    std::filesystem::path path;
    std::string languageId;
  };

  struct SymbolDiagnostic
  {
    std::string message;
    SourceRange range;
  };

  struct SymbolParseRequest
  {
    std::filesystem::path path;
    std::string languageId;
    std::string text;
  };

  struct SymbolParseResult
  {
    std::vector<SymbolInfo> symbols;
    std::vector<SymbolDiagnostic> diagnostics;
    bool partial{false};
    bool cancelled{false};
  };

} // namespace uburu::symbols
