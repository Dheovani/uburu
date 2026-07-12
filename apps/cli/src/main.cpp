#include "cli-application.hpp"
#include "cli-options.hpp"
#include "cli-runtime.hpp"

#include <iostream>
#include <string_view>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{

#ifdef _WIN32
  void configureConsoleEncoding()
  {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
  }
#else
  void configureConsoleEncoding()
  {}
#endif

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

} // namespace

int main(int argc, char** argv)
{
  configureConsoleEncoding();

  uburu::cli::resetCancellationSignal();
  uburu::cli::installCancellationSignalHandler();

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
    return toProcessExitCode(uburu::cli::runSearch(options));

  if (options.command == uburu::cli::CliCommand::indexStatus)
    return toProcessExitCode(uburu::cli::runIndexStatus(options));

  if (options.command == uburu::cli::CliCommand::indexRebuild)
    return toProcessExitCode(uburu::cli::runIndexRebuild(options));

  std::cout << uburu::cli::cliHelpText();

  return toProcessExitCode(uburu::cli::CliExitCode::ok);
}
