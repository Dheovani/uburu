#include "core/document/docx-document-extractor.hpp"

#include "core/archive/zip-archive-reader.hpp"
#include "core/document/xml-document-utils.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace uburu::document
{
  namespace
  {

    constexpr std::string_view wordDocumentXmlPath = "word/document.xml";
    constexpr std::string_view corePropertiesXmlPath = "docProps/core.xml";
    constexpr std::string_view appPropertiesXmlPath = "docProps/app.xml";
    constexpr std::string_view extractorName = "docx";
    constexpr std::string_view documentLocationLabel = "document";
    constexpr std::string_view metadataLocationLabel = "metadata";
    constexpr std::string_view tableLocationPrefix = "table ";
    constexpr std::uintmax_t xmlSafetyMultiplier = 16;
    constexpr char tableCellSeparator = '\t';

    struct WordDocumentText
    {
      std::string documentText;
      std::vector<std::string> tableTexts;
    };

    struct MetadataField
    {
      std::string_view tagName;
      std::string_view label;
    };

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

    void appendCellBreak(std::string& output)
    {
      while (!output.empty() && output.back() == ' ') {
        output.pop_back();
      }

      if (!output.empty() && output.back() != '\n' && output.back() != tableCellSeparator)
        output.push_back(tableCellSeparator);
    }

    [[nodiscard]]
    std::string& activeTextTarget(WordDocumentText& text, std::size_t tableDepth)
    {
      if (tableDepth == 0)
        return text.documentText;

      return text.tableTexts.back();
    }

    [[nodiscard]]
    bool wouldExceedByteLimit(const DocumentExtractionOptions& options, std::size_t bytes)
    {
      return options.maximumExtractedBytes > 0 && bytes > options.maximumExtractedBytes;
    }

    [[nodiscard]]
    bool wouldExceedSegmentLimit(const DocumentExtractionOptions& options, std::size_t segments)
    {
      return options.maximumSegments > 0 && segments > options.maximumSegments;
    }

    [[nodiscard]]
    std::uint64_t scaledXmlSafetyLimit(std::uintmax_t extractedBytes)
    {
      if (extractedBytes == 0)
        return text::defaultMaximumSingleExpandedEntryBytes;

      if (extractedBytes > std::numeric_limits<std::uint64_t>::max() / xmlSafetyMultiplier)
        return text::defaultMaximumSingleExpandedEntryBytes;

      return static_cast<std::uint64_t>(extractedBytes * xmlSafetyMultiplier);
    }

    [[nodiscard]]
    text::RichFormatSafetyLimits ooxmlSafetyLimits(const DocumentExtractionOptions& options)
    {
      auto limits = text::RichFormatSafetyLimits{};

      if (options.maximumExtractedBytes == 0)
        return limits;

      limits.maximumSingleExpandedEntryBytes = std::min(
        limits.maximumSingleExpandedEntryBytes,
        scaledXmlSafetyLimit(options.maximumExtractedBytes));
      limits.maximumExpandedArchiveBytes = std::min(
        limits.maximumExpandedArchiveBytes,
        limits.maximumSingleExpandedEntryBytes * 4U);

      return limits;
    }

    [[nodiscard]]
    std::optional<std::string> firstElementText(std::string_view xml, std::string_view wantedTagName)
    {
      std::string value;
      bool insideWantedTag = false;
      std::size_t offset = 0;

      while (offset < xml.size()) {
        const auto tagStart = xml.find('<', offset);

        if (tagStart == std::string_view::npos) {
          if (insideWantedTag)
            xml::appendDecodedText(xml.substr(offset), value);

          break;
        }

        if (insideWantedTag)
          xml::appendDecodedText(xml.substr(offset, tagStart - offset), value);

        const auto tagEnd = xml.find('>', tagStart + 1);

        if (tagEnd == std::string_view::npos)
          break;

        const auto tagContent = xml.substr(tagStart + 1, tagEnd - tagStart - 1);
        const auto tagName = xml::localNameFromTag(tagContent);
        const auto closingTag = xml::isClosingTag(tagContent);

        if (tagName == wantedTagName) {
          if (closingTag)
            break;

          insideWantedTag = !xml::isSelfClosingTag(tagContent);
        }

        offset = tagEnd + 1;
      }

      xml::trimTrailingAsciiWhitespace(value);

      if (value.empty())
        return std::nullopt;

      return value;
    }

    void appendMetadataFields(std::string& metadataText, std::string_view xml, std::span<const MetadataField> fields)
    {
      for (const auto& field : fields) {
        const auto value = firstElementText(xml, field.tagName);

        if (!value)
          continue;

        if (!metadataText.empty())
          metadataText.push_back('\n');

        metadataText += field.label;
        metadataText += ": ";
        metadataText += *value;
      }
    }

    [[nodiscard]]
    WordDocumentText visibleTextFromWordDocumentXml(std::string_view xml, std::stop_token stopToken)
    {
      WordDocumentText text;
      bool insideTextNode = false;
      std::size_t tableDepth = 0;
      std::size_t offset = 0;

      text.documentText.reserve(xml.size() / 2);

      while (offset < xml.size()) {
        if (stopToken.stop_requested())
          break;

        const auto tagStart = xml.find('<', offset);

        if (tagStart == std::string_view::npos) {
          if (insideTextNode)
            xml::appendDecodedText(xml.substr(offset), activeTextTarget(text, tableDepth));

          break;
        }

        if (insideTextNode)
          xml::appendDecodedText(xml.substr(offset, tagStart - offset), activeTextTarget(text, tableDepth));

        const auto tagEnd = xml.find('>', tagStart + 1);

        if (tagEnd == std::string_view::npos)
          break;

        const auto tagContent = xml.substr(tagStart + 1, tagEnd - tagStart - 1);
        const auto tagName = xml::localNameFromTag(tagContent);
        const auto closingTag = xml::isClosingTag(tagContent);
        const auto selfClosingTag = xml::isSelfClosingTag(tagContent);

        if (!insideTextNode && tagName == "tbl") {
          if (closingTag) {
            if (tableDepth > 0)
              --tableDepth;
          } else if (!selfClosingTag) {
            if (tableDepth == 0)
              text.tableTexts.emplace_back();

            ++tableDepth;
          }
        } else if (tagName == "t") {
          insideTextNode = !closingTag && !selfClosingTag;
        } else if (!insideTextNode && tagName == "tab") {
          appendSpace(activeTextTarget(text, tableDepth));
        } else if (!insideTextNode && (tagName == "br" || tagName == "cr")) {
          appendLineBreak(activeTextTarget(text, tableDepth));
        } else if (!insideTextNode && closingTag && tagName == "tc") {
          appendCellBreak(activeTextTarget(text, tableDepth));
        } else if (!insideTextNode && closingTag && tagName == "p" && tableDepth == 0) {
          appendLineBreak(activeTextTarget(text, tableDepth));
        } else if (!insideTextNode && closingTag && tagName == "tr") {
          appendLineBreak(activeTextTarget(text, tableDepth));
        }

        offset = tagEnd + 1;
      }

      xml::trimTrailingAsciiWhitespace(text.documentText);

      for (auto& tableText : text.tableTexts) {
        xml::trimTrailingAsciiWhitespace(tableText);
      }

      return text;
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

    [[nodiscard]]
    std::optional<std::string> readZipTextEntry(
      const archive::ZipArchiveReader& reader,
      const std::filesystem::path& path,
      const archive::ZipArchiveCatalog& catalog,
      std::string_view rawName,
      text::RichFormatSafetyLimits safetyLimits,
      DocumentExtractionSummary& summary,
      std::stop_token stopToken)
    {
      const auto entry = std::ranges::find(catalog.entries, rawName, &archive::ZipArchiveEntry::rawName);

      if (entry == catalog.entries.end())
        return std::nullopt;

      const auto entryRead = reader.readEntry(path, *entry, safetyLimits, stopToken);

      if (entryRead.status != archive::ZipArchiveReadStatus::completed) {
        summary.status = statusFromZipStatus(entryRead.status);
        summary.error = entryRead.error;

        return std::nullopt;
      }

      return std::string(reinterpret_cast<const char*>(entryRead.bytes.data()), entryRead.bytes.size());
    }

    [[nodiscard]]
    std::string metadataTextFromPackage(
      const archive::ZipArchiveReader& reader,
      const std::filesystem::path& path,
      const archive::ZipArchiveCatalog& catalog,
      text::RichFormatSafetyLimits safetyLimits,
      DocumentExtractionSummary& summary,
      std::stop_token stopToken)
    {
      constexpr std::array coreFields{
        MetadataField{"title", "Title"},
        MetadataField{"subject", "Subject"},
        MetadataField{"creator", "Creator"},
        MetadataField{"keywords", "Keywords"},
        MetadataField{"description", "Description"},
        MetadataField{"lastmodifiedby", "Last modified by"},
      };

      constexpr std::array appFields{
        MetadataField{"company", "Company"},
        MetadataField{"manager", "Manager"},
      };

      std::string metadataText;

      if (const auto coreProperties =
            readZipTextEntry(reader, path, catalog, corePropertiesXmlPath, safetyLimits, summary, stopToken)) {
        appendMetadataFields(metadataText, *coreProperties, coreFields);
      }

      if (summary.status != DocumentExtractionStatus::completed || stopToken.stop_requested())
        return metadataText;

      if (const auto appProperties =
            readZipTextEntry(reader, path, catalog, appPropertiesXmlPath, safetyLimits, summary, stopToken)) {
        appendMetadataFields(metadataText, *appProperties, appFields);
      }

      return metadataText;
    }

    [[nodiscard]]
    bool publishSegment(
      const ExtractedTextSink& sink,
      DocumentExtractionSummary& summary,
      std::string text,
      std::string label,
      std::size_t primary = 0)
    {
      if (text.empty())
        return true;

      ExtractedTextSegment segment;

      segment.text = std::move(text);
      segment.location.kind = DocumentLocationKind::none;
      segment.location.primary = primary;
      segment.location.label = std::move(label);

      if (!sink(segment)) {
        summary.status = DocumentExtractionStatus::cancelled;

        return false;
      }

      ++summary.segmentsExtracted;
      summary.bytesExtracted += segment.text.size();

      return true;
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
    const auto safetyLimits = ooxmlSafetyLimits(options);
    const auto catalog = reader.readCatalog(path, safetyLimits, stopToken);

    if (catalog.status != archive::ZipArchiveReadStatus::completed) {
      summary.status = statusFromZipStatus(catalog.status);
      summary.error = catalog.error;

      return summary;
    }

    auto metadataText = metadataTextFromPackage(reader, path, catalog, safetyLimits, summary, stopToken);

    if (summary.status != DocumentExtractionStatus::completed)
      return summary;

    if (stopToken.stop_requested()) {
      summary.status = DocumentExtractionStatus::cancelled;

      return summary;
    }

    const auto documentEntry =
      std::ranges::find(catalog.entries, wordDocumentXmlPath, &archive::ZipArchiveEntry::rawName);

    if (documentEntry == catalog.entries.end()) {
      summary.status = DocumentExtractionStatus::parserFailed;

      return summary;
    }

    const auto entryRead = reader.readEntry(path, *documentEntry, safetyLimits, stopToken);

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

    std::size_t totalBytes = metadataText.size() + visibleText.documentText.size();
    std::size_t segmentCount = 0;

    if (!metadataText.empty())
      ++segmentCount;

    if (!visibleText.documentText.empty())
      ++segmentCount;

    for (const auto& tableText : visibleText.tableTexts) {
      if (!tableText.empty()) {
        totalBytes += tableText.size();
        ++segmentCount;
      }
    }

    if (wouldExceedByteLimit(options, totalBytes) || wouldExceedSegmentLimit(options, segmentCount)) {
      summary.status = DocumentExtractionStatus::safetyLimitExceeded;

      return summary;
    }

    if (!publishSegment(sink, summary, std::move(metadataText), std::string{metadataLocationLabel}))
      return summary;

    if (!publishSegment(sink, summary, std::move(visibleText.documentText), std::string{documentLocationLabel}))
      return summary;

    for (std::size_t index = 0; index < visibleText.tableTexts.size(); ++index) {
      auto label = std::string{tableLocationPrefix};
      label += std::to_string(index + 1);

      if (!publishSegment(sink, summary, std::move(visibleText.tableTexts[index]), std::move(label), index + 1))
        return summary;
    }

    return summary;
  }

} // namespace uburu::document
