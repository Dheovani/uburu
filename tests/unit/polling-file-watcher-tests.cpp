#include "core/filesystem/polling-file-watcher.hpp"
#include "helpers/temporary-paths.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <stop_token>

namespace
{

  using uburu::tests::TemporaryDirectory;
  using uburu::tests::writeFile;

} // namespace

TEST_CASE("polling file watcher reports created files")
{
  TemporaryDirectory directory("uburu-polling-file-watcher-created-test");
  uburu::filesystem::PollingFileWatcher watcher(directory.path());

  writeFile(directory.path() / "created.txt", "created\n");

  const auto batch = watcher.poll();
  const auto& events = batch.events;

  CHECK_FALSE(batch.eventsMayBeIncomplete);
  CHECK_FALSE(batch.requiresRescan);
  REQUIRE(events.size() == 1);
  CHECK(events.front().relativePath == std::filesystem::path("created.txt"));
  CHECK(events.front().kind == uburu::filesystem::FileChangeKind::created);
  CHECK_FALSE(events.front().directory);
}

TEST_CASE("polling file watcher reports modified files")
{
  TemporaryDirectory directory("uburu-polling-file-watcher-modified-test");
  writeFile(directory.path() / "modified.txt", "before\n");
  uburu::filesystem::PollingFileWatcher watcher(directory.path());

  writeFile(directory.path() / "modified.txt", "after with a different size\n");

  const auto batch = watcher.poll();
  const auto& events = batch.events;

  CHECK_FALSE(batch.eventsMayBeIncomplete);
  CHECK_FALSE(batch.requiresRescan);
  REQUIRE(events.size() == 1);
  CHECK(events.front().relativePath == std::filesystem::path("modified.txt"));
  CHECK(events.front().kind == uburu::filesystem::FileChangeKind::modified);
}

TEST_CASE("polling file watcher reports deleted files")
{
  TemporaryDirectory directory("uburu-polling-file-watcher-deleted-test");
  const auto path = directory.path() / "deleted.txt";
  writeFile(path, "deleted\n");
  uburu::filesystem::PollingFileWatcher watcher(directory.path());

  std::filesystem::remove(path);

  const auto batch = watcher.poll();
  const auto& events = batch.events;

  CHECK_FALSE(batch.eventsMayBeIncomplete);
  CHECK_FALSE(batch.requiresRescan);
  REQUIRE(events.size() == 1);
  CHECK(events.front().relativePath == std::filesystem::path("deleted.txt"));
  CHECK(events.front().kind == uburu::filesystem::FileChangeKind::deleted);
}

TEST_CASE("polling file watcher requests rescan after cancelled snapshot")
{
  TemporaryDirectory directory("uburu-polling-file-watcher-cancelled-test");
  writeFile(directory.path() / "known.txt", "known\n");
  uburu::filesystem::PollingFileWatcher watcher(directory.path());

  std::stop_source cancellation;
  cancellation.request_stop();

  const auto cancelled = watcher.poll(cancellation.get_token());

  CHECK(cancelled.events.empty());
  CHECK(cancelled.eventsMayBeIncomplete);
  CHECK(cancelled.requiresRescan);

  writeFile(directory.path() / "created-after-cancel.txt", "created\n");

  const auto recovered = watcher.poll();

  CHECK_FALSE(recovered.eventsMayBeIncomplete);
  CHECK_FALSE(recovered.requiresRescan);
  REQUIRE(recovered.events.size() == 1);
  CHECK(recovered.events.front().relativePath == std::filesystem::path("created-after-cancel.txt"));
  CHECK(recovered.events.front().kind == uburu::filesystem::FileChangeKind::created);
}
