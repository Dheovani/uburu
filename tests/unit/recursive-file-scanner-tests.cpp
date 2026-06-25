#include "core/filesystem/recursive-file-scanner.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
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

  class TemporaryDirectory
  {
  public:
    explicit TemporaryDirectory(std::string name)
        : path_(std::filesystem::temp_directory_path() / std::move(name))
    {
      std::error_code error;

      std::filesystem::remove_all(path_, error);
      std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory()
    {
      std::error_code error;

      std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] const std::filesystem::path& path() const
    {
      return path_;
    }

  private:
    std::filesystem::path path_;
  };

  void write_file(const std::filesystem::path& path, std::string_view content)
  {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
  }

#ifdef _WIN32
  bool create_sparse_file(const std::filesystem::path& path)
  {
    constexpr auto sparse_file_size = 1024LL * 1024LL;

    std::filesystem::create_directories(path.parent_path());

    HANDLE file = CreateFileW(path.wstring().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);

    if (file == INVALID_HANDLE_VALUE)
      return false;

    DWORD bytes_returned = 0;
    const auto sparse_enabled =
      DeviceIoControl(file, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &bytes_returned, nullptr);

    LARGE_INTEGER distance;
    distance.QuadPart = sparse_file_size;
    const auto moved = SetFilePointerEx(file, distance, nullptr, FILE_BEGIN);
    const auto resized = SetEndOfFile(file);

    CloseHandle(file);

    return sparse_enabled != 0 && moved != 0 && resized != 0;
  }
#endif

  bool create_directory_symlink_or_skip(const std::filesystem::path& target,
                                        const std::filesystem::path& link)
  {
    std::error_code error;
    std::filesystem::create_directory_symlink(target, link, error);

    return !error;
  }

  std::vector<uburu::FileEntry> scan_entries(const std::filesystem::path& root,
                                             const uburu::SearchOptions& options)
  {
    uburu::filesystem::RecursiveFileScanner scanner;
    std::vector<uburu::FileEntry> entries;

    scanner.scan(root, options, [&](uburu::FileEntry entry) {
      entries.push_back(std::move(entry));
      return true;
    });

    return entries;
  }

  uburu::diagnostics::SearchMetrics scan_metrics(const std::filesystem::path& root,
                                                 const uburu::SearchOptions& options)
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
  write_file(directory.path() / "main.CPP", "int main() {}\n");
  write_file(directory.path() / "notes.txt", "notes\n");

  uburu::SearchOptions options;
  options.extensions = {"cpp"};

  const auto entries = scan_entries(directory.path(), options);

#ifdef _WIN32
  REQUIRE(entries.size() == 1);
  CHECK(entries.front().relative_path == std::filesystem::path("main.CPP"));
#else
  CHECK(entries.empty());
#endif
}

TEST_CASE("recursive scanner applies directory includes and excludes with exclusion precedence")
{
  TemporaryDirectory directory("uburu-recursive-scanner-directory-filter-test");
  write_file(directory.path() / "src" / "main.cpp", "main\n");
  write_file(directory.path() / "src" / "generated" / "main.cpp", "generated\n");
  write_file(directory.path() / "tests" / "main.cpp", "tests\n");

  uburu::SearchOptions options;
  options.included_directories = {std::filesystem::path("src")};
  options.excluded_directories = {std::filesystem::path("src") / "generated"};

  const auto entries = scan_entries(directory.path(), options);

  REQUIRE(entries.size() == 1);
  CHECK(entries.front().relative_path == std::filesystem::path("src") / "main.cpp");
}

TEST_CASE("recursive scanner applies glob includes and excludes")
{
  TemporaryDirectory directory("uburu-recursive-scanner-glob-filter-test");
  write_file(directory.path() / "src" / "main.cpp", "main\n");
  write_file(directory.path() / "src" / "main.generated.cpp", "generated\n");
  write_file(directory.path() / "docs" / "main.cpp", "docs\n");

  uburu::SearchOptions options;
  options.included_globs = {"src/*.cpp"};
  options.excluded_globs = {"*.generated.cpp"};

  const auto entries = scan_entries(directory.path(), options);

  REQUIRE(entries.size() == 1);
  CHECK(entries.front().relative_path == std::filesystem::path("src") / "main.cpp");
}

TEST_CASE("recursive scanner applies maximum file size before publishing entries")
{
  TemporaryDirectory directory("uburu-recursive-scanner-size-filter-test");
  write_file(directory.path() / "small.txt", "ok");
  write_file(directory.path() / "large.txt", "too large");

  uburu::SearchOptions options;
  options.maximum_file_size = 2;

  const auto entries = scan_entries(directory.path(), options);

  REQUIRE(entries.size() == 1);
  CHECK(entries.front().relative_path == std::filesystem::path("small.txt"));
}

TEST_CASE("recursive scanner publishes entries in deterministic path order")
{
  TemporaryDirectory directory("uburu-recursive-scanner-order-test");
  write_file(directory.path() / "zeta.txt", "z\n");
  write_file(directory.path() / "alpha" / "zeta.txt", "az\n");
  write_file(directory.path() / "alpha" / "beta.txt", "ab\n");

  uburu::SearchOptions options;

  const auto entries = scan_entries(directory.path(), options);

  REQUIRE(entries.size() == 3);
  CHECK(entries[0].relative_path == std::filesystem::path("alpha") / "beta.txt");
  CHECK(entries[1].relative_path == std::filesystem::path("alpha") / "zeta.txt");
  CHECK(entries[2].relative_path == std::filesystem::path("zeta.txt"));
}

TEST_CASE("recursive scanner prioritizes smaller files deterministically")
{
  TemporaryDirectory directory("uburu-recursive-scanner-small-file-priority-test");
  write_file(directory.path() / "a-large.txt", "larger content\n");
  write_file(directory.path() / "z-small.txt", "x\n");
  write_file(directory.path() / "m-small.txt", "y\n");

  uburu::SearchOptions options;

  const auto entries = scan_entries(directory.path(), options);

  REQUIRE(entries.size() == 3);
  CHECK(entries[0].relative_path == std::filesystem::path("m-small.txt"));
  CHECK(entries[1].relative_path == std::filesystem::path("z-small.txt"));
  CHECK(entries[2].relative_path == std::filesystem::path("a-large.txt"));
}

TEST_CASE("recursive scanner respects root gitignore rules")
{
  TemporaryDirectory directory("uburu-recursive-scanner-root-gitignore-test");
  write_file(directory.path() / ".gitignore", "*.log\n!important.log\nbuild/\n");
  write_file(directory.path() / "debug.log", "ignored\n");
  write_file(directory.path() / "important.log", "kept\n");
  write_file(directory.path() / "build" / "output.txt", "ignored\n");
  write_file(directory.path() / "src" / "main.cpp", "kept\n");

  uburu::SearchOptions options;

  const auto entries = scan_entries(directory.path(), options);

  REQUIRE(entries.size() == 2);
  CHECK(entries[0].relative_path == std::filesystem::path("src") / "main.cpp");
  CHECK(entries[1].relative_path == std::filesystem::path("important.log"));
}

TEST_CASE("recursive scanner respects nested gitignore rules")
{
  TemporaryDirectory directory("uburu-recursive-scanner-nested-gitignore-test");
  write_file(directory.path() / "src" / ".gitignore", "*.generated.cpp\n");
  write_file(directory.path() / "src" / "main.cpp", "kept\n");
  write_file(directory.path() / "src" / "main.generated.cpp", "ignored\n");
  write_file(directory.path() / "tests" / "main.generated.cpp", "kept\n");

  uburu::SearchOptions options;

  const auto entries = scan_entries(directory.path(), options);

  REQUIRE(entries.size() == 2);
  CHECK(entries[0].relative_path == std::filesystem::path("src") / "main.cpp");
  CHECK(entries[1].relative_path == std::filesystem::path("tests") / "main.generated.cpp");
}

TEST_CASE("recursive scanner can disable gitignore handling")
{
  TemporaryDirectory directory("uburu-recursive-scanner-disable-gitignore-test");
  write_file(directory.path() / ".gitignore", "*.log\n");
  write_file(directory.path() / "debug.log", "kept\n");

  uburu::SearchOptions options;
  options.respect_gitignore = false;

  const auto entries = scan_entries(directory.path(), options);

  REQUIRE(entries.size() == 1);
  CHECK(entries[0].relative_path == std::filesystem::path("debug.log"));
}

TEST_CASE("recursive scanner respects git info exclude")
{
  TemporaryDirectory directory("uburu-recursive-scanner-git-info-exclude-test");
  write_file(directory.path() / ".git" / "info" / "exclude", "*.local\n");
  write_file(directory.path() / "settings.local", "ignored\n");
  write_file(directory.path() / "settings.example", "kept\n");

  uburu::SearchOptions options;

  const auto entries = scan_entries(directory.path(), options);

  REQUIRE(entries.size() == 1);
  CHECK(entries[0].relative_path == std::filesystem::path("settings.example"));
}

TEST_CASE("recursive scanner respects configured global git ignore files")
{
  TemporaryDirectory directory("uburu-recursive-scanner-global-git-ignore-test");
  const auto global_ignore_file = directory.path() / ".config" / "global-ignore";
  write_file(global_ignore_file, "*.user\n");
  write_file(directory.path() / "app.user", "ignored\n");
  write_file(directory.path() / "app.config", "kept\n");

  uburu::SearchOptions options;
  options.global_git_ignore_files = {global_ignore_file};

  const auto entries = scan_entries(directory.path(), options);

  REQUIRE(entries.size() == 1);
  CHECK(entries[0].relative_path == std::filesystem::path("app.config"));
}

TEST_CASE("recursive scanner records hidden and ignored file metrics")
{
  TemporaryDirectory directory("uburu-recursive-scanner-skip-metrics-test");
  write_file(directory.path() / ".gitignore", "*.log\n");
  write_file(directory.path() / ".hidden", "hidden\n");
  write_file(directory.path() / "debug.log", "ignored\n");
  write_file(directory.path() / "visible.txt", "kept\n");

  uburu::SearchOptions options;

  const auto entries = scan_entries(directory.path(), options);
  const auto metrics = scan_metrics(directory.path(), options);

  REQUIRE(entries.size() == 1);
  CHECK(entries[0].relative_path == std::filesystem::path("visible.txt"));
  CHECK(metrics.hidden_files == 2);
  CHECK(metrics.ignored_files == 1);
}

#ifdef _WIN32
TEST_CASE("recursive scanner marks sparse files on Windows")
{
  TemporaryDirectory directory("uburu-recursive-scanner-sparse-file-test");
  const auto sparse_path = directory.path() / "sparse.bin";

  if (!create_sparse_file(sparse_path))
    SKIP("The current filesystem did not allow creating a sparse test file.");

  uburu::SearchOptions options;

  const auto entries = scan_entries(directory.path(), options);

  REQUIRE(entries.size() == 1);
  CHECK(entries.front().relative_path == std::filesystem::path("sparse.bin"));
  CHECK(entries.front().sparse);
}
#endif

TEST_CASE("recursive scanner does not follow directory symlinks by default")
{
  TemporaryDirectory directory("uburu-recursive-scanner-symlink-skip-test");
  write_file(directory.path() / "target" / "inside.txt", "target\n");

  const auto link = directory.path() / "link";
  if (!create_directory_symlink_or_skip(directory.path() / "target", link))
    SKIP("The current environment did not allow creating a directory symlink.");

  uburu::SearchOptions options;

  const auto entries = scan_entries(directory.path(), options);

  REQUIRE(entries.size() == 1);
  CHECK(entries.front().relative_path == std::filesystem::path("target") / "inside.txt");
}

TEST_CASE("recursive scanner avoids cycles when following directory symlinks")
{
  TemporaryDirectory directory("uburu-recursive-scanner-symlink-cycle-test");
  write_file(directory.path() / "root.txt", "root\n");
  std::filesystem::create_directories(directory.path() / "nested");

  const auto link = directory.path() / "nested" / "loop";
  if (!create_directory_symlink_or_skip(directory.path(), link))
    SKIP("The current environment did not allow creating a directory symlink.");

  uburu::SearchOptions options;
  options.follow_symlinks = true;

  const auto entries = scan_entries(directory.path(), options);

  REQUIRE(entries.size() == 1);
  CHECK(entries.front().relative_path == std::filesystem::path("root.txt"));
}
