#pragma once

#include "cli-options.hpp"

namespace uburu::cli
{

  [[nodiscard]]
  uburu::cli::CliExitCode runSearch(const uburu::cli::CliOptions& options);

  [[nodiscard]]
  uburu::cli::CliExitCode runIndexStatus(const uburu::cli::CliOptions& options);

  [[nodiscard]]
  uburu::cli::CliExitCode runIndexRebuild(const uburu::cli::CliOptions& options);

}
