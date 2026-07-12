#include "cli-runtime.hpp"

#include "cli-output.hpp"

#include <chrono>
#include <csignal>
#include <ostream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace uburu::cli
{
  namespace
  {

    constexpr auto cancellationPollInterval = std::chrono::milliseconds{20};
    constexpr std::size_t cliOutputFlushInterval = 64;

    volatile std::sig_atomic_t cancellationSignalRequested = 0;

    void handleInterruptionSignal(int signal)
    {
      if (signal == SIGINT)
        cancellationSignalRequested = 1;
    }

  } // namespace

  void installCancellationSignalHandler()
  {
    std::signal(SIGINT, handleInterruptionSignal);
  }

  void resetCancellationSignal()
  {
    cancellationSignalRequested = 0;
  }

  CliCancellation::CliCancellation()
  {
    watcher = std::jthread([this](std::stop_token watcherStopToken) {
      watchCancellationSignal(watcherStopToken);
    });
  }

  std::stop_token CliCancellation::stopToken() const
  {
    return stopSource.get_token();
  }

  bool CliCancellation::stopRequested() const
  {
    return stopSource.get_token().stop_requested();
  }

  void CliCancellation::requestStop()
  {
    stopSource.request_stop();
  }

  void CliCancellation::watchCancellationSignal(std::stop_token watcherStopToken)
  {
    while (!watcherStopToken.stop_requested()) {
      if (cancellationSignalRequested != 0) {
        stopSource.request_stop();

        return;
      }

      std::this_thread::sleep_for(cancellationPollInterval);
    }
  }

  CliResultStream::CliResultStream(std::ostream& output, CliOutputFormat format)
    : output(output), format(format)
  {}

  bool CliResultStream::write(SearchResult result)
  {
    if (failed)
      return false;

    writeSearchResult(output, result, format);
    ++pendingResults;

    if (pendingResults >= cliOutputFlushInterval)
      flush();

    failed = failed || !output.good();

    return !failed;
  }

  void CliResultStream::flush()
  {
    output.flush();
    pendingResults = 0;
    failed = failed || !output.good();
  }

  bool CliResultStream::outputFailed() const
  {
    return failed;
  }

} // namespace uburu::cli
