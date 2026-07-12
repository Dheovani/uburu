#include "cli-options.hpp"

#include <algorithm>
#include <charconv>
#include <sstream>
#include <utility>

namespace uburu::cli
{
  namespace
  {

    constexpr std::size_t rootArgumentIndex = 1;
    constexpr std::size_t expressionArgumentIndex = 2;
    constexpr std::size_t indexRootArgumentIndex = 1;
    constexpr std::size_t firstIndexOptionArgumentIndex = 2;
    constexpr std::size_t firstOptionArgumentIndex = 3;
    constexpr std::size_t minimumSearchArgumentCount = 3;
    constexpr std::size_t minimumIndexArgumentCount = 2;
    constexpr std::uintmax_t bytesPerMib = 1024U * 1024U;

    [[nodiscard]]
    std::string toString(std::string_view value)
    {
      return std::string(value);
    }

    [[nodiscard]]
    bool isHelpArgument(std::string_view value)
    {
      return value == "--help" || value == "-h";
    }

    [[nodiscard]]
    std::vector<std::string> splitCommaSeparated(std::string_view value)
    {
      std::vector<std::string> values;

      while (!value.empty()) {
        const auto comma = value.find(',');
        const auto current = value.substr(0, comma);

        if (!current.empty())
          values.push_back(toString(current));

        if (comma == std::string_view::npos)
          break;

        value.remove_prefix(comma + 1);
      }

      return values;
    }

    [[nodiscard]]
    std::optional<std::uintmax_t> parseMib(std::string_view value)
    {
      std::uintmax_t parsed = 0;
      const auto* first = value.data();
      const auto* last = value.data() + value.size();
      const auto [position, error] = std::from_chars(first, last, parsed);

      if (error != std::errc{} || position != last)
        return std::nullopt;

      return parsed * bytesPerMib;
    }

    [[nodiscard]]
    bool parseOutputFormat(CliOptions& options, std::string_view format)
    {
      if (format == "human") {
        options.outputFormat = CliOutputFormat::human;

        return true;
      }

      if (format == "jsonl") {
        options.outputFormat = CliOutputFormat::jsonLines;

        return true;
      }

      return false;
    }

    [[nodiscard]]
    bool parseSearchStrategy(CliOptions& options, std::string_view strategy)
    {
      if (strategy == "direct") {
        options.searchStrategy = CliSearchStrategy::direct;

        return true;
      }

      if (strategy == "indexed") {
        options.searchStrategy = CliSearchStrategy::indexed;

        return true;
      }

      if (strategy == "hybrid") {
        options.searchStrategy = CliSearchStrategy::hybrid;

        return true;
      }

      return false;
    }

    [[nodiscard]]
    bool parseCommonOption(
      CliOptions& options,
      const std::vector<std::string_view>& arguments,
      std::size_t& index,
      std::string& error)
    {
      const auto argument = arguments[index];

      if (argument == "--format") {
        if (index + 1 >= arguments.size()) {
          error = "--format requires human or jsonl";

          return false;
        }

        if (!parseOutputFormat(options, arguments[++index])) {
          error = "--format requires human or jsonl";

          return false;
        }

        return true;
      }

      if (argument == "--database") {
        if (index + 1 >= arguments.size()) {
          error = "--database requires a path";

          return false;
        }

        options.databasePath = std::filesystem::path(toString(arguments[++index]));

        return true;
      }

      return false;
    }

    [[nodiscard]]
    CliParseResult usageError(std::string error)
    {
      CliParseResult result;
      result.error = std::move(error);

      return result;
    }

    [[nodiscard]]
    CliParseResult parseSearchOptions(std::vector<std::string_view> arguments)
    {
      if (arguments.size() < minimumSearchArgumentCount)
        return usageError("search requires <root> and <expression>");

      CliOptions options;
      options.command = CliCommand::search;
      options.query.root = std::filesystem::path(toString(arguments[rootArgumentIndex]));
      options.query.expression = toString(arguments[expressionArgumentIndex]);

      SearchRoot root;
      root.path = options.query.root;
      options.query.scope.roots.push_back(std::move(root));

      options.query.options.target = SearchTarget::contentAndFileName;

      for (std::size_t index = firstOptionArgumentIndex; index < arguments.size(); ++index) {
        const auto argument = arguments[index];
        std::string commonError;

        if (parseCommonOption(options, arguments, index, commonError)) {
          continue;
        }

        if (!commonError.empty())
          return usageError(std::move(commonError));

        if (argument == "--strategy") {
          if (index + 1 >= arguments.size())
            return usageError("--strategy requires direct, indexed, or hybrid");

          if (!parseSearchStrategy(options, arguments[++index]))
            return usageError("--strategy requires direct, indexed, or hybrid");
        } else if (argument == "--regex") {
          options.query.options.mode = SearchMode::regex;
        } else if (argument == "--case-sensitive") {
          options.query.options.caseSensitive = true;
        } else if (argument == "--whole-word") {
          options.query.options.wholeWord = true;
        } else if (argument == "--no-gitignore") {
          options.query.options.respectGitignore = false;
        } else if (argument == "--hidden") {
          options.query.options.includeHidden = true;
        } else if (argument == "--binary") {
          options.query.options.includeBinary = true;
        } else if (argument == "--no-subdirectories") {
          options.query.options.includeSubdirectories = false;
        } else if (argument == "--types") {
          if (index + 1 >= arguments.size())
            return usageError("--types requires a comma-separated extension list");

          options.query.options.extensions = splitCommaSeparated(arguments[++index]);
        } else if (argument == "--max-size-mib") {
          if (index + 1 >= arguments.size())
            return usageError("--max-size-mib requires a numeric value");

          auto maximumFileSize = parseMib(arguments[++index]);

          if (!maximumFileSize)
            return usageError("--max-size-mib requires a numeric value");

          options.query.options.maximumFileSize = *maximumFileSize;
        } else if (isHelpArgument(argument)) {
          options.showHelp = true;
        } else {
          return usageError("unknown option: " + toString(argument));
        }
      }

      CliParseResult result;
      result.options = std::move(options);

      return result;
    }

    [[nodiscard]]
    CliParseResult parseIndexCommandOptions(std::vector<std::string_view> arguments, CliCommand command)
    {
      if (arguments.size() < minimumIndexArgumentCount)
        return usageError("index command requires <root>");

      CliOptions options;
      options.command = command;
      options.query.root = std::filesystem::path(toString(arguments[indexRootArgumentIndex]));

      for (std::size_t index = firstIndexOptionArgumentIndex; index < arguments.size(); ++index) {
        const auto argument = arguments[index];
        std::string commonError;

        if (parseCommonOption(options, arguments, index, commonError)) {
          continue;
        }

        if (!commonError.empty())
          return usageError(std::move(commonError));

        if (argument == "--types") {
          if (index + 1 >= arguments.size())
            return usageError("--types requires a comma-separated extension list");

          options.query.options.extensions = splitCommaSeparated(arguments[++index]);
        } else if (argument == "--no-gitignore") {
          options.query.options.respectGitignore = false;
        } else if (argument == "--hidden") {
          options.query.options.includeHidden = true;
        } else if (argument == "--binary") {
          options.query.options.includeBinary = true;
        } else if (argument == "--no-subdirectories") {
          options.query.options.includeSubdirectories = false;
        } else if (isHelpArgument(argument)) {
          options.showHelp = true;
        } else {
          return usageError("unknown option: " + toString(argument));
        }
      }

      CliParseResult result;
      result.options = std::move(options);

      return result;
    }

  } // namespace

  CliParseResult parseCliOptions(std::vector<std::string_view> arguments)
  {
    if (arguments.empty() || isHelpArgument(arguments.front())) {
      CliOptions options;
      options.showHelp = true;

      CliParseResult result;
      result.options = std::move(options);

      return result;
    }

    const auto command = arguments.front();

    if (command == "search")
      return parseSearchOptions(std::move(arguments));

    if (command == "index-status")
      return parseIndexCommandOptions(std::move(arguments), CliCommand::indexStatus);

    if (command == "index-rebuild")
      return parseIndexCommandOptions(std::move(arguments), CliCommand::indexRebuild);

    if (command == "help") {
      CliOptions options;
      options.showHelp = true;

      CliParseResult result;
      result.options = std::move(options);

      return result;
    }

    return usageError("unknown command: " + toString(command));
  }

  std::string cliHelpText()
  {
    std::ostringstream output;

    output << "Uburu CLI\n\n";
    output << "Usage:\n";
    output << "  uburu search <root> <expression> [options]\n\n";
    output << "  uburu index-status <root> [options]\n";
    output << "  uburu index-rebuild <root> [options]\n\n";
    output << "Options:\n";
    output << "  --format human|jsonl       Output format. Defaults to human.\n";
    output << "  --strategy direct|indexed|hybrid\n";
    output << "                              Search strategy. Defaults to direct.\n";
    output << "  --database PATH            Override the CLI index database path.\n";
    output << "  --types txt,cpp,md         Restrict file extensions.\n";
    output << "  --max-size-mib N           Maximum file size in MiB.\n";
    output << "  --regex                    Treat expression as PCRE2 regex.\n";
    output << "  --case-sensitive           Enable case-sensitive matching.\n";
    output << "  --whole-word               Match only whole words.\n";
    output << "  --no-gitignore             Do not respect .gitignore.\n";
    output << "  --hidden                   Include hidden files.\n";
    output << "  --binary                   Include binary files.\n";
    output << "  --no-subdirectories        Search only the selected root.\n";
    output << "  --help                     Show this help text.\n";

    return output.str();
  }

} // namespace uburu::cli
