#include "core/document/document-extractor.hpp"
#include "core/document/docx-document-extractor.hpp"
#include "core/document/html-document-extractor.hpp"
#include "core/document/open-document-extractor.hpp"
#include "core/document/pdf-document-extractor.hpp"
#include "core/document/plain-text-extractor.hpp"
#include "core/document/pptx-document-extractor.hpp"
#include "core/document/rtf-document-extractor.hpp"
#include "core/document/subtitle-document-extractor.hpp"
#include "core/document/xlsx-document-extractor.hpp"
#include "fixtures/test-fixtures.hpp"
#include "helpers/temporary-paths.hpp"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <stop_token>
#include <string>
#include <vector>

namespace
{

  constexpr std::uint16_t unsupportedZipCompressionMethod = 99;
  constexpr std::size_t zipLocalCompressionMethodOffset = 8;
  constexpr std::size_t zipCentralCompressionMethodOffset = 10;
  constexpr std::size_t zipLocalFixedHeaderBytes = 30;
  constexpr std::size_t zipCentralFixedHeaderBytes = 46;

  void writeLittleEndian16At(std::vector<unsigned char>& bytes, std::size_t offset, std::uint16_t value)
  {
    bytes[offset] = static_cast<unsigned char>(value & 0xFFU);
    bytes[offset + 1] = static_cast<unsigned char>((value >> 8U) & 0xFFU);
  }

  [[nodiscard]]
  std::vector<unsigned char> storedZipBytesWithUnsupportedCompression(
    const std::vector<uburu::tests::fixtures::StoredZipEntryFixture>& entries)
  {
    auto bytes = uburu::tests::fixtures::storedZipBytes(entries);
    std::size_t localOffset = 0;

    for (const auto& entry : entries) {
      writeLittleEndian16At(bytes, localOffset + zipLocalCompressionMethodOffset, unsupportedZipCompressionMethod);
      localOffset += zipLocalFixedHeaderBytes + entry.name.size() + entry.content.size();
    }

    auto centralOffset = localOffset;

    for (const auto& entry : entries) {
      writeLittleEndian16At(bytes, centralOffset + zipCentralCompressionMethodOffset, unsupportedZipCompressionMethod);
      centralOffset += zipCentralFixedHeaderBytes + entry.name.size();
    }

    return bytes;
  }

} // namespace

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

TEST_CASE("document extraction policy exposes stable status and availability names")
{
  CHECK(uburu::document::documentExtractionStatusName(uburu::document::DocumentExtractionStatus::unsupportedFormat) ==
        "unsupportedFormat");
  CHECK(uburu::document::documentExtractionStatusName(uburu::document::DocumentExtractionStatus::parserFailed) ==
        "parserFailed");
  CHECK(uburu::document::documentContentAvailabilityName(
          uburu::document::DocumentContentAvailability::nameOnlyUnsupported) == "nameOnlyUnsupported");
  CHECK(uburu::document::documentContentAvailabilityName(
          uburu::document::DocumentContentAvailability::extractionFailed) == "extractionFailed");
}

TEST_CASE("document extraction policy separates name-only and failed content states")
{
  const auto unsupported =
    uburu::document::documentContentAvailability(uburu::document::DocumentExtractionStatus::unsupportedFormat);
  const auto binary =
    uburu::document::documentContentAvailability(uburu::document::DocumentExtractionStatus::binarySkipped);
  const auto unsafe =
    uburu::document::documentContentAvailability(uburu::document::DocumentExtractionStatus::safetyLimitExceeded);
  const auto protectedDocument =
    uburu::document::documentContentAvailability(uburu::document::DocumentExtractionStatus::encryptedOrProtected);
  const auto failed =
    uburu::document::documentContentAvailability(uburu::document::DocumentExtractionStatus::parserFailed);

  CHECK(unsupported == uburu::document::DocumentContentAvailability::nameOnlyUnsupported);
  CHECK(binary == uburu::document::DocumentContentAvailability::nameOnlyBinary);
  CHECK(unsafe == uburu::document::DocumentContentAvailability::nameOnlySafetyLimited);
  CHECK(protectedDocument == uburu::document::DocumentContentAvailability::nameOnlyProtected);
  CHECK(failed == uburu::document::DocumentContentAvailability::extractionFailed);
  CHECK(uburu::document::isNameOnlySearchable(unsupported));
  CHECK_FALSE(uburu::document::isNameOnlySearchable(failed));
}

TEST_CASE("docx document extractor supports docx extension")
{
  uburu::document::DocxDocumentExtractor extractor;

  CHECK(extractor.supports("document.docx"));
  CHECK(extractor.supports("DOCUMENT.DOCX"));
  CHECK_FALSE(extractor.supports("document.doc"));
}

TEST_CASE("docx document extractor emits visible wordprocessing text")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-docx-visible-test");
  const auto path = directory.path() / "document.docx";
  uburu::document::DocxDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::vector<uburu::document::ExtractedTextSegment> segments;

  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::minimalDocxBytes(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
      "<w:body>"
      "<w:p><w:r><w:t>Hello &amp; needle</w:t></w:r></w:p>"
      "<w:p><w:r><w:t>Second</w:t><w:tab/><w:t>line</w:t></w:r></w:p>"
      "</w:body>"
      "</w:document>"));

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment& segment) {
    segments.push_back(segment);

    return true;
  });

  REQUIRE(summary.status == uburu::document::DocumentExtractionStatus::completed);
  REQUIRE(segments.size() == 1);
  CHECK(segments.front().text == "Hello & needle\nSecond line");
  CHECK(segments.front().location.kind == uburu::document::DocumentLocationKind::none);
}

TEST_CASE("docx document extractor emits metadata and table segments")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-docx-metadata-table-test");
  const auto path = directory.path() / "document.docx";
  uburu::document::DocxDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::vector<uburu::document::ExtractedTextSegment> segments;

  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::storedZipBytes({
      uburu::tests::fixtures::StoredZipEntryFixture{.name = "[Content_Types].xml", .content = "<Types/>"},
      uburu::tests::fixtures::StoredZipEntryFixture{
        .name = "docProps/core.xml",
        .content = "<cp:coreProperties xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
                   "<dc:title>Important title</dc:title>"
                   "<dc:creator>Dheovani</dc:creator>"
                   "<dc:description>Metadata needle</dc:description>"
                   "</cp:coreProperties>"},
      uburu::tests::fixtures::StoredZipEntryFixture{
        .name = "word/document.xml",
        .content = "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
                   "<w:body>"
                   "<w:p><w:r><w:t>Outside paragraph</w:t></w:r></w:p>"
                   "<w:tbl>"
                   "<w:tr>"
                   "<w:tc><w:p><w:r><w:t>A1</w:t></w:r></w:p></w:tc>"
                   "<w:tc><w:p><w:r><w:t>B1 needle</w:t></w:r></w:p></w:tc>"
                   "</w:tr>"
                   "</w:tbl>"
                   "</w:body>"
                   "</w:document>"},
    }));

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment& segment) {
    segments.push_back(segment);

    return true;
  });

  REQUIRE(summary.status == uburu::document::DocumentExtractionStatus::completed);
  REQUIRE(segments.size() == 3);
  CHECK(segments[0].location.label == "metadata");
  CHECK(segments[0].text == "Title: Important title\nCreator: Dheovani\nDescription: Metadata needle");
  CHECK(segments[1].location.label == "document");
  CHECK(segments[1].text == "Outside paragraph");
  CHECK(segments[2].location.label == "table 1");
  CHECK(segments[2].location.primary == 1);
  CHECK(segments[2].text == "A1\tB1 needle");
}

TEST_CASE("docx document extractor applies extracted byte and segment limits before publishing")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-docx-limit-test");
  const auto path = directory.path() / "document.docx";
  uburu::document::DocxDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::size_t deliveredSegments = 0;

  options.maximumSegments = 1;

  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::storedZipBytes({
      uburu::tests::fixtures::StoredZipEntryFixture{.name = "docProps/core.xml",
                                                    .content = "<dc:title>Metadata</dc:title>"},
      uburu::tests::fixtures::StoredZipEntryFixture{
        .name = "word/document.xml",
        .content = "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
                   "<w:body><w:p><w:r><w:t>Body</w:t></w:r></w:p></w:body></w:document>"},
    }));

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment&) {
    ++deliveredSegments;

    return true;
  });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::safetyLimitExceeded);
  CHECK(deliveredSegments == 0);
}

TEST_CASE("docx document extractor rejects oversized XML before publishing text")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-docx-oversized-xml-test");
  const auto path = directory.path() / "document.docx";
  uburu::document::DocxDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::size_t deliveredSegments = 0;

  options.maximumExtractedBytes = 2;

  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::minimalDocxBytes(
      "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
      "<w:body><w:p><w:r><w:t>oversized payload</w:t></w:r></w:p></w:body></w:document>"));

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment&) {
    ++deliveredSegments;

    return true;
  });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::safetyLimitExceeded);
  CHECK(deliveredSegments == 0);
}

TEST_CASE("docx document extractor reports cancellation before reading package content")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-docx-cancel-test");
  const auto path = directory.path() / "document.docx";
  uburu::document::DocxDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::stop_source stopSource;

  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::minimalDocxBytes(
      "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
      "<w:body><w:p><w:r><w:t>needle</w:t></w:r></w:p></w:body></w:document>"));

  stopSource.request_stop();

  const auto summary = extractor.extract(
    path,
    options,
    [](const uburu::document::ExtractedTextSegment&) { return true; },
    stopSource.get_token());

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::cancelled);
}

TEST_CASE("docx document extractor treats unsupported zip features as safety failures")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-docx-unsupported-zip-test");
  const auto path = directory.path() / "document.docx";
  uburu::document::DocxDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;

  uburu::tests::writeBytes(
    path,
    storedZipBytesWithUnsupportedCompression({
      uburu::tests::fixtures::StoredZipEntryFixture{
        .name = "word/document.xml",
        .content = "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"/>"},
    }));

  const auto summary =
    extractor.extract(path, options, [](const uburu::document::ExtractedTextSegment&) { return true; });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::safetyLimitExceeded);
}

TEST_CASE("docx document extractor reports malformed packages as parser failures")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-docx-malformed-test");
  const auto path = directory.path() / "document.docx";
  uburu::document::DocxDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;

  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::storedZipBytes(
      {uburu::tests::fixtures::StoredZipEntryFixture{.name = "word/missing.xml", .content = "<xml/>"}}));

  const auto summary =
    extractor.extract(path, options, [](const uburu::document::ExtractedTextSegment&) { return true; });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::parserFailed);
}

TEST_CASE("xlsx document extractor supports xlsx extension")
{
  uburu::document::XlsxDocumentExtractor extractor;

  CHECK(extractor.supports("document.xlsx"));
  CHECK(extractor.supports("DOCUMENT.XLSX"));
  CHECK_FALSE(extractor.supports("document.xls"));
}

TEST_CASE("xlsx document extractor emits shared and inline worksheet text")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-xlsx-visible-test");
  const auto path = directory.path() / "document.xlsx";
  uburu::document::XlsxDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::vector<uburu::document::ExtractedTextSegment> segments;

  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::minimalXlsxBytes(
      "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
      "<sheetData>"
      "<row r=\"1\"><c r=\"A1\" t=\"s\"><v>0</v></c><c r=\"B1\" t=\"inlineStr\"><is><t>inline needle</t></is></c></row>"
      "<row r=\"2\"><c r=\"A2\"><v>42</v></c></row>"
      "</sheetData>"
      "</worksheet>",
      "<sst xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
      "<si><t>Shared &amp; value</t></si>"
      "</sst>"));

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment& segment) {
    segments.push_back(segment);

    return true;
  });

  REQUIRE(summary.status == uburu::document::DocumentExtractionStatus::completed);
  REQUIRE(segments.size() == 1);
  CHECK(segments.front().text == "Shared & value\tinline needle\n42");
  CHECK(segments.front().location.kind == uburu::document::DocumentLocationKind::sheet);
  CHECK(segments.front().location.primary == 1);
  CHECK(segments.front().location.label == "Sheet One");
}

TEST_CASE("xlsx document extractor follows workbook relationships and rich cell values")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-xlsx-relationships-test");
  const auto path = directory.path() / "document.xlsx";
  uburu::document::XlsxDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::vector<uburu::document::ExtractedTextSegment> segments;

  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::storedZipBytes({
      uburu::tests::fixtures::StoredZipEntryFixture{
        .name = "xl/workbook.xml",
        .content = "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
                   "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
                   "<sheets>"
                   "<sheet name=\"Formula Sheet\" sheetId=\"1\" r:id=\"rIdFormula\"/>"
                   "<sheet name=\"Boolean Sheet\" sheetId=\"2\" r:id=\"rIdBoolean\"/>"
                   "</sheets>"
                   "</workbook>"},
      uburu::tests::fixtures::StoredZipEntryFixture{
        .name = "xl/_rels/workbook.xml.rels",
        .content = "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                   "<Relationship Id=\"rIdBoolean\" "
                   "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" "
                   "Target=\"worksheets/sheet1.xml\"/>"
                   "<Relationship Id=\"rIdFormula\" "
                   "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" "
                   "Target=\"worksheets/sheet2.xml\"/>"
                   "</Relationships>"},
      uburu::tests::fixtures::StoredZipEntryFixture{
        .name = "xl/worksheets/sheet1.xml",
        .content = "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
                   "<sheetData><row><c t=\"b\"><v>1</v></c><c t=\"e\"><v>#N/A</v></c></row></sheetData>"
                   "</worksheet>"},
      uburu::tests::fixtures::StoredZipEntryFixture{
        .name = "xl/worksheets/sheet2.xml",
        .content = "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
                   "<sheetData><row><c><f>SUM(A1:A2)</f><v>3</v></c><c t=\"str\"><v>cached text</v></c></row>"
                   "</sheetData></worksheet>"},
    }));

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment& segment) {
    segments.push_back(segment);

    return true;
  });

  REQUIRE(summary.status == uburu::document::DocumentExtractionStatus::completed);
  REQUIRE(segments.size() == 2);
  CHECK(segments[0].location.label == "Formula Sheet");
  CHECK(segments[0].text == "SUM(A1:A2) = 3\tcached text");
  CHECK(segments[1].location.label == "Boolean Sheet");
  CHECK(segments[1].text == "TRUE\t#N/A");
}

TEST_CASE("xlsx document extractor applies extracted byte limits before publishing")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-xlsx-limit-test");
  const auto path = directory.path() / "document.xlsx";
  uburu::document::XlsxDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::size_t deliveredSegments = 0;

  options.maximumExtractedBytes = 3;

  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::minimalXlsxBytes(
      "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
      "<sheetData><row><c t=\"inlineStr\"><is><t>too large</t></is></c></row></sheetData>"
      "</worksheet>",
      "<sst/>"));

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment&) {
    ++deliveredSegments;

    return true;
  });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::safetyLimitExceeded);
  CHECK(deliveredSegments == 0);
}

TEST_CASE("xlsx document extractor rejects oversized shared strings before publishing text")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-xlsx-oversized-shared-strings-test");
  const auto path = directory.path() / "document.xlsx";
  uburu::document::XlsxDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::size_t deliveredSegments = 0;

  options.maximumExtractedBytes = 2;

  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::minimalXlsxBytes(
      "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
      "<sheetData><row><c t=\"s\"><v>0</v></c></row></sheetData>"
      "</worksheet>",
      "<sst xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
      "<si><t>oversized shared string</t></si>"
      "</sst>"));

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment&) {
    ++deliveredSegments;

    return true;
  });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::safetyLimitExceeded);
  CHECK(deliveredSegments == 0);
}

TEST_CASE("xlsx document extractor reports cancellation before reading workbook content")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-xlsx-cancel-test");
  const auto path = directory.path() / "document.xlsx";
  uburu::document::XlsxDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::stop_source stopSource;

  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::minimalXlsxBytes(
      "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
      "<sheetData><row><c t=\"inlineStr\"><is><t>needle</t></is></c></row></sheetData>"
      "</worksheet>",
      "<sst/>"));

  stopSource.request_stop();

  const auto summary = extractor.extract(
    path,
    options,
    [](const uburu::document::ExtractedTextSegment&) { return true; },
    stopSource.get_token());

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::cancelled);
}

TEST_CASE("xlsx document extractor treats unsupported zip features as safety failures")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-xlsx-unsupported-zip-test");
  const auto path = directory.path() / "document.xlsx";
  uburu::document::XlsxDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;

  uburu::tests::writeBytes(
    path,
    storedZipBytesWithUnsupportedCompression({
      uburu::tests::fixtures::StoredZipEntryFixture{.name = "xl/workbook.xml", .content = "<workbook/>"},
    }));

  const auto summary =
    extractor.extract(path, options, [](const uburu::document::ExtractedTextSegment&) { return true; });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::safetyLimitExceeded);
}

TEST_CASE("xlsx document extractor reports malformed workbook relationship graphs")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-xlsx-malformed-relationships-test");
  const auto path = directory.path() / "document.xlsx";
  uburu::document::XlsxDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;

  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::storedZipBytes({
      uburu::tests::fixtures::StoredZipEntryFixture{
        .name = "xl/workbook.xml",
        .content = "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
                   "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
                   "<sheets><sheet name=\"Missing\" sheetId=\"1\" r:id=\"rIdMissing\"/></sheets></workbook>"},
      uburu::tests::fixtures::StoredZipEntryFixture{
        .name = "xl/_rels/workbook.xml.rels",
        .content = "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                   "<Relationship Id=\"rIdOther\" "
                   "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" "
                   "Target=\"worksheets/sheet1.xml\"/>"
                   "</Relationships>"},
      uburu::tests::fixtures::StoredZipEntryFixture{
        .name = "xl/worksheets/sheet1.xml",
        .content = "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
                   "<sheetData><row><c t=\"inlineStr\"><is><t>orphan</t></is></c></row></sheetData></worksheet>"},
    }));

  const auto summary =
    extractor.extract(path, options, [](const uburu::document::ExtractedTextSegment&) { return true; });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::parserFailed);
}

TEST_CASE("xlsx document extractor reports malformed packages as parser failures")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-xlsx-malformed-test");
  const auto path = directory.path() / "document.xlsx";
  uburu::document::XlsxDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;

  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::storedZipBytes(
      {uburu::tests::fixtures::StoredZipEntryFixture{.name = "xl/workbook.xml", .content = "<xml/>"}}));

  const auto summary =
    extractor.extract(path, options, [](const uburu::document::ExtractedTextSegment&) { return true; });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::parserFailed);
}

TEST_CASE("pptx document extractor supports pptx extension")
{
  uburu::document::PptxDocumentExtractor extractor;

  CHECK(extractor.supports("document.pptx"));
  CHECK(extractor.supports("DOCUMENT.PPTX"));
  CHECK_FALSE(extractor.supports("document.docx"));
}

TEST_CASE("pptx document extractor emits slide and speaker-note text")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-pptx-visible-test");
  const auto path = directory.path() / "document.pptx";
  uburu::document::PptxDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::vector<uburu::document::ExtractedTextSegment> segments;

  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::minimalPptxBytes(
      "<p:sld><p:cSld><p:spTree><a:t>Slide title</a:t><a:t>Slide body</a:t></p:spTree></p:cSld></p:sld>",
      "<p:notes><p:cSld><p:spTree><a:t>Speaker note</a:t></p:spTree></p:cSld></p:notes>"));

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment& segment) {
    segments.push_back(segment);

    return true;
  });

  REQUIRE(summary.status == uburu::document::DocumentExtractionStatus::completed);
  REQUIRE(segments.size() == 2);
  CHECK(segments[0].text == "Slide title\nSlide body");
  CHECK(segments[0].location.kind == uburu::document::DocumentLocationKind::slide);
  CHECK(segments[0].location.primary == 1);
  CHECK(segments[0].location.label == "slide 1");
  CHECK(segments[1].text == "Speaker note");
  CHECK(segments[1].location.secondary == 1);
  CHECK(segments[1].location.label == "slide 1 notes");
}

TEST_CASE("pptx document extractor applies extracted byte limits before publishing")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-pptx-limit-test");
  const auto path = directory.path() / "document.pptx";
  uburu::document::PptxDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::size_t deliveredSegments = 0;

  options.maximumExtractedBytes = 3;
  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::minimalPptxBytes(
      "<p:sld><p:cSld><p:spTree><a:t>too large</a:t></p:spTree></p:cSld></p:sld>"));

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment&) {
    ++deliveredSegments;

    return true;
  });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::safetyLimitExceeded);
  CHECK(summary.segmentsExtracted == 0);
  CHECK(deliveredSegments == 0);
}

TEST_CASE("pptx document extractor reports cancellation before reading package content")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-pptx-cancel-test");
  const auto path = directory.path() / "document.pptx";
  uburu::document::PptxDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::stop_source stopSource;

  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::minimalPptxBytes(
      "<p:sld><p:cSld><p:spTree><a:t>cancel me</a:t></p:spTree></p:cSld></p:sld>"));
  stopSource.request_stop();

  const auto summary = extractor.extract(
    path,
    options,
    [](const uburu::document::ExtractedTextSegment&) { return true; },
    stopSource.get_token());

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::cancelled);
}

TEST_CASE("pptx document extractor treats unsupported zip features as safety failures")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-pptx-unsupported-zip-test");
  const auto path = directory.path() / "document.pptx";
  uburu::document::PptxDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;

  uburu::tests::writeBytes(
    path,
    storedZipBytesWithUnsupportedCompression({
      uburu::tests::fixtures::StoredZipEntryFixture{
        .name = "ppt/slides/slide1.xml",
        .content = "<p:sld><p:cSld><p:spTree><a:t>hidden</a:t></p:spTree></p:cSld></p:sld>"},
    }));

  const auto summary =
    extractor.extract(path, options, [](const uburu::document::ExtractedTextSegment&) { return true; });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::safetyLimitExceeded);
}

TEST_CASE("pptx document extractor reports malformed presentation relationship graphs")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-pptx-malformed-relationships-test");
  const auto path = directory.path() / "document.pptx";
  uburu::document::PptxDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;

  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::storedZipBytes({
      uburu::tests::fixtures::StoredZipEntryFixture{.name = "ppt/presentation.xml",
                                                    .content = "<p:presentation><p:sldIdLst><p:sldId r:id=\"missing\"/>"
                                                               "</p:sldIdLst></p:presentation>"},
      uburu::tests::fixtures::StoredZipEntryFixture{.name = "ppt/_rels/presentation.xml.rels",
                                                    .content = "<Relationships></Relationships>"},
      uburu::tests::fixtures::StoredZipEntryFixture{
        .name = "ppt/slides/slide1.xml",
        .content = "<p:sld><p:cSld><p:spTree><a:t>orphan</a:t></p:spTree></p:cSld></p:sld>"},
    }));

  const auto summary =
    extractor.extract(path, options, [](const uburu::document::ExtractedTextSegment&) { return true; });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::parserFailed);
}

TEST_CASE("open document extractor supports odt ods and odp extensions")
{
  uburu::document::OpenDocumentExtractor extractor;

  CHECK(extractor.supports("document.odt"));
  CHECK(extractor.supports("spreadsheet.ods"));
  CHECK(extractor.supports("presentation.odp"));
  CHECK(extractor.supports("DOCUMENT.ODT"));
  CHECK_FALSE(extractor.supports("document.docx"));
}

TEST_CASE("open document extractor emits text document content and metadata")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-odt-visible-test");
  const auto path = directory.path() / "document.odt";
  uburu::document::OpenDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::vector<uburu::document::ExtractedTextSegment> segments;

  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::minimalOpenDocumentBytes(
      "<office:document-content><office:body><office:text><text:h>Title</text:h><text:p>Hello "
      "<text:s text:c=\"2\"/>world</text:p></office:text></office:body></office:document-content>",
      "<office:document-meta><office:meta><dc:title>ODT title</dc:title><dc:creator>Writer</dc:creator>"
      "</office:meta></office:document-meta>"));

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment& segment) {
    segments.push_back(segment);

    return true;
  });

  REQUIRE(summary.status == uburu::document::DocumentExtractionStatus::completed);
  REQUIRE(segments.size() == 2);
  CHECK(segments[0].text == "Title: ODT title\nCreator: Writer");
  CHECK(segments[0].location.label == "metadata");
  CHECK(segments[1].text == "Title\nHello world");
  CHECK(segments[1].location.label == "document");
}

TEST_CASE("open document extractor emits spreadsheet sheet segments")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-ods-visible-test");
  const auto path = directory.path() / "spreadsheet.ods";
  uburu::document::OpenDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::vector<uburu::document::ExtractedTextSegment> segments;

  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::minimalOpenDocumentBytes(
      "<office:document-content><office:body><office:spreadsheet>"
      "<table:table table:name=\"Data\"><table:table-row><table:table-cell><text:p>Alpha</text:p>"
      "</table:table-cell><table:table-cell><text:p>Beta</text:p></table:table-cell></table:table-row></table:table>"
      "</office:spreadsheet></office:body></office:document-content>"));

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment& segment) {
    segments.push_back(segment);

    return true;
  });

  REQUIRE(summary.status == uburu::document::DocumentExtractionStatus::completed);
  REQUIRE(segments.size() == 1);
  CHECK(segments.front().text == "Alpha\nBeta");
  CHECK(segments.front().location.kind == uburu::document::DocumentLocationKind::sheet);
  CHECK(segments.front().location.primary == 1);
  CHECK(segments.front().location.label == "Data");
}

TEST_CASE("open document extractor emits presentation slide segments")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-odp-visible-test");
  const auto path = directory.path() / "presentation.odp";
  uburu::document::OpenDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::vector<uburu::document::ExtractedTextSegment> segments;

  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::minimalOpenDocumentBytes(
      "<office:document-content><office:body><office:presentation>"
      "<draw:page draw:name=\"Intro\"><draw:frame><text:p>Slide title</text:p><text:p>Slide body</text:p>"
      "</draw:frame></draw:page></office:presentation></office:body></office:document-content>"));

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment& segment) {
    segments.push_back(segment);

    return true;
  });

  REQUIRE(summary.status == uburu::document::DocumentExtractionStatus::completed);
  REQUIRE(segments.size() == 1);
  CHECK(segments.front().text == "Slide title\nSlide body");
  CHECK(segments.front().location.kind == uburu::document::DocumentLocationKind::slide);
  CHECK(segments.front().location.primary == 1);
  CHECK(segments.front().location.label == "Intro");
}

TEST_CASE("open document extractor applies extracted byte limits before publishing")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-open-document-limit-test");
  const auto path = directory.path() / "document.odt";
  uburu::document::OpenDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::size_t deliveredSegments = 0;

  options.maximumExtractedBytes = 3;
  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::minimalOpenDocumentBytes(
      "<office:document-content><office:body><office:text><text:p>too large</text:p></office:text></office:body>"
      "</office:document-content>"));

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment&) {
    ++deliveredSegments;

    return true;
  });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::safetyLimitExceeded);
  CHECK(summary.segmentsExtracted == 0);
  CHECK(deliveredSegments == 0);
}

TEST_CASE("open document extractor reports cancellation before reading package content")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-open-document-cancel-test");
  const auto path = directory.path() / "document.odt";
  uburu::document::OpenDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::stop_source stopSource;

  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::minimalOpenDocumentBytes(
      "<office:document-content><office:body><office:text><text:p>cancel me</text:p></office:text></office:body>"
      "</office:document-content>"));
  stopSource.request_stop();

  const auto summary = extractor.extract(
    path,
    options,
    [](const uburu::document::ExtractedTextSegment&) { return true; },
    stopSource.get_token());

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::cancelled);
}

TEST_CASE("open document extractor treats unsupported zip features as safety failures")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-open-document-unsupported-zip-test");
  const auto path = directory.path() / "document.odt";
  uburu::document::OpenDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;

  uburu::tests::writeBytes(
    path,
    storedZipBytesWithUnsupportedCompression({
      uburu::tests::fixtures::StoredZipEntryFixture{
        .name = "content.xml",
        .content = "<office:document-content><text:p>hidden</text:p></office:document-content>"},
    }));

  const auto summary =
    extractor.extract(path, options, [](const uburu::document::ExtractedTextSegment&) { return true; });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::safetyLimitExceeded);
}

TEST_CASE("open document extractor reports packages without content xml as parser failures")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-open-document-malformed-test");
  const auto path = directory.path() / "document.odt";
  uburu::document::OpenDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;

  uburu::tests::writeBytes(
    path,
    uburu::tests::fixtures::storedZipBytes(
      {uburu::tests::fixtures::StoredZipEntryFixture{.name = "meta.xml", .content = "<office:meta/>"}}));

  const auto summary =
    extractor.extract(path, options, [](const uburu::document::ExtractedTextSegment&) { return true; });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::parserFailed);
}

TEST_CASE("pdf document extractor supports pdf extension")
{
  uburu::document::PdfDocumentExtractor extractor;

  CHECK(extractor.supports("document.pdf"));
  CHECK(extractor.supports("DOCUMENT.PDF"));
  CHECK_FALSE(extractor.supports("document.odt"));
}

TEST_CASE("pdf document extractor emits page scoped literal text")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-pdf-visible-test");
  const auto path = directory.path() / "document.pdf";
  uburu::document::PdfDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::vector<uburu::document::ExtractedTextSegment> segments;

  uburu::tests::writeFile(path, uburu::tests::fixtures::minimalPdfText("BT (Hello PDF) Tj (Second line) Tj ET"));

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment& segment) {
    segments.push_back(segment);

    return true;
  });

  REQUIRE(summary.status == uburu::document::DocumentExtractionStatus::completed);
  REQUIRE(segments.size() == 1);
  CHECK(segments.front().text == "Hello PDF Second line");
  CHECK(segments.front().location.kind == uburu::document::DocumentLocationKind::page);
  CHECK(segments.front().location.primary == 1);
  CHECK(segments.front().location.label == "page 1");
}

TEST_CASE("pdf document extractor decodes hex strings in page streams")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-pdf-hex-test");
  const auto path = directory.path() / "document.pdf";
  uburu::document::PdfDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::vector<uburu::document::ExtractedTextSegment> segments;

  uburu::tests::writeFile(path, uburu::tests::fixtures::minimalPdfText("BT <48657820504446> Tj ET"));

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment& segment) {
    segments.push_back(segment);

    return true;
  });

  REQUIRE(summary.status == uburu::document::DocumentExtractionStatus::completed);
  REQUIRE(segments.size() == 1);
  CHECK(segments.front().text == "Hex PDF");
}

TEST_CASE("pdf document extractor applies extracted byte limits before publishing")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-pdf-limit-test");
  const auto path = directory.path() / "document.pdf";
  uburu::document::PdfDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::size_t deliveredSegments = 0;

  options.maximumExtractedBytes = 3;
  uburu::tests::writeFile(path, uburu::tests::fixtures::minimalPdfText("BT (too large) Tj ET"));

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment&) {
    ++deliveredSegments;

    return true;
  });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::safetyLimitExceeded);
  CHECK(summary.segmentsExtracted == 0);
  CHECK(deliveredSegments == 0);
}

TEST_CASE("pdf document extractor reports cancellation before parsing")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-pdf-cancel-test");
  const auto path = directory.path() / "document.pdf";
  uburu::document::PdfDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::stop_source stopSource;

  uburu::tests::writeFile(path, uburu::tests::fixtures::minimalPdfText("BT (cancel me) Tj ET"));
  stopSource.request_stop();

  const auto summary = extractor.extract(
    path,
    options,
    [](const uburu::document::ExtractedTextSegment&) { return true; },
    stopSource.get_token());

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::cancelled);
}

TEST_CASE("pdf document extractor reports encrypted documents as protected")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-pdf-encrypted-test");
  const auto path = directory.path() / "document.pdf";
  uburu::document::PdfDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;

  uburu::tests::writeFile(path, "%PDF-1.4\ntrailer\n<< /Encrypt 5 0 R >>\n%%EOF\n");

  const auto summary =
    extractor.extract(path, options, [](const uburu::document::ExtractedTextSegment&) { return true; });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::encryptedOrProtected);
}

TEST_CASE("pdf document extractor reports malformed files as parser failures")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-pdf-malformed-test");
  const auto path = directory.path() / "document.pdf";
  uburu::document::PdfDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;

  uburu::tests::writeFile(path, "not a pdf");

  const auto summary =
    extractor.extract(path, options, [](const uburu::document::ExtractedTextSegment&) { return true; });

  CHECK(summary.status == uburu::document::DocumentExtractionStatus::parserFailed);
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

TEST_CASE("subtitle document extractor streams cue text with timestamp locations")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-subtitle-test");
  const auto path = directory.path() / "sample.srt";
  uburu::document::SubtitleDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::vector<uburu::document::ExtractedTextSegment> segments;

  uburu::tests::writeFile(path, "1\n00:00:01,500 --> 00:00:03,000\nHello\nworld\n\n");

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment& segment) {
    segments.push_back(segment);

    return true;
  });

  REQUIRE(summary.status == uburu::document::DocumentExtractionStatus::completed);
  REQUIRE(segments.size() == 1);
  CHECK(segments.front().text == "Hello world");
  CHECK(segments.front().location.kind == uburu::document::DocumentLocationKind::timestamp);
  CHECK(segments.front().location.primary == 1500);
  CHECK(segments.front().location.label == "00:00:01,500");
}

TEST_CASE("subtitle document extractor ignores webvtt headers notes and cue markup")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-webvtt-test");
  const auto path = directory.path() / "sample.vtt";
  uburu::document::SubtitleDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::vector<uburu::document::ExtractedTextSegment> segments;

  uburu::tests::writeFile(path,
                          "WEBVTT\n\nNOTE hidden text\nstill hidden\n\n00:01.000 --> 00:02.000\n<v Bob>Hello "
                          "<i>visible</i></v>\n");

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment& segment) {
    segments.push_back(segment);

    return true;
  });

  REQUIRE(summary.status == uburu::document::DocumentExtractionStatus::completed);
  REQUIRE(segments.size() == 1);
  CHECK(segments.front().text == "Hello visible");
  CHECK(segments.front().location.primary == 1000);
}

TEST_CASE("rtf document extractor emits visible text and decodes common escapes")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-rtf-visible-test");
  const auto path = directory.path() / "sample.rtf";
  uburu::document::RtfDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::vector<uburu::document::ExtractedTextSegment> segments;

  uburu::tests::writeFile(
    path,
    "{\\rtf1\\ansi Visible \\b bold\\b0\\par Unicode \\u233\\'e9 caf\\'e9 {\\fonttbl hiddenNeedle}}");

  const auto summary = extractor.extract(path, options, [&](const uburu::document::ExtractedTextSegment& segment) {
    segments.push_back(segment);

    return true;
  });

  REQUIRE(summary.status == uburu::document::DocumentExtractionStatus::completed);
  REQUIRE(segments.size() == 1);
  std::string expectedText = "Visible bold\nUnicode ";

  expectedText.push_back(static_cast<char>(0xC3));
  expectedText.push_back(static_cast<char>(0xA9));
  expectedText += " caf";
  expectedText.push_back(static_cast<char>(0xC3));
  expectedText.push_back(static_cast<char>(0xA9));

  CHECK(segments.front().text == expectedText);
  CHECK(segments.front().location.kind == uburu::document::DocumentLocationKind::none);
}

TEST_CASE("rtf document extractor skips images objects and oversized binary payloads")
{
  uburu::tests::TemporaryDirectory directory("uburu-document-rtf-safety-test");
  const auto safePath = directory.path() / "safe.rtf";
  const auto unsafePath = directory.path() / "unsafe.rtf";
  uburu::document::RtfDocumentExtractor extractor;
  uburu::document::DocumentExtractionOptions options;
  std::vector<uburu::document::ExtractedTextSegment> segments;

  uburu::tests::writeFile(safePath, "{\\rtf1 Visible {\\pict hiddenNeedle} text}");

  const auto safeSummary =
    extractor.extract(safePath, options, [&](const uburu::document::ExtractedTextSegment& segment) {
      segments.push_back(segment);

      return true;
    });

  REQUIRE(safeSummary.status == uburu::document::DocumentExtractionStatus::completed);
  REQUIRE(segments.size() == 1);
  CHECK(segments.front().text == "Visible  text");

  uburu::tests::writeFile(unsafePath, "{\\rtf1 Visible \\bin999999999 hidden}");

  const auto unsafeSummary =
    extractor.extract(unsafePath, options, [](const uburu::document::ExtractedTextSegment&) { return true; });

  CHECK(unsafeSummary.status == uburu::document::DocumentExtractionStatus::safetyLimitExceeded);
}
