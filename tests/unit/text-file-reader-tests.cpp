#include "core/text/text-file-reader.hpp"
#include "helpers/temporary-paths.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>
#include <vector>

namespace
{

  using uburu::tests::TemporaryFile;
  using uburu::tests::writeBytes;

  std::vector<uburu::text::TextLine> readLines(const std::filesystem::path& path, uburu::SearchOptions options = {})
  {
    std::vector<uburu::text::TextLine> lines;
    const auto summary = uburu::text::readTextFileLines(path, options, [&](const uburu::text::TextLine& line) {
      lines.push_back(line);

      return true;
    });

    REQUIRE(summary.status == uburu::text::TextReadStatus::completed);

    return lines;
  }

} // namespace

TEST_CASE("text reader strips UTF-8 BOM and supports LF, CRLF and CR endings")
{
  TemporaryFile file("uburu-text-reader-utf8-bom.txt");
  writeBytes(file.path(), {0xEFU, 0xBBU, 0xBFU, 'o', 'n', 'e', '\n', 't', 'w', 'o', '\r',
                           '\n',  't',   'h',   'r', 'e', 'e', '\r', 'f', 'o', 'u', 'r'});

  const auto lines = readLines(file.path());

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
  TemporaryFile file("uburu-text-reader-utf16-le.txt");
  writeBytes(file.path(), {0xFFU, 0xFEU, 'a', 0x00U, 0xE7U, 0x00U, 0xE3U, 0x00U, 'o', 0x00U});

  const auto lines = readLines(file.path());

  REQUIRE(lines.size() == 1);
  CHECK(lines.front().text == std::string{"a\303\247\303\243o"});
}

TEST_CASE("text reader decodes UTF-16 big endian with BOM")
{
  TemporaryFile file("uburu-text-reader-utf16-be.txt");
  writeBytes(file.path(), {0xFEU, 0xFFU, 0x00U, 'a', 0x00U, 0xE7U, 0x00U, 0xE3U, 0x00U, 'o'});

  const auto lines = readLines(file.path());

  REQUIRE(lines.size() == 1);
  CHECK(lines.front().text == std::string{"a\303\247\303\243o"});
}

TEST_CASE("text reader prefers valid UTF-8 without BOM before Latin-1 fallback")
{
  TemporaryFile file("uburu-text-reader-utf8-without-bom.txt");
  const std::string text = "gera\xC3\xA7\xC3\xA3o e a corrup\xC3\xA7\xC3\xA3o da mat\xC3\xA9ria";
  const std::vector<unsigned char> bytes(text.begin(), text.end());
  writeBytes(file.path(), bytes);

  uburu::SearchOptions options;
  options.fallbackEncoding = uburu::TextEncoding::latin1;
  std::vector<uburu::text::TextLine> lines;
  const auto summary = uburu::text::readTextFileLines(file.path(), options, [&](const uburu::text::TextLine& line) {
    lines.push_back(line);

    return true;
  });

  REQUIRE(summary.status == uburu::text::TextReadStatus::completed);
  CHECK(summary.encoding == uburu::TextEncoding::utf8);
  REQUIRE(lines.size() == 1);
  CHECK(lines.front().text == text);
}

TEST_CASE("text reader uses configurable Latin-1 fallback")
{
  TemporaryFile file("uburu-text-reader-latin1.txt");
  writeBytes(file.path(), {'a', 0xE7U, 0xE3U, 'o'});

  uburu::SearchOptions options;
  options.fallbackEncoding = uburu::TextEncoding::latin1;
  const auto lines = readLines(file.path(), options);

  REQUIRE(lines.size() == 1);
  CHECK(lines.front().text == std::string{"a\303\247\303\243o"});
}

TEST_CASE("text reader replaces invalid UTF-8 when policy allows it")
{
  TemporaryFile file("uburu-text-reader-invalid-utf8.txt");
  writeBytes(file.path(), {'a', 0xFFU, 'o'});

  uburu::SearchOptions options;
  options.fallbackEncoding = uburu::TextEncoding::utf8;
  options.invalidUtf8Policy = uburu::InvalidUtf8Policy::replace;
  std::vector<uburu::text::TextLine> lines;
  const auto summary = uburu::text::readTextFileLines(file.path(), options, [&](const uburu::text::TextLine& line) {
    lines.push_back(line);

    return true;
  });

  REQUIRE(summary.status == uburu::text::TextReadStatus::completed);
  CHECK(summary.hadInvalidSequences);
  REQUIRE(lines.size() == 1);
  CHECK(lines.front().text == std::string{"a\357\277\275o"});
}

TEST_CASE("text reader skips binary files using a sample instead of per-line checks")
{
  TemporaryFile file("uburu-text-reader-binary.bin");
  writeBytes(file.path(), {'t', 'e', 'x', 't', 0x00U, 'm', 'o', 'r', 'e'});

  uburu::SearchOptions options;
  options.includeBinary = false;
  bool publishedLine = false;
  const auto summary = uburu::text::readTextFileLines(file.path(), options, [&](const uburu::text::TextLine&) {
    publishedLine = true;

    return true;
  });

  CHECK(summary.status == uburu::text::TextReadStatus::binarySkipped);
  CHECK_FALSE(publishedLine);
}

TEST_CASE("text reader reports extremely long lines")
{
  TemporaryFile file("uburu-text-reader-long-line.txt");
  writeBytes(file.path(), {'a', 'b', 'c', 'd'});

  uburu::SearchOptions options;
  options.maximumLineLength = 3;
  const auto summary =
    uburu::text::readTextFileLines(file.path(), options, [](const uburu::text::TextLine&) { return true; });

  CHECK(summary.status == uburu::text::TextReadStatus::lineTooLong);
}

TEST_CASE("visual columns count UTF-8 scalars instead of raw bytes")
{
  const std::string text{"pr\303\251-a\303\247\303\243o"};

  CHECK(uburu::text::visualColumnForByteOffset(text, 5) == 5);
}
