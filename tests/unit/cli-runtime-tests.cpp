#include "cli-runtime.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <csignal>
#include <thread>

namespace
{

  constexpr auto cancellationObservationTimeout = std::chrono::milliseconds{500};
  constexpr auto cancellationObservationInterval = std::chrono::milliseconds{1};

  [[nodiscard]]
  bool waitForCancellation(const uburu::cli::CliCancellation& cancellation)
  {
    const auto deadline = std::chrono::steady_clock::now() + cancellationObservationTimeout;

    while (!cancellation.stopRequested() && std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(cancellationObservationInterval);

    return cancellation.stopRequested();
  }

} // namespace

TEST_CASE("CLI interruption signal requests cooperative cancellation promptly")
{
  uburu::cli::resetCancellationSignal();
  uburu::cli::installCancellationSignalHandler();

  uburu::cli::CliCancellation cancellation;
  const auto startedAt = std::chrono::steady_clock::now();

  REQUIRE(std::raise(SIGINT) == 0);
  REQUIRE(waitForCancellation(cancellation));

  const auto latency = std::chrono::steady_clock::now() - startedAt;

  CHECK(latency < cancellationObservationTimeout);
  CHECK(cancellation.stopToken().stop_requested());
}

TEST_CASE("CLI cancellation signal state can be reset between operations")
{
  uburu::cli::resetCancellationSignal();
  uburu::cli::installCancellationSignalHandler();

  uburu::cli::CliCancellation firstCancellation;

  REQUIRE(std::raise(SIGINT) == 0);
  REQUIRE(waitForCancellation(firstCancellation));

  uburu::cli::resetCancellationSignal();

  uburu::cli::CliCancellation secondCancellation;

  std::this_thread::sleep_for(cancellationObservationInterval * 2);

  CHECK_FALSE(secondCancellation.stopRequested());
}

TEST_CASE("CLI automatic cancellation uses the same cooperative stop token")
{
  constexpr auto automaticCancellationDelay = std::chrono::milliseconds{20};

  uburu::cli::resetCancellationSignal();

  uburu::cli::CliCancellation cancellation(automaticCancellationDelay);

  REQUIRE(waitForCancellation(cancellation));
  CHECK(cancellation.stopToken().stop_requested());
}
