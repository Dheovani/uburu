#include "core/document/pptx-document-extractor.hpp"

#include "core/archive/zip-archive-reader.hpp"
#include "core/document/xml-document-utils.hpp"

#include <algorithm>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace uburu::document
{
  namespace
  {

    constexpr std::string_view extractorName = "pptx";
    constexpr std::string_view presentationPath = "ppt/presentation.xml";
    constexpr std::string_view presentationRelationshipsPath = "ppt/_rels/presentation.xml.rels";
    constexpr std::string_view officeDocumentRelationshipPrefix =
      "http://schemas.openxmlformats.org/officeDocument/2006/relationships/";
    constexpr std::string_view slideDirectory = "ppt/slides/";
    constexpr std::string_view slideRelationshipDirectory = "ppt/slides/_rels/";
    constexpr std::string_view slideExtension = ".xml";
    constexpr std::string_view notesDirectory = "ppt/notesSlides/";
    constexpr std::uintmax_t xmlSafetyMultiplier = 16;

    struct PresentationSlide
    {
      std::string relationshipId;
    };

    struct PackageRelationship
    {
      std::string id;
      std::string type;
      std::string target;
    };

    struct SlideText
    {
      std::string text;
      std::string notes;
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
        limits.maximumSingleExpandedEntryBytes * 8U);

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
    bool isSlideEntry(const archive::ZipArchiveEntry& entry)
    {
      return !entry.directory &&
             entry.rawName.starts_with(slideDirectory) &&
             entry.rawName.ends_with(slideExtension) &&
             !entry.rawName.starts_with(slideRelationshipDirectory);
    }

    [[nodiscard]]
    std::string normalizedPackageTarget(std::string_view baseDirectory, std::string target)
    {
      if (target.empty())
        return {};

      if (target.starts_with('/'))
        target.erase(0, 1);
      else
        target = std::string{baseDirectory} + target;

      std::vector<std::string> parts;
      std::size_t offset = 0;

      while (offset <= target.size()) {
        const auto separator = target.find('/', offset);
        const auto partSize = separator == std::string::npos ? std::string::npos : separator - offset;
        const auto part = target.substr(offset, partSize);

        if (part == "..") {
          if (!parts.empty())
            parts.pop_back();
        } else if (!part.empty() && part != ".") {
          parts.push_back(part);
        }

        if (separator == std::string::npos)
          break;

        offset = separator + 1;
      }

      std::string normalized;

      for (const auto& part : parts) {
        if (!normalized.empty())
          normalized.push_back('/');

        normalized += part;
      }

      return normalized;
    }

    [[nodiscard]]
    std::vector<PresentationSlide> presentationSlides(std::string_view xmlText)
    {
      std::vector<PresentationSlide> slides;
      std::size_t offset = 0;

      while (offset < xmlText.size()) {
        const auto tagStart = xmlText.find('<', offset);

        if (tagStart == std::string_view::npos)
          break;

        const auto tagEnd = xmlText.find('>', tagStart + 1);

        if (tagEnd == std::string_view::npos)
          break;

        const auto tagContent = xmlText.substr(tagStart + 1, tagEnd - tagStart - 1);

        if (!xml::isClosingTag(tagContent) && xml::localNameFromTag(tagContent) == "sldid") {
          auto relationshipId = xml::attributeValue(tagContent, "r:id").value_or("");

          if (!relationshipId.empty())
            slides.push_back({.relationshipId = std::move(relationshipId)});
        }

        offset = tagEnd + 1;
      }

      return slides;
    }

    [[nodiscard]]
    std::vector<PackageRelationship> packageRelationships(
      std::string_view xmlText,
      std::string_view baseDirectory)
    {
      std::vector<PackageRelationship> relationships;
      std::size_t offset = 0;

      while (offset < xmlText.size()) {
        const auto tagStart = xmlText.find('<', offset);

        if (tagStart == std::string_view::npos)
          break;

        const auto tagEnd = xmlText.find('>', tagStart + 1);

        if (tagEnd == std::string_view::npos)
          break;

        const auto tagContent = xmlText.substr(tagStart + 1, tagEnd - tagStart - 1);

        if (!xml::isClosingTag(tagContent) && xml::localNameFromTag(tagContent) == "relationship") {
          auto id = xml::attributeValue(tagContent, "id").value_or("");
          auto type = xml::attributeValue(tagContent, "type").value_or("");
          auto target = xml::attributeValue(tagContent, "target").value_or("");

          if (!id.empty() && !type.empty() && !target.empty()) {
            relationships.push_back({
              .id = std::move(id),
              .type = std::move(type),
              .target = normalizedPackageTarget(baseDirectory, std::move(target)),
            });
          }
        }

        offset = tagEnd + 1;
      }

      return relationships;
    }

    [[nodiscard]]
    std::string visibleDrawingText(std::string_view xmlText, std::stop_token stopToken)
    {
      std::string text;
      bool insideText = false;
      std::size_t offset = 0;

      while (offset < xmlText.size()) {
        if (stopToken.stop_requested())
          break;

        const auto tagStart = xmlText.find('<', offset);

        if (tagStart == std::string_view::npos) {
          if (insideText)
            xml::appendDecodedText(xmlText.substr(offset), text);

          break;
        }

        if (insideText)
          xml::appendDecodedText(xmlText.substr(offset, tagStart - offset), text);

        const auto tagEnd = xmlText.find('>', tagStart + 1);

        if (tagEnd == std::string_view::npos)
          break;

        const auto tagContent = xmlText.substr(tagStart + 1, tagEnd - tagStart - 1);
        const auto tagName = xml::localNameFromTag(tagContent);
        const auto closingTag = xml::isClosingTag(tagContent);

        if (tagName == "t") {
          if (closingTag) {
            if (!text.empty() && text.back() != '\n')
              text.push_back('\n');

            insideText = false;
          } else {
            insideText = !xml::isSelfClosingTag(tagContent);
          }
        }

        offset = tagEnd + 1;
      }

      xml::trimTrailingAsciiWhitespace(text);

      return text;
    }

    [[nodiscard]]
    std::optional<std::string> slideRelationshipPath(std::string_view slidePath)
    {
      constexpr std::string_view slidesPathPrefix = "ppt/slides/";

      if (!slidePath.starts_with(slidesPathPrefix))
        return std::nullopt;

      auto fileName = std::string{slidePath.substr(slidesPathPrefix.size())};

      if (fileName.empty())
        return std::nullopt;

      return std::string{slideRelationshipDirectory} + fileName + ".rels";
    }

    [[nodiscard]]
    std::optional<std::string> notesPathForSlide(
      const archive::ZipArchiveCatalog& catalog,
      const archive::ZipArchiveReader& reader,
      const std::filesystem::path& path,
      std::string_view slidePath,
      text::RichFormatSafetyLimits safetyLimits,
      std::stop_token stopToken,
      DocumentExtractionSummary& summary)
    {
      const auto relationshipPath = slideRelationshipPath(slidePath);

      if (!relationshipPath)
        return std::nullopt;

      const auto* relationshipEntry = findEntry(catalog, *relationshipPath);

      if (relationshipEntry == nullptr)
        return std::nullopt;

      const auto relationshipRead = reader.readEntry(path, *relationshipEntry, safetyLimits, stopToken);

      if (relationshipRead.status != archive::ZipArchiveReadStatus::completed) {
        summary.status = statusFromZipStatus(relationshipRead.status);
        summary.error = relationshipRead.error;

        return std::nullopt;
      }

      const auto relationships = packageRelationships(bytesToString(relationshipRead.bytes), slideDirectory);
      const auto notesType = std::string{officeDocumentRelationshipPrefix} + "notesSlide";

      for (const auto& relationship : relationships) {
        if (relationship.type == notesType && relationship.target.starts_with(notesDirectory))
          return relationship.target;
      }

      return std::nullopt;
    }

    [[nodiscard]]
    std::map<std::string, const archive::ZipArchiveEntry*> entryMap(const archive::ZipArchiveCatalog& catalog)
    {
      std::map<std::string, const archive::ZipArchiveEntry*> mappedEntries;

      for (const auto& entry : catalog.entries) {
        if (!entry.directory)
          mappedEntries.emplace(entry.rawName, &entry);
      }

      return mappedEntries;
    }

    [[nodiscard]]
    std::vector<const archive::ZipArchiveEntry*> orderedSlideEntries(
      const archive::ZipArchiveCatalog& catalog,
      const std::vector<PresentationSlide>& slides,
      const std::vector<PackageRelationship>& relationships,
      DocumentExtractionSummary& summary)
    {
      const auto slideType = std::string{officeDocumentRelationshipPrefix} + "slide";
      std::vector<const archive::ZipArchiveEntry*> entries;
      const auto availableEntries = entryMap(catalog);

      if (!slides.empty()) {
        if (relationships.empty()) {
          summary.status = DocumentExtractionStatus::parserFailed;

          return {};
        }

        for (const auto& slide : slides) {
          const auto relationship = std::ranges::find(relationships, slide.relationshipId, &PackageRelationship::id);

          if (relationship == relationships.end() || relationship->type != slideType) {
            summary.status = DocumentExtractionStatus::parserFailed;

            return {};
          }

          const auto entry = availableEntries.find(relationship->target);

          if (entry == availableEntries.end()) {
            summary.status = DocumentExtractionStatus::parserFailed;

            return {};
          }

          entries.push_back(entry->second);
        }

        return entries;
      }

      for (const auto& entry : catalog.entries) {
        if (isSlideEntry(entry))
          entries.push_back(&entry);
      }

      std::ranges::sort(entries, {}, [](const auto* entry) { return entry->rawName; });

      return entries;
    }

    [[nodiscard]]
    std::optional<SlideText> readSlideText(
      const archive::ZipArchiveCatalog& catalog,
      const archive::ZipArchiveReader& reader,
      const std::filesystem::path& path,
      const archive::ZipArchiveEntry& slideEntry,
      text::RichFormatSafetyLimits safetyLimits,
      std::stop_token stopToken,
      DocumentExtractionSummary& summary)
    {
      const auto slideRead = reader.readEntry(path, slideEntry, safetyLimits, stopToken);

      if (slideRead.status != archive::ZipArchiveReadStatus::completed) {
        summary.status = statusFromZipStatus(slideRead.status);
        summary.error = slideRead.error;

        return std::nullopt;
      }

      SlideText slideText;
      slideText.text = visibleDrawingText(bytesToString(slideRead.bytes), stopToken);

      const auto notesPath = notesPathForSlide(
        catalog,
        reader,
        path,
        slideEntry.rawName,
        safetyLimits,
        stopToken,
        summary);

      if (summary.status != DocumentExtractionStatus::completed)
        return std::nullopt;

      if (!notesPath)
        return slideText;

      const auto* notesEntry = findEntry(catalog, *notesPath);

      if (notesEntry == nullptr)
        return slideText;

      const auto notesRead = reader.readEntry(path, *notesEntry, safetyLimits, stopToken);

      if (notesRead.status != archive::ZipArchiveReadStatus::completed) {
        summary.status = statusFromZipStatus(notesRead.status);
        summary.error = notesRead.error;

        return std::nullopt;
      }

      slideText.notes = visibleDrawingText(bytesToString(notesRead.bytes), stopToken);

      return slideText;
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

  std::string_view PptxDocumentExtractor::name() const
  {
    return extractorName;
  }

  bool PptxDocumentExtractor::supports(const std::filesystem::path& path) const
  {
    return xml::lowerAscii(path.extension().string()) == ".pptx";
  }

  DocumentExtractionSummary PptxDocumentExtractor::extract(
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

    std::vector<PresentationSlide> slides;

    if (const auto* presentationEntry = findEntry(catalog, presentationPath)) {
      const auto presentationRead = reader.readEntry(path, *presentationEntry, safetyLimits, stopToken);

      if (presentationRead.status != archive::ZipArchiveReadStatus::completed) {
        summary.status = statusFromZipStatus(presentationRead.status);
        summary.error = presentationRead.error;

        return summary;
      }

      slides = presentationSlides(bytesToString(presentationRead.bytes));
    }

    std::vector<PackageRelationship> relationships;

    if (const auto* relationshipEntry = findEntry(catalog, presentationRelationshipsPath)) {
      const auto relationshipRead = reader.readEntry(path, *relationshipEntry, safetyLimits, stopToken);

      if (relationshipRead.status != archive::ZipArchiveReadStatus::completed) {
        summary.status = statusFromZipStatus(relationshipRead.status);
        summary.error = relationshipRead.error;

        return summary;
      }

      relationships = packageRelationships(bytesToString(relationshipRead.bytes), "ppt/");
    }

    const auto slideEntries = orderedSlideEntries(catalog, slides, relationships, summary);

    if (summary.status != DocumentExtractionStatus::completed)
      return summary;

    if (slideEntries.empty()) {
      summary.status = DocumentExtractionStatus::parserFailed;

      return summary;
    }

    std::uintmax_t totalBytes = 0;

    for (std::size_t index = 0; index < slideEntries.size(); ++index) {
      if (stopToken.stop_requested()) {
        summary.status = DocumentExtractionStatus::cancelled;

        return summary;
      }

      const auto slideText = readSlideText(
        catalog,
        reader,
        path,
        *slideEntries[index],
        safetyLimits,
        stopToken,
        summary);

      if (!slideText)
        return summary;

      ExtractedTextSegment slideSegment;
      slideSegment.text = slideText->text;
      slideSegment.location.kind = DocumentLocationKind::slide;
      slideSegment.location.primary = index + 1;
      slideSegment.location.label = "slide " + std::to_string(index + 1);

      if (!publishSegment(std::move(slideSegment), options, sink, summary, totalBytes))
        return summary;

      ExtractedTextSegment notesSegment;
      notesSegment.text = slideText->notes;
      notesSegment.location.kind = DocumentLocationKind::slide;
      notesSegment.location.primary = index + 1;
      notesSegment.location.secondary = 1;
      notesSegment.location.label = "slide " + std::to_string(index + 1) + " notes";

      if (!publishSegment(std::move(notesSegment), options, sink, summary, totalBytes))
        return summary;
    }

    summary.bytesExtracted = totalBytes;

    return summary;
  }

} // namespace uburu::document
