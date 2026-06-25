#include "core/text/text-file-reader.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace
{

  std::filesystem::path fixture_path(std::string_view name)
  {
    return std::filesystem::temp_directory_path() / std::string{name};
  }

  void write_bytes(const std::filesystem::path& path, const std::vector<unsigned char>& bytes)
  {
    std::ofstream file(path, std::ios::binary);
    for (const auto byte : bytes)
      file.put(static_cast<char>(byte));
  }

  std::vector<uburu::text::TextLine> read_lines(const std::filesystem::path& path,
                                                uburu::SearchOptions options = {})
  {
    std::vector<uburu::text::TextLine> lines;
    const auto summary =
        uburu::text::read_text_file_lines(path, options, [&](const uburu::text::TextLine& line) {
          lines.push_back(line);

          return true;
        });

    REQUIRE(summary.status == uburu::text::TextReadStatus::completed);

    return lines;
  }

} // namespace

TEST_CASE("text reader strips UTF-8 BOM and supports LF, CRLF and CR endings")
{
  const auto path = fixture_path("uburu-text-reader-utf8-bom.txt");
  write_bytes(path, {0xEFU, 0xBBU, 0xBFU, 'o', 'n', 'e', '\n', 't', 'w', 'o', '\r',
                     '\n',  't',   'h',   'r', 'e', 'e', '\r', 'f', 'o', 'u', 'r'});

  const auto lines = read_lines(path);
  std::filesystem::remove(path);

  REQUIRE(lines.size() == 4);
  CHECK(lines[0].text == "one");
  CHECK(lines[0].ending == uburu::text::LineEnding::lf);
  CHECK(lines[1].text == "two");
  CHECK(lines[1].ending == uburu::text::LineEnding::crlf);
  CHECK(lines[2].text == "three");
  CHECK(lines[2].ending == uburu::text::LineEnding::cr);
  CHECK(lines[3].text == "four");
  CHECK(lines[3].ending == uburu::text::LineEnding::none);
}

TEST_CASE("text reader decodes UTF-16 little endian with BOM")
{
  const auto path = fixture_path("uburu-text-reader-utf16-le.txt");
  write_bytes(path, {0xFFU, 0xFEU, 'a', 0x00U, 0xE7U, 0x00U, 0xE3U, 0x00U, 'o', 0x00U});

  const auto lines = read_lines(path);
  std::filesystem::remove(path);

  REQUIRE(lines.size() == 1);
  CHECK(lines.front().text == std::string{"a\303\247\303\243o"});
}

TEST_CASE("text reader decodes UTF-16 big endian with BOM")
{
  const auto path = fixture_path("uburu-text-reader-utf16-be.txt");
  write_bytes(path, {0xFEU, 0xFFU, 0x00U, 'a', 0x00U, 0xE7U, 0x00U, 0xE3U, 0x00U, 'o'});

  const auto lines = read_lines(path);
  std::filesystem::remove(path);

  REQUIRE(lines.size() == 1);
  CHECK(lines.front().text == std::string{"a\303\247\303\243o"});
}

TEST_CASE("text reader uses configurable Latin-1 fallback")
{
  const auto path = fixture_path("uburu-text-reader-latin1.txt");
  write_bytes(path, {'a', 0xE7U, 0xE3U, 'o'});

  uburu::SearchOptions options;
  options.fallback_encoding = uburu::TextEncoding::latin1;
  const auto lines = read_lines(path, options);
  std::filesystem::remove(path);

  REQUIRE(lines.size() == 1);
  CHECK(lines.front().text == std::string{"a\303\247\303\243o"});
}

TEST_CASE("text reader replaces invalid UTF-8 when policy allows it")
{
  const auto path = fixture_path("uburu-text-reader-invalid-utf8.txt");
  write_bytes(path, {'a', 0xFFU, 'o'});

  uburu::SearchOptions options;
  options.fallback_encoding = uburu::TextEncoding::utf8;
  options.invalid_utf8_policy = uburu::InvalidUtf8Policy::replace;
  std::vector<uburu::text::TextLine> lines;
  const auto summary =
      uburu::text::read_text_file_lines(path, options, [&](const uburu::text::TextLine& line) {
        lines.push_back(line);

        return true;
      });
  std::filesystem::remove(path);

  REQUIRE(summary.status == uburu::text::TextReadStatus::completed);
  CHECK(summary.had_invalid_sequences);
  REQUIRE(lines.size() == 1);
  CHECK(lines.front().text == std::string{"a\357\277\275o"});
}

TEST_CASE("text reader skips binary files using a sample instead of per-line checks")
{
  const auto path = fixture_path("uburu-text-reader-binary.bin");
  write_bytes(path, {'t', 'e', 'x', 't', 0x00U, 'm', 'o', 'r', 'e'});

  uburu::SearchOptions options;
  options.include_binary = false;
  const auto summary =
      uburu::text::read_text_file_lines(path, options, [](const uburu::text::TextLine&) {
        FAIL("binary files should not publish text lines");

        return true;
      });
  std::filesystem::remove(path);

  CHECK(summary.status == uburu::text::TextReadStatus::binary_skipped);
}

TEST_CASE("text reader reports extremely long lines")
{
  const auto path = fixture_path("uburu-text-reader-long-line.txt");
  write_bytes(path, {'a', 'b', 'c', 'd'});

  uburu::SearchOptions options;
  options.maximum_line_length = 3;
  const auto summary = uburu::text::read_text_file_lines(
      path, options, [](const uburu::text::TextLine&) { return true; });
  std::filesystem::remove(path);

  CHECK(summary.status == uburu::text::TextReadStatus::line_too_long);
}

TEST_CASE("visual columns count UTF-8 scalars instead of raw bytes")
{
  const std::string text{"pr\303\251-a\303\247\303\243o"};

  CHECK(uburu::text::visual_column_for_byte_offset(text, 5) == 5);
}
