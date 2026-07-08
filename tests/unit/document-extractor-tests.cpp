#include "core/document/document-extractor.hpp"
#include "core/document/html-document-extractor.hpp"
#include "core/document/plain-text-extractor.hpp"
#include "helpers/temporary-paths.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

TEST_CASE("plain text extractor streams line-based segments")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-plain-text-test");
  const auto path = directory.path() / "sample.txt";
  uburu::document::PlainTextExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::vector<uburu::document::ExtractedTextSegment> segments;

  uburu::tests::writeFile(path, "first\nsecond\n");

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment& segment) {
    segments.push_back(segment);

    return true;
  });

  REQUIRE(summary.status == uburu::document::DocumentExtractionStatus::completed);
  REQUIRE(segments.size() == 2);
  CHECK(segments[0].text == "first");
  CHECK(segments[0].location.kind == uburu::document::DocumentLocationKind::line);
  CHECK(segments[0].location.primary == 1);
  CHECK(segments[1].text == "second");
  CHECK(segments[1].location.primary == 2);
}

TEST_CASE("plain text extractor reports cancellation requested by the sink")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-cancel-test");
  const auto path = directory.path() / "sample.txt";
  uburu::document::PlainTextExtractor extractor;
  uburu::document::DocumentExtractionOptions options;

  uburu::tests::writeFile(path, "first\nsecond\n");

  const auto summary =
    extractor.extract(path, options, [](const uburu::document::ExtractedTextSegment&) { return false; });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::cancelled);
  CHECK(summary.segmentsExtracted == 0);
}

TEST_CASE("plain text extractor enforces extracted byte limits before publishing oversized segments")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-byte-limit-test");
  const auto path = directory.path() / "sample.txt";
  uburu::document::PlainTextExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::size_t deliveredSegments = 0;

  options.maximumExtractedBytes = 3;
  uburu::tests::writeFile(path, "too-large\n");

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment&) {
    ++deliveredSegments;

    return true;
  });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::safetyLimitExceeded);
  CHECK(summary.segmentsExtracted == 0);
  CHECK(deliveredSegments == 0);
}

TEST_CASE("document extractor registry reports unsupported files without a matching extractor")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-registry-empty-test");
  const auto path = directory.path() / "sample.txt";
  uburu::document::DocumentExtractorRegistry registry;
  uburu::document::DocumentExtractionOptions options;

  uburu::tests::writeFile(path, "content\n");

  const auto summary =
    registry.extract(path, options, [](const uburu::document::ExtractedTextSegment&) { return true; });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::unsupportedFormat);
}

TEST_CASE("document extractor registry delegates to the first supporting extractor")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-registry-delegate-test");
  const auto path = directory.path() / "sample.txt";
  uburu::document::DocumentExtractorRegistry registry;
  uburu::document::DocumentExtractionOptions options;
  std::vector<std::string> lines;

  uburu::tests::writeFile(path, "content\n");
  registry.add(std::make_shared<uburu::document::PlainTextExtractor>());

  const auto summary = registry.extract(path, options, [&](const uburu::document::ExtractedTextSegment& segment) {
    lines.push_back(segment.text);

    return true;
  });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::completed);
  REQUIRE(lines.size() == 1);
  CHECK(lines.front() == "content");
}

TEST_CASE("html document extractor supports common html extensions")
{
  uburu::document::HtmlDocumentExtractor extractor;

  CHECK(extractor.supports("index.html"));
  CHECK(extractor.supports("index.htm"));
  CHECK(extractor.supports("index.xhtml"));
  CHECK(extractor.supports("INDEX.HTML"));
  CHECK_FALSE(extractor.supports("index.txt"));
}

TEST_CASE("html document extractor emits visible text and decodes common entities")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-html-visible-test");
  const auto path = directory.path() / "index.html";
  uburu::document::HtmlDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::vector<uburu::document::ExtractedTextSegment> segments;

  uburu::tests::writeFile(path, "<html><body><h1>Title &amp; More</h1><p>caf&#233; &lt;ok&gt;</p></body></html>");

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment& segment) {
    segments.push_back(segment);

    return true;
  });

  REQUIRE(summary.status == uburu::document::DocumentExtractionStatus::completed);
  REQUIRE(segments.size() == 1);
  CHECK(segments.front().text == "Title & More\ncafé <ok>");
  CHECK(segments.front().location.kind == uburu::document::DocumentLocationKind::none);
}

TEST_CASE("html document extractor excludes script style and comments")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-html-hidden-test");
  const auto path = directory.path() / "index.html";
  uburu::document::HtmlDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::vector<uburu::document::ExtractedTextSegment> segments;

  uburu::tests::writeFile(
    path, "<body>Visible<script>hiddenNeedle()</script><style>.hiddenNeedle{}</style><!-- hiddenNeedle -->Text</body>");

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment& segment) {
    segments.push_back(segment);

    return true;
  });

  REQUIRE(summary.status == uburu::document::DocumentExtractionStatus::completed);
  REQUIRE(segments.size() == 1);
  CHECK(segments.front().text == "Visible Text");
}

TEST_CASE("html document extractor enforces extracted byte limits before publishing text")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-html-byte-limit-test");
  const auto path = directory.path() / "index.html";
  uburu::document::HtmlDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::size_t deliveredSegments = 0;

  options.maximumExtractedBytes = 3;
  uburu::tests::writeFile(path, "<body>visible text</body>");

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment&) {
    ++deliveredSegments;

    return true;
  });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::safetyLimitExceeded);
  CHECK(summary.segmentsExtracted == 0);
  CHECK(deliveredSegments == 0);
}
