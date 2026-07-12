#pragma once

#include "core/contracts/contract-version.hpp"
#include "core/symbols/symbol-types.hpp"

#include <optional>
#include <stop_token>
#include <string_view>

namespace uburu::symbols
{

  /**
   * Detects source language metadata without binding the index to a parser backend.
   */
  class LanguageParser
  {
  public:
    virtual ~LanguageParser() = default;

    [[nodiscard]]
    virtual std::optional<LanguageInfo> detect(
      const std::filesystem::path& path,
      std::string_view contentSample = {}) const = 0;
  };

  /**
   * Extracts navigable symbols for one or more language ids.
   */
  class SymbolParser
  {
  public:
    virtual ~SymbolParser() = default;

    [[nodiscard]]
    virtual contracts::ContractVersion contract() const
    {
      return contracts::symbolParserContract;
    }

    [[nodiscard]]
    virtual bool supportsLanguage(std::string_view languageId) const = 0;

    [[nodiscard]]
    virtual SymbolParseResult parse(
      const SymbolParseRequest& request,
      std::stop_token stopToken = {}) const = 0;
  };

} // namespace uburu::symbols
