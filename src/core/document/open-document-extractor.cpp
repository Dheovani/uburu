#include "core/document/open-document-extractor.hpp"

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

    constexpr std::string_view extractorName = "open-document";
    constexpr std::string_view contentXmlPath = "content.xml";
    constexpr std::string_view metaXmlPath = "meta.xml";
    constexpr std::string_view documentLocationLabel = "document";
    constexpr std::string_view metadataLocationLabel = "metadata";
    constexpr std::uintmax_t xmlSafetyMultiplier = 16;

    enum class OpenDocumentKind
    {
      text,
      spreadsheet,
      presentation
    };

    struct MetadataField
    {
      std::string_view tagName;
      std::string_view label;
    };

    struct ExtractedOpenDocumentText
    {
      std::string documentText;
      std::vector<ExtractedTextSegment> scopedSegments;
    };

    [[nodiscard]]
    bool wouldExceedByteLimit(const DocumentExtractionOptions& options, std::uintmax_t bytes)
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
    text::RichFormatSafetyLimits openDocumentSafetyLimits(const DocumentExtractionOptions& options)
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
    std::string bytesToString(const std::vector<unsigned char>& bytes)
    {
      return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }

    [[nodiscard]]
    const archive::ZipArchiveEntry* findEntry(
      const archive::ZipArchiveCatalog& catalog,
      std::string_view rawName)
    {
      const auto entry = std::ranges::find(catalog.entries, rawName, &archive::ZipArchiveEntry::rawName);

      if (entry == catalog.entries.end())
        return nullptr;

      return &*entry;
    }

    [[nodiscard]]
    OpenDocumentKind kindFromPath(const std::filesystem::path& path)
    {
      const auto extension = xml::lowerAscii(path.extension().string());

      if (extension == ".ods")
        return OpenDocumentKind::spreadsheet;

      if (extension == ".odp")
        return OpenDocumentKind::presentation;

      return OpenDocumentKind::text;
    }

    [[nodiscard]]
    bool supportedExtension(const std::filesystem::path& path)
    {
      const auto extension = xml::lowerAscii(path.extension().string());

      return extension == ".odt" || extension == ".ods" || extension == ".odp";
    }

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

    void appendDecodedTrimmedText(std::string_view text, std::string& output)
    {
      std::string decoded;
      xml::appendDecodedText(text, decoded);

      for (const auto character : decoded) {
        if (xml::isAsciiWhitespace(character)) {
          appendSpace(output);

          continue;
        }

        output.push_back(character);
      }
    }

    [[nodiscard]]
    bool isParagraphBoundary(std::string_view tagName)
    {
      return tagName == "p" || tagName == "h" || tagName == "list-item" || tagName == "table-row";
    }

    [[nodiscard]]
    std::string segmentLabel(OpenDocumentKind kind, std::size_t index, std::string name)
    {
      if (!name.empty())
        return name;

      if (kind == OpenDocumentKind::spreadsheet)
        return "sheet " + std::to_string(index + 1);

      return "slide " + std::to_string(index + 1);
    }

    [[nodiscard]]
    DocumentLocationKind locationKind(OpenDocumentKind kind)
    {
      if (kind == OpenDocumentKind::spreadsheet)
        return DocumentLocationKind::sheet;

      if (kind == OpenDocumentKind::presentation)
        return DocumentLocationKind::slide;

      return DocumentLocationKind::none;
    }

    [[nodiscard]]
    std::optional<std::string_view> scopedElementName(OpenDocumentKind kind)
    {
      if (kind == OpenDocumentKind::spreadsheet)
        return "table";

      if (kind == OpenDocumentKind::presentation)
        return "page";

      return std::nullopt;
    }

    [[nodiscard]]
    ExtractedOpenDocumentText visibleTextFromContentXml(
      std::string_view xmlText,
      OpenDocumentKind kind,
      std::stop_token stopToken)
    {
      ExtractedOpenDocumentText extracted;
      std::string scopedText;
      std::string scopedLabel;
      std::size_t scopedDepth = 0;
      std::size_t scopedIndex = 0;
      std::size_t offset = 0;
      const auto scopedName = scopedElementName(kind);

      while (offset < xmlText.size()) {
        if (stopToken.stop_requested())
          break;

        const auto tagStart = xmlText.find('<', offset);

        if (tagStart == std::string_view::npos) {
          appendDecodedTrimmedText(
            xmlText.substr(offset),
            scopedDepth > 0 ? scopedText : extracted.documentText);

          break;
        }

        appendDecodedTrimmedText(
          xmlText.substr(offset, tagStart - offset),
          scopedDepth > 0 ? scopedText : extracted.documentText);

        const auto tagEnd = xmlText.find('>', tagStart + 1);

        if (tagEnd == std::string_view::npos)
          break;

        const auto tagContent = xmlText.substr(tagStart + 1, tagEnd - tagStart - 1);
        const auto tagName = xml::localNameFromTag(tagContent);
        const auto closingTag = xml::isClosingTag(tagContent);
        const auto selfClosingTag = xml::isSelfClosingTag(tagContent);
        auto& target = scopedDepth > 0 ? scopedText : extracted.documentText;

        if (scopedName && tagName == *scopedName) {
          if (closingTag && scopedDepth > 0) {
            --scopedDepth;

            if (scopedDepth == 0) {
              xml::trimTrailingAsciiWhitespace(scopedText);

              if (!scopedText.empty()) {
                ExtractedTextSegment segment;

                segment.text = std::move(scopedText);
                segment.location.kind = locationKind(kind);
                segment.location.primary = scopedIndex + 1;
                segment.location.label = segmentLabel(kind, scopedIndex, std::move(scopedLabel));
                extracted.scopedSegments.push_back(std::move(segment));
              }

              scopedText = {};
              scopedLabel = {};
              ++scopedIndex;
            }
          } else if (!closingTag && !selfClosingTag) {
            if (scopedDepth == 0) {
              scopedText = {};
              scopedLabel = xml::attributeValue(tagContent, "name").value_or("");
            }

            ++scopedDepth;
          }
        } else if (tagName == "s") {
          const auto count = xml::attributeValue(tagContent, "c").value_or("1");
          const auto spaces = count == "2" ? 2 : 1;

          for (int index = 0; index < spaces; ++index) {
            appendSpace(target);
          }
        } else if (tagName == "tab") {
          appendSpace(target);
        } else if (tagName == "line-break") {
          appendLineBreak(target);
        } else if (closingTag && isParagraphBoundary(tagName)) {
          appendLineBreak(target);
        }

        offset = tagEnd + 1;
      }

      xml::trimTrailingAsciiWhitespace(extracted.documentText);

      return extracted;
    }

    [[nodiscard]]
    std::optional<std::string> firstElementText(std::string_view xmlText, std::string_view wantedTagName)
    {
      std::string value;
      bool insideWantedTag = false;
      std::size_t offset = 0;

      while (offset < xmlText.size()) {
        const auto tagStart = xmlText.find('<', offset);

        if (tagStart == std::string_view::npos) {
          if (insideWantedTag)
            appendDecodedTrimmedText(xmlText.substr(offset), value);

          break;
        }

        if (insideWantedTag)
          appendDecodedTrimmedText(xmlText.substr(offset, tagStart - offset), value);

        const auto tagEnd = xmlText.find('>', tagStart + 1);

        if (tagEnd == std::string_view::npos)
          break;

        const auto tagContent = xmlText.substr(tagStart + 1, tagEnd - tagStart - 1);
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

    void appendMetadataFields(
      std::string& metadataText,
      std::string_view xmlText,
      std::span<const MetadataField> fields)
    {
      for (const auto& field : fields) {
        const auto value = firstElementText(xmlText, field.tagName);

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
    std::string metadataTextFromPackage(
      const archive::ZipArchiveReader& reader,
      const std::filesystem::path& path,
      const archive::ZipArchiveCatalog& catalog,
      text::RichFormatSafetyLimits safetyLimits,
      DocumentExtractionSummary& summary,
      std::stop_token stopToken)
    {
      constexpr std::array fields{
        MetadataField{"title", "Title"},
        MetadataField{"subject", "Subject"},
        MetadataField{"creator", "Creator"},
        MetadataField{"keyword", "Keywords"},
        MetadataField{"description", "Description"},
      };

      const auto* entry = findEntry(catalog, metaXmlPath);

      if (entry == nullptr)
        return {};

      const auto entryRead = reader.readEntry(path, *entry, safetyLimits, stopToken);

      if (entryRead.status != archive::ZipArchiveReadStatus::completed) {
        summary.status = statusFromZipStatus(entryRead.status);
        summary.error = entryRead.error;

        return {};
      }

      std::string metadataText;
      appendMetadataFields(metadataText, bytesToString(entryRead.bytes), fields);

      return metadataText;
    }

    [[nodiscard]]
    bool publishSegment(
      ExtractedTextSegment segment,
      const DocumentExtractionOptions& options,
      const ExtractedTextSink& sink,
      DocumentExtractionSummary& summary,
      std::uintmax_t& totalBytes)
    {
      if (segment.text.empty())
        return true;

      totalBytes += segment.text.size();

      const auto nextSegmentCount = summary.segmentsExtracted + 1;

      if (wouldExceedByteLimit(options, totalBytes) || wouldExceedSegmentLimit(options, nextSegmentCount)) {
        summary.status = DocumentExtractionStatus::safetyLimitExceeded;

        return false;
      }

      if (!sink(segment)) {
        summary.status = DocumentExtractionStatus::cancelled;

        return false;
      }

      ++summary.segmentsExtracted;

      return true;
    }

  } // namespace

  std::string_view OpenDocumentExtractor::name() const
  {
    return extractorName;
  }

  bool OpenDocumentExtractor::supports(const std::filesystem::path& path) const
  {
    return supportedExtension(path);
  }

  DocumentExtractionSummary OpenDocumentExtractor::extract(
    const std::filesystem::path& path,
    const DocumentExtractionOptions& options,
    const ExtractedTextSink& sink,
    std::stop_token stopToken) const
  {
    DocumentExtractionSummary summary;
    archive::ZipArchiveReader reader;
    const auto safetyLimits = openDocumentSafetyLimits(options);
    const auto catalog = reader.readCatalog(path, safetyLimits, stopToken);

    if (catalog.status != archive::ZipArchiveReadStatus::completed) {
      summary.status = statusFromZipStatus(catalog.status);
      summary.error = catalog.error;

      return summary;
    }

    auto metadataText = metadataTextFromPackage(reader, path, catalog, safetyLimits, summary, stopToken);

    if (summary.status != DocumentExtractionStatus::completed)
      return summary;

    const auto* contentEntry = findEntry(catalog, contentXmlPath);

    if (contentEntry == nullptr) {
      summary.status = DocumentExtractionStatus::parserFailed;

      return summary;
    }

    const auto contentRead = reader.readEntry(path, *contentEntry, safetyLimits, stopToken);

    if (contentRead.status != archive::ZipArchiveReadStatus::completed) {
      summary.status = statusFromZipStatus(contentRead.status);
      summary.error = contentRead.error;

      return summary;
    }

    if (stopToken.stop_requested()) {
      summary.status = DocumentExtractionStatus::cancelled;

      return summary;
    }

    auto extracted = visibleTextFromContentXml(bytesToString(contentRead.bytes), kindFromPath(path), stopToken);

    if (stopToken.stop_requested()) {
      summary.status = DocumentExtractionStatus::cancelled;

      return summary;
    }

    std::uintmax_t totalBytes = 0;

    ExtractedTextSegment metadataSegment;
    metadataSegment.text = std::move(metadataText);
    metadataSegment.location.label = metadataLocationLabel;

    if (!publishSegment(std::move(metadataSegment), options, sink, summary, totalBytes))
      return summary;

    ExtractedTextSegment documentSegment;
    documentSegment.text = std::move(extracted.documentText);
    documentSegment.location.label = documentLocationLabel;

    if (!publishSegment(std::move(documentSegment), options, sink, summary, totalBytes))
      return summary;

    for (auto& segment : extracted.scopedSegments) {
      if (!publishSegment(std::move(segment), options, sink, summary, totalBytes))
        return summary;
    }

    summary.bytesExtracted = totalBytes;

    return summary;
  }

} // namespace uburu::document
