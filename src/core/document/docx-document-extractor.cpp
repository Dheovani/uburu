#include "core/document/docx-document-extractor.hpp"

#include "core/archive/zip-archive-reader.hpp"
#include "core/document/xml-document-utils.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

namespace uburu::document
{
  namespace
  {

    constexpr std::string_view wordDocumentXmlPath = "word/document.xml";
    constexpr std::string_view extractorName = "docx";
    constexpr std::string_view documentLocationLabel = "document";

    void appendSpace(std::string& output)
    {
      if (output.empty() || output.back() == '\n' || output.back() == ' ')
        return;

      output.push_back(' ');
    }

    void appendLineBreak(std::string& output)
    {
      while (!output.empty() && output.back() == ' ') {
        output.pop_back();
      }

      if (output.empty() || output.back() == '\n')
        return;

      output.push_back('\n');
    }

    [[nodiscard]]
    bool wouldExceedByteLimit(const DocumentExtractionOptions& options, std::size_t bytes)
    {
      return options.maximumExtractedBytes > 0 && bytes > options.maximumExtractedBytes;
    }

    [[nodiscard]]
    std::string visibleTextFromWordDocumentXml(std::string_view xml, std::stop_token stopToken)
    {
      std::string visibleText;
      bool insideTextNode = false;
      std::size_t offset = 0;

      visibleText.reserve(xml.size() / 2);

      while (offset < xml.size()) {
        if (stopToken.stop_requested())
          break;

        const auto tagStart = xml.find('<', offset);

        if (tagStart == std::string_view::npos) {
          if (insideTextNode)
            xml::appendDecodedText(xml.substr(offset), visibleText);

          break;
        }

        if (insideTextNode)
          xml::appendDecodedText(xml.substr(offset, tagStart - offset), visibleText);

        const auto tagEnd = xml.find('>', tagStart + 1);

        if (tagEnd == std::string_view::npos)
          break;

        const auto tagContent = xml.substr(tagStart + 1, tagEnd - tagStart - 1);
        const auto tagName = xml::localNameFromTag(tagContent);
        const auto closingTag = xml::isClosingTag(tagContent);

        if (tagName == "t") {
          insideTextNode = !closingTag && !xml::isSelfClosingTag(tagContent);
        } else if (!insideTextNode && tagName == "tab") {
          appendSpace(visibleText);
        } else if (!insideTextNode && (tagName == "br" || tagName == "cr")) {
          appendLineBreak(visibleText);
        } else if (closingTag && tagName == "p") {
          appendLineBreak(visibleText);
        }

        offset = tagEnd + 1;
      }

      xml::trimTrailingAsciiWhitespace(visibleText);

      return visibleText;
    }

    [[nodiscard]]
    DocumentExtractionStatus statusFromZipStatus(archive::ZipArchiveReadStatus status)
    {
      switch (status) {
      case archive::ZipArchiveReadStatus::completed:
        return DocumentExtractionStatus::completed;
      case archive::ZipArchiveReadStatus::cancelled:
        return DocumentExtractionStatus::cancelled;
      case archive::ZipArchiveReadStatus::openFailed:
        return DocumentExtractionStatus::openFailed;
      case archive::ZipArchiveReadStatus::readFailed:
        return DocumentExtractionStatus::readFailed;
      case archive::ZipArchiveReadStatus::safetyLimitExceeded:
      case archive::ZipArchiveReadStatus::unsafeEntryName:
      case archive::ZipArchiveReadStatus::unsupportedCompressionMethod:
      case archive::ZipArchiveReadStatus::unsupportedZip64:
        return DocumentExtractionStatus::safetyLimitExceeded;
      case archive::ZipArchiveReadStatus::decompressionFailed:
      case archive::ZipArchiveReadStatus::entryNotFound:
      case archive::ZipArchiveReadStatus::invalidArchive:
        return DocumentExtractionStatus::parserFailed;
      }

      return DocumentExtractionStatus::parserFailed;
    }

  } // namespace

  std::string_view DocxDocumentExtractor::name() const
  {
    return extractorName;
  }

  bool DocxDocumentExtractor::supports(const std::filesystem::path& path) const
  {
    return xml::lowerAscii(path.extension().string()) == ".docx";
  }

  DocumentExtractionSummary DocxDocumentExtractor::extract(
    const std::filesystem::path& path,
    const DocumentExtractionOptions& options,
    const ExtractedTextSink& sink,
    std::stop_token stopToken) const
  {
    DocumentExtractionSummary summary;
    archive::ZipArchiveReader reader;
    const auto catalog = reader.readCatalog(path, {}, stopToken);

    if (catalog.status != archive::ZipArchiveReadStatus::completed) {
      summary.status = statusFromZipStatus(catalog.status);
      summary.error = catalog.error;

      return summary;
    }

    const auto documentEntry =
      std::ranges::find(catalog.entries, wordDocumentXmlPath, &archive::ZipArchiveEntry::rawName);

    if (documentEntry == catalog.entries.end()) {
      summary.status = DocumentExtractionStatus::parserFailed;

      return summary;
    }

    const auto entryRead = reader.readEntry(path, *documentEntry, {}, stopToken);

    if (entryRead.status != archive::ZipArchiveReadStatus::completed) {
      summary.status = statusFromZipStatus(entryRead.status);
      summary.error = entryRead.error;

      return summary;
    }

    if (stopToken.stop_requested()) {
      summary.status = DocumentExtractionStatus::cancelled;

      return summary;
    }

    const std::string xml(reinterpret_cast<const char*>(entryRead.bytes.data()), entryRead.bytes.size());
    auto visibleText = visibleTextFromWordDocumentXml(xml, stopToken);

    if (stopToken.stop_requested()) {
      summary.status = DocumentExtractionStatus::cancelled;

      return summary;
    }

    if (wouldExceedByteLimit(options, visibleText.size())) {
      summary.status = DocumentExtractionStatus::safetyLimitExceeded;

      return summary;
    }

    ExtractedTextSegment segment;

    segment.text = std::move(visibleText);
    segment.location.kind = DocumentLocationKind::none;
    segment.location.label = documentLocationLabel;

    if (!sink(segment)) {
      summary.status = DocumentExtractionStatus::cancelled;

      return summary;
    }

    summary.segmentsExtracted = 1;
    summary.bytesExtracted = segment.text.size();

    return summary;
  }

} // namespace uburu::document
