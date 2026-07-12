#pragma once

#include "shared/types/domain-types.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace uburu::cli
{

  enum class CliCommand
  {
    help,
    search,
    indexStatus,
    indexRebuild
  };

  enum class CliSearchStrategy
  {
    direct,
    indexed,
    hybrid
  };

  enum class CliOutputFormat
  {
    human,
    jsonLines
  };

  enum class CliExitCode
  {
    ok = 0,
    noMatches = 1,
    usageError = 2,
    searchFailed = 3,
    cancelled = 4
  };

  struct CliOptions
  {
    CliCommand command{CliCommand::help};
    CliOutputFormat outputFormat{CliOutputFormat::human};
    CliSearchStrategy searchStrategy{CliSearchStrategy::direct};
    SearchQuery query;
    std::optional<std::filesystem::path> databasePath;
    bool showHelp{false};
  };

  struct CliParseResult
  {
    std::optional<CliOptions> options;
    std::string error;
  };

  /**
   * Parses command-line arguments into a UI-independent search request.
   */
  [[nodiscard]]
  CliParseResult parseCliOptions(std::vector<std::string_view> arguments);

  /**
   * Returns the stable command-line help text.
   */
  [[nodiscard]]
  std::string cliHelpText();

} // namespace uburu::cli
