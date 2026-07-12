#include "cli-options.hpp"
#include "cli-output.hpp"

#include "app/services/search-service.hpp"
#include "core/filesystem/recursive-file-scanner.hpp"
#include "core/search/direct-search-engine.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

  [[nodiscard]]
  std::vector<std::string_view> argumentsWithoutExecutable(int argc, char** argv)
  {
    std::vector<std::string_view> arguments;

    for (int index = 1; index < argc; ++index)
      arguments.emplace_back(argv[index]);

    return arguments;
  }

  [[nodiscard]]
  int toProcessExitCode(uburu::cli::CliExitCode code)
  {
    return static_cast<int>(code);
  }

  [[nodiscard]]
  uburu::cli::CliExitCode exitCodeForSummary(const uburu::search::SearchSummary& summary)
  {
    if (summary.cancelled)
      return uburu::cli::CliExitCode::cancelled;

    if (!summary.errors.empty())
      return uburu::cli::CliExitCode::searchFailed;

    if (summary.matches == 0)
      return uburu::cli::CliExitCode::noMatches;

    return uburu::cli::CliExitCode::ok;
  }

  [[nodiscard]]
  std::shared_ptr<const uburu::app::SearchService> makeSearchService()
  {
    auto scanner = std::make_shared<uburu::filesystem::RecursiveFileScanner>();
    auto engine = std::make_shared<uburu::search::DirectSearchEngine>(std::move(scanner));

    return std::make_shared<uburu::app::DefaultSearchService>(std::move(engine));
  }

  [[nodiscard]]
  uburu::cli::CliExitCode runSearch(const uburu::cli::CliOptions& options)
  {
    const auto service = makeSearchService();
    auto summary = service->search(options.query, [&](uburu::SearchResult result) {
      uburu::cli::writeSearchResult(std::cout, result, options.outputFormat);

      return true;
    });

    uburu::cli::writeSearchSummary(std::cout, summary, options.outputFormat);

    return exitCodeForSummary(summary);
  }

} // namespace

int main(int argc, char** argv)
{
  auto parseResult = uburu::cli::parseCliOptions(argumentsWithoutExecutable(argc, argv));

  if (!parseResult.options) {
    std::cerr << parseResult.error << "\n\n" << uburu::cli::cliHelpText();

    return toProcessExitCode(uburu::cli::CliExitCode::usageError);
  }

  const auto& options = *parseResult.options;

  if (options.showHelp) {
    std::cout << uburu::cli::cliHelpText();

    return toProcessExitCode(uburu::cli::CliExitCode::ok);
  }

  if (options.command == uburu::cli::CliCommand::search)
    return toProcessExitCode(runSearch(options));

  std::cout << uburu::cli::cliHelpText();

  return toProcessExitCode(uburu::cli::CliExitCode::ok);
}
