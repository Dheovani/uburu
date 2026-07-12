#pragma once

#include "cli-options.hpp"

#include "shared/types/domain-types.hpp"

#include <cstddef>
#include <iosfwd>
#include <stop_token>
#include <thread>

namespace uburu::cli
{

  void installCancellationSignalHandler();
  void resetCancellationSignal();

  class CliCancellation final
  {
  public:
    CliCancellation();

    [[nodiscard]]
    std::stop_token stopToken() const;

    [[nodiscard]]
    bool stopRequested() const;

    void requestStop();

  private:
    void watchCancellationSignal(std::stop_token watcherStopToken);

    std::stop_source stopSource;
    std::jthread watcher;
  };

  class CliResultStream final
  {
  public:
    CliResultStream(std::ostream& output, CliOutputFormat format);

    [[nodiscard]]
    bool write(SearchResult result);

    void flush();

    [[nodiscard]]
    bool outputFailed() const;

  private:
    std::ostream& output;
    CliOutputFormat format;
    std::size_t pendingResults{0};
    bool failed{false};
  };

} // namespace uburu::cli
