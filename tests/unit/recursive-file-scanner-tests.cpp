#include "core/filesystem/recursive-file-scanner.hpp"
#include "fixtures/test-fixtures.hpp"
#include "helpers/temporary-paths.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winioctl.h>
#endif

namespace
{

  using uburu::tests::TemporaryDirectory;
  using uburu::tests::writeFile;

#ifdef _WIN32
  bool createSparseFile(const std::filesystem::path& path)
  {
    constexpr auto sparseFileSize = 1024LL * 1024LL;

    std::filesystem::create_directories(path.parent_path());

    HANDLE file =
      CreateFileW(path.wstring().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (file == INVALID_HANDLE_VALUE)
      return false;

    DWORD bytesReturned = 0;
    const auto sparseEnabled = DeviceIoControl(file, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &bytesReturned, nullptr);

    LARGE_INTEGER distance;
    distance.QuadPart = sparseFileSize;
    const auto moved = SetFilePointerEx(file, distance, nullptr, FILE_BEGIN);
    const auto resized = SetEndOfFile(file);

    CloseHandle(file);

    return sparseEnabled != 0 && moved != 0 && resized != 0;
  }
#endif

  bool createDirectorySymlinkOrSkip(const std::filesystem::path& target, const std::filesystem::path& link)
  {
    std::error_code error;
    std::filesystem::create_directory_symlink(target, link, error);

    return !error;
  }

  std::vector<uburu::FileEntry> scanEntries(const std::filesystem::path& root, const uburu::SearchOptions& options)
  {
    uburu::filesystem::RecursiveFileScanner scanner;
    std::vector<uburu::FileEntry> entries;

    scanner.scan(root, options, [&](uburu::FileEntry entry) {
      entries.push_back(std::move(entry));
      return true;
    });

    return entries;
  }

  uburu::diagnostics::SearchMetrics scanMetrics(const std::filesystem::path& root, const uburu::SearchOptions& options)
  {
    uburu::filesystem::RecursiveFileScanner scanner;
    uburu::diagnostics::SearchMetrics metrics;

    scanner.scan(root, options, [](uburu::FileEntry) { return true; }, {}, &metrics);

    return metrics;
  }

} // namespace

TEST_CASE("recursive scanner filters by extension using platform case rules")
{
  TemporaryDirectory directory("uburu-recursive-scanner-extension-test");
  writeFile(directory.path() / "main.CPP", "int main() {}\n");
  writeFile(directory.path() / "notes.txt", "notes\n");

  uburu::SearchOptions options;
  options.extensions = {"cpp"};

  const auto entries = scanEntries(directory.path(), options);

#ifdef _WIN32
  REQUIRE(entries.size() == 1);
  CHECK(entries.front().relativePath == std::filesystem::path("main.CPP"));
#else
  CHECK(entries.empty());
#endif
}

TEST_CASE("recursive scanner can ignore subdirectories")
{
  TemporaryDirectory directory("uburu-recursive-scanner-subdirectories-test");
  writeFile(directory.path() / "root.txt", "root\n");
  writeFile(directory.path() / "nested" / "child.txt", "child\n");

  uburu::SearchOptions options;
  options.includeSubdirectories = false;

  const auto entries = scanEntries(directory.path(), options);

  REQUIRE(entries.size() == 1);
  CHECK(entries.front().relativePath == std::filesystem::path("root.txt"));
}

TEST_CASE("recursive scanner applies directory includes and excludes with exclusion precedence")
{
  TemporaryDirectory directory("uburu-recursive-scanner-directory-filter-test");
  writeFile(directory.path() / "src" / "main.cpp", "main\n");
  writeFile(directory.path() / "src" / "generated" / "main.cpp", "generated\n");
  writeFile(directory.path() / "tests" / "main.cpp", "tests\n");

  uburu::SearchOptions options;
  options.includedDirectories = {std::filesystem::path("src")};
  options.excludedDirectories = {std::filesystem::path("src") / "generated"};

  const auto entries = scanEntries(directory.path(), options);

  REQUIRE(entries.size() == 1);
  CHECK(entries.front().relativePath == std::filesystem::path("src") / "main.cpp");
}

TEST_CASE("recursive scanner applies glob includes and excludes")
{
  TemporaryDirectory directory("uburu-recursive-scanner-glob-filter-test");
  writeFile(directory.path() / "src" / "main.cpp", "main\n");
  writeFile(directory.path() / "src" / "main.generated.cpp", "generated\n");
  writeFile(directory.path() / "docs" / "main.cpp", "docs\n");

  uburu::SearchOptions options;
  options.includedGlobs = {"src/*.cpp"};
  options.excludedGlobs = {"*.generated.cpp"};

  const auto entries = scanEntries(directory.path(), options);

  REQUIRE(entries.size() == 1);
  CHECK(entries.front().relativePath == std::filesystem::path("src") / "main.cpp");
}

TEST_CASE("recursive scanner applies maximum file size before publishing entries")
{
  TemporaryDirectory directory("uburu-recursive-scanner-size-filter-test");
  writeFile(directory.path() / "small.txt", "ok");
  writeFile(directory.path() / "large.txt", "too large");

  uburu::SearchOptions options;
  options.maximumFileSize = 2;

  const auto entries = scanEntries(directory.path(), options);

  REQUIRE(entries.size() == 1);
  CHECK(entries.front().relativePath == std::filesystem::path("small.txt"));
}

TEST_CASE("recursive scanner publishes entries in deterministic path order")
{
  TemporaryDirectory directory("uburu-recursive-scanner-order-test");
  writeFile(directory.path() / "zeta.txt", "z\n");
  writeFile(directory.path() / "alpha" / "zeta.txt", "az\n");
  writeFile(directory.path() / "alpha" / "beta.txt", "ab\n");

  uburu::SearchOptions options;

  const auto entries = scanEntries(directory.path(), options);

  REQUIRE(entries.size() == 3);
  CHECK(entries[0].relativePath == std::filesystem::path("alpha") / "beta.txt");
  CHECK(entries[1].relativePath == std::filesystem::path("alpha") / "zeta.txt");
  CHECK(entries[2].relativePath == std::filesystem::path("zeta.txt"));
}

TEST_CASE("recursive scanner prioritizes smaller files deterministically")
{
  TemporaryDirectory directory("uburu-recursive-scanner-small-file-priority-test");
  writeFile(directory.path() / "a-large.txt", "larger content\n");
  writeFile(directory.path() / "z-small.txt", "x\n");
  writeFile(directory.path() / "m-small.txt", "y\n");

  uburu::SearchOptions options;

  const auto entries = scanEntries(directory.path(), options);

  REQUIRE(entries.size() == 3);
  CHECK(entries[0].relativePath == std::filesystem::path("m-small.txt"));
  CHECK(entries[1].relativePath == std::filesystem::path("z-small.txt"));
  CHECK(entries[2].relativePath == std::filesystem::path("a-large.txt"));
}

TEST_CASE("recursive scanner respects root gitignore rules")
{
  TemporaryDirectory directory("uburu-recursive-scanner-root-gitignore-test");
  uburu::tests::fixtures::writeRootGitIgnoreFixture(directory.path());

  uburu::SearchOptions options;

  const auto entries = scanEntries(directory.path(), options);

  REQUIRE(entries.size() == 2);
  CHECK(entries[0].relativePath == std::filesystem::path("src") / "main.cpp");
  CHECK(entries[1].relativePath == std::filesystem::path("important.log"));
}

TEST_CASE("recursive scanner respects nested gitignore rules")
{
  TemporaryDirectory directory("uburu-recursive-scanner-nested-gitignore-test");
  uburu::tests::fixtures::writeNestedGitIgnoreFixture(directory.path());

  uburu::SearchOptions options;

  const auto entries = scanEntries(directory.path(), options);

  REQUIRE(entries.size() == 2);
  CHECK(entries[0].relativePath == std::filesystem::path("src") / "main.cpp");
  CHECK(entries[1].relativePath == std::filesystem::path("tests") / "main.generated.cpp");
}

TEST_CASE("recursive scanner can disable gitignore handling")
{
  TemporaryDirectory directory("uburu-recursive-scanner-disable-gitignore-test");
  writeFile(directory.path() / ".gitignore", "*.log\n");
  writeFile(directory.path() / "debug.log", "kept\n");

  uburu::SearchOptions options;
  options.respectGitignore = false;

  const auto entries = scanEntries(directory.path(), options);

  REQUIRE(entries.size() == 1);
  CHECK(entries[0].relativePath == std::filesystem::path("debug.log"));
}

TEST_CASE("recursive scanner respects git info exclude")
{
  TemporaryDirectory directory("uburu-recursive-scanner-git-info-exclude-test");
  writeFile(directory.path() / ".git" / "info" / "exclude", "*.local\n");
  writeFile(directory.path() / "settings.local", "ignored\n");
  writeFile(directory.path() / "settings.example", "kept\n");

  uburu::SearchOptions options;

  const auto entries = scanEntries(directory.path(), options);

  REQUIRE(entries.size() == 1);
  CHECK(entries[0].relativePath == std::filesystem::path("settings.example"));
}

TEST_CASE("recursive scanner respects configured global git ignore files")
{
  TemporaryDirectory directory("uburu-recursive-scanner-global-git-ignore-test");
  const auto globalIgnoreFile = directory.path() / ".config" / "global-ignore";
  writeFile(globalIgnoreFile, "*.user\n");
  writeFile(directory.path() / "app.user", "ignored\n");
  writeFile(directory.path() / "app.config", "kept\n");

  uburu::SearchOptions options;
  options.globalGitIgnoreFiles = {globalIgnoreFile};

  const auto entries = scanEntries(directory.path(), options);

  REQUIRE(entries.size() == 1);
  CHECK(entries[0].relativePath == std::filesystem::path("app.config"));
}

TEST_CASE("recursive scanner records hidden and ignored file metrics")
{
  TemporaryDirectory directory("uburu-recursive-scanner-skip-metrics-test");
  writeFile(directory.path() / ".gitignore", "*.log\n");
  writeFile(directory.path() / ".hidden", "hidden\n");
  writeFile(directory.path() / "debug.log", "ignored\n");
  writeFile(directory.path() / "visible.txt", "kept\n");

  uburu::SearchOptions options;

  const auto entries = scanEntries(directory.path(), options);
  const auto metrics = scanMetrics(directory.path(), options);

  REQUIRE(entries.size() == 1);
  CHECK(entries[0].relativePath == std::filesystem::path("visible.txt"));
  CHECK(metrics.hiddenFiles == 2);
  CHECK(metrics.ignoredFiles == 1);
}

#ifdef _WIN32
TEST_CASE("recursive scanner marks sparse files on Windows")
{
  TemporaryDirectory directory("uburu-recursive-scanner-sparse-file-test");
  const auto sparsePath = directory.path() / "sparse.bin";

  if (!createSparseFile(sparsePath))
    SKIP("The current filesystem did not allow creating a sparse test file.");

  uburu::SearchOptions options;

  const auto entries = scanEntries(directory.path(), options);

  REQUIRE(entries.size() == 1);
  CHECK(entries.front().relativePath == std::filesystem::path("sparse.bin"));
  CHECK(entries.front().sparse);
}
#endif

TEST_CASE("recursive scanner does not follow directory symlinks by default")
{
  TemporaryDirectory directory("uburu-recursive-scanner-symlink-skip-test");
  writeFile(directory.path() / "target" / "inside.txt", "target\n");

  const auto link = directory.path() / "link";
  if (!createDirectorySymlinkOrSkip(directory.path() / "target", link)) {
    SUCCEED("The current environment did not allow creating a directory symlink.");

    return;
  }

  uburu::SearchOptions options;

  const auto entries = scanEntries(directory.path(), options);

  REQUIRE(entries.size() == 1);
  CHECK(entries.front().relativePath == std::filesystem::path("target") / "inside.txt");
}

TEST_CASE("recursive scanner avoids cycles when following directory symlinks")
{
  TemporaryDirectory directory("uburu-recursive-scanner-symlink-cycle-test");
  writeFile(directory.path() / "root.txt", "root\n");
  std::filesystem::create_directories(directory.path() / "nested");

  const auto link = directory.path() / "nested" / "loop";
  if (!createDirectorySymlinkOrSkip(directory.path(), link)) {
    SUCCEED("The current environment did not allow creating a directory symlink.");

    return;
  }

  uburu::SearchOptions options;
  options.followSymlinks = true;

  const auto entries = scanEntries(directory.path(), options);

  REQUIRE(entries.size() == 1);
  CHECK(entries.front().relativePath == std::filesystem::path("root.txt"));
}
