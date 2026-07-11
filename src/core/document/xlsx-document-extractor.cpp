#include "core/document/xlsx-document-extractor.hpp"

#include "core/archive/zip-archive-reader.hpp"
#include "core/document/xml-document-utils.hpp"

#include <algorithm>
#include <charconv>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace uburu::document
{
  namespace
  {

    constexpr std::string_view extractorName = "xlsx";
    constexpr std::string_view sharedStringsPath = "xl/sharedStrings.xml";
    constexpr std::string_view workbookPath = "xl/workbook.xml";
    constexpr std::string_view workbookRelationshipsPath = "xl/_rels/workbook.xml.rels";
    constexpr std::string_view officeDocumentRelationshipPrefix =
      "http://schemas.openxmlformats.org/officeDocument/2006/relationships/";
    constexpr std::string_view worksheetDirectory = "xl/worksheets/";
    constexpr std::string_view worksheetExtension = ".xml";
    constexpr char cellSeparator = '\t';

    struct WorksheetText
    {
      std::string label;
      std::string text;
    };

    struct WorkbookSheet
    {
      std::string name;
      std::string relationshipId;
    };

    struct WorkbookRelationship
    {
      std::string id;
      std::string target;
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
    bool isWorksheetEntry(const archive::ZipArchiveEntry& entry)
    {
      return !entry.directory &&
              entry.rawName.starts_with(worksheetDirectory) &&
              entry.rawName.ends_with(worksheetExtension);
    }

    [[nodiscard]]
    std::optional<std::size_t> parseSize(std::string_view text)
    {
      std::size_t value = 0;
      const auto* begin = text.data();
      const auto* end = text.data() + text.size();
      const auto result = std::from_chars(begin, end, value);

      if (result.ec != std::errc{} || result.ptr != end)
        return std::nullopt;

      return value;
    }

    [[nodiscard]]
    std::string normalizedRelationshipTarget(std::string target)
    {
      constexpr std::string_view workbookBaseDirectory = "xl/";

      if (target.empty())
        return {};

      if (target.starts_with('/'))
        target.erase(0, 1);
      else if (!target.starts_with(workbookBaseDirectory))
        target = std::string{workbookBaseDirectory} + target;

      return target;
    }

    void appendCellValue(std::vector<std::string>& rowValues, std::string value)
    {
      xml::trimTrailingAsciiWhitespace(value);

      if (value.empty())
        return;

      rowValues.push_back(std::move(value));
    }

    void appendRow(std::string& sheetText, std::vector<std::string>& rowValues)
    {
      if (rowValues.empty())
        return;

      if (!sheetText.empty())
        sheetText.push_back('\n');

      for (std::size_t index = 0; index < rowValues.size(); ++index) {
        if (index > 0)
          sheetText.push_back(cellSeparator);

        sheetText += rowValues[index];
      }

      rowValues.clear();
    }

    [[nodiscard]]
    std::vector<WorkbookSheet> workbookSheets(std::string_view xmlText)
    {
      std::vector<WorkbookSheet> sheets;
      std::size_t offset = 0;

      while (offset < xmlText.size()) {
        const auto tagStart = xmlText.find('<', offset);

        if (tagStart == std::string_view::npos)
          break;

        const auto tagEnd = xmlText.find('>', tagStart + 1);

        if (tagEnd == std::string_view::npos)
          break;

        const auto tagContent = xmlText.substr(tagStart + 1, tagEnd - tagStart - 1);

        if (!xml::isClosingTag(tagContent) && xml::localNameFromTag(tagContent) == "sheet") {
          auto name = xml::attributeValue(tagContent, "name").value_or("");
          auto relationshipId = xml::attributeValue(tagContent, "id").value_or("");

          if (!name.empty())
            sheets.push_back({.name = std::move(name), .relationshipId = std::move(relationshipId)});
        }

        offset = tagEnd + 1;
      }

      return sheets;
    }

    [[nodiscard]]
    std::vector<WorkbookRelationship> workbookRelationships(std::string_view xmlText)
    {
      std::vector<WorkbookRelationship> relationships;
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
          const auto worksheetRelationshipType = std::string{officeDocumentRelationshipPrefix} + "worksheet";

          if (!id.empty() && type == worksheetRelationshipType && !target.empty()) {
            relationships.push_back({
              .id = std::move(id),
              .target = normalizedRelationshipTarget(std::move(target)),
            });
          }
        }

        offset = tagEnd + 1;
      }

      return relationships;
    }

    [[nodiscard]]
    std::vector<std::string> sharedStrings(std::string_view xmlText)
    {
      std::vector<std::string> values;
      std::string current;
      bool insideStringItem = false;
      bool insideText = false;
      std::size_t offset = 0;

      while (offset < xmlText.size()) {
        const auto tagStart = xmlText.find('<', offset);

        if (tagStart == std::string_view::npos) {
          if (insideText)
            xml::appendDecodedText(xmlText.substr(offset), current);

          break;
        }

        if (insideText)
          xml::appendDecodedText(xmlText.substr(offset, tagStart - offset), current);

        const auto tagEnd = xmlText.find('>', tagStart + 1);

        if (tagEnd == std::string_view::npos)
          break;

        const auto tagContent = xmlText.substr(tagStart + 1, tagEnd - tagStart - 1);
        const auto tagName = xml::localNameFromTag(tagContent);
        const auto closingTag = xml::isClosingTag(tagContent);

        if (tagName == "si") {
          if (closingTag) {
            xml::trimTrailingAsciiWhitespace(current);
            values.push_back(std::move(current));
            current = {};
            insideStringItem = false;
          } else {
            insideStringItem = true;
          }
        } else if (insideStringItem && tagName == "t") {
          insideText = !closingTag && !xml::isSelfClosingTag(tagContent);
        }

        offset = tagEnd + 1;
      }

      return values;
    }

    [[nodiscard]]
    std::string resolveCellValue(
      std::string cellType,
      std::string rawValue,
      std::string formulaValue,
      const std::vector<std::string>& sharedStringValues)
    {
      xml::trimTrailingAsciiWhitespace(rawValue);
      xml::trimTrailingAsciiWhitespace(formulaValue);

      if (cellType == "b") {
        if (rawValue == "1")
          return "TRUE";

        if (rawValue == "0")
          return "FALSE";
      }

      if (cellType == "s") {
        const auto sharedStringIndex = parseSize(rawValue);

        if (!sharedStringIndex || *sharedStringIndex >= sharedStringValues.size())
          return {};

        return sharedStringValues[*sharedStringIndex];
      }

      if (cellType == "str" || cellType == "inlineStr" || cellType == "d" || cellType == "e")
        return rawValue;

      if (!formulaValue.empty() && !rawValue.empty())
        return formulaValue + " = " + rawValue;

      if (!formulaValue.empty())
        return formulaValue;

      return rawValue;
    }

    [[nodiscard]]
    std::string worksheetText(
      std::string_view xmlText,
      const std::vector<std::string>& sharedStringValues,
      std::stop_token stopToken)
    {
      std::string sheetText;
      std::vector<std::string> rowValues;
      std::string currentCellType;
      std::string currentCellValue;
      std::string currentFormulaValue;
      bool insideRow = false;
      bool insideValue = false;
      bool insideFormula = false;
      bool insideInlineText = false;
      std::size_t offset = 0;

      while (offset < xmlText.size()) {
        if (stopToken.stop_requested())
          break;

        const auto tagStart = xmlText.find('<', offset);

        if (tagStart == std::string_view::npos) {
          if (insideFormula)
            xml::appendDecodedText(xmlText.substr(offset), currentFormulaValue);
          else if (insideValue || insideInlineText)
            xml::appendDecodedText(xmlText.substr(offset), currentCellValue);

          break;
        }

        if (insideFormula)
          xml::appendDecodedText(xmlText.substr(offset, tagStart - offset), currentFormulaValue);
        else if (insideValue || insideInlineText)
          xml::appendDecodedText(xmlText.substr(offset, tagStart - offset), currentCellValue);

        const auto tagEnd = xmlText.find('>', tagStart + 1);

        if (tagEnd == std::string_view::npos)
          break;

        const auto tagContent = xmlText.substr(tagStart + 1, tagEnd - tagStart - 1);
        const auto tagName = xml::localNameFromTag(tagContent);
        const auto closingTag = xml::isClosingTag(tagContent);

        if (tagName == "row") {
          if (closingTag) {
            appendRow(sheetText, rowValues);
            insideRow = false;
          } else {
            insideRow = true;
          }
        } else if (insideRow && tagName == "c") {
          if (closingTag) {
            appendCellValue(
              rowValues,
              resolveCellValue(currentCellType, currentCellValue, currentFormulaValue, sharedStringValues));
            currentCellType = {};
            currentCellValue = {};
            currentFormulaValue = {};
          } else {
            currentCellType = xml::attributeValue(tagContent, "t").value_or("");
            currentCellValue = {};
            currentFormulaValue = {};
          }
        } else if (insideRow && tagName == "v") {
          insideValue = !closingTag && !xml::isSelfClosingTag(tagContent);
        } else if (insideRow && tagName == "f") {
          insideFormula = !closingTag && !xml::isSelfClosingTag(tagContent);
        } else if (insideRow && tagName == "t") {
          insideInlineText = !closingTag && !xml::isSelfClosingTag(tagContent);
        }

        offset = tagEnd + 1;
      }

      xml::trimTrailingAsciiWhitespace(sheetText);

      return sheetText;
    }

    [[nodiscard]]
    std::string defaultSheetLabel(std::size_t index)
    {
      return "sheet" + std::to_string(index + 1);
    }

    [[nodiscard]]
    std::map<std::string, const archive::ZipArchiveEntry*> worksheetEntryMap(
      const std::vector<const archive::ZipArchiveEntry*>& entries)
    {
      std::map<std::string, const archive::ZipArchiveEntry*> mappedEntries;

      for (const auto* entry : entries) {
        mappedEntries.emplace(entry->rawName, entry);
      }

      return mappedEntries;
    }

    void appendWorkbookOrderedWorksheets(
      std::vector<WorksheetText>& worksheets,
      const std::vector<WorkbookSheet>& sheets,
      const std::vector<WorkbookRelationship>& relationships,
      const std::map<std::string, const archive::ZipArchiveEntry*>& availableEntries,
      const std::vector<std::string>& sharedStringValues,
      const archive::ZipArchiveReader& reader,
      const std::filesystem::path& path,
      std::stop_token stopToken,
      DocumentExtractionSummary& summary)
    {
      for (std::size_t index = 0; index < sheets.size(); ++index) {
        const auto& sheet = sheets[index];
        const auto relationship =
          std::ranges::find(relationships, sheet.relationshipId, &WorkbookRelationship::id);

        if (relationship == relationships.end())
          continue;

        const auto entry = availableEntries.find(relationship->target);

        if (entry == availableEntries.end())
          continue;

        const auto entryRead = reader.readEntry(path, *entry->second, {}, stopToken);

        if (entryRead.status != archive::ZipArchiveReadStatus::completed) {
          summary.status = statusFromZipStatus(entryRead.status);
          summary.error = entryRead.error;

          return;
        }

        worksheets.push_back({
          .label = sheet.name.empty() ? defaultSheetLabel(index) : sheet.name,
          .text = worksheetText(bytesToString(entryRead.bytes), sharedStringValues, stopToken),
        });
      }
    }

  } // namespace

  std::string_view XlsxDocumentExtractor::name() const
  {
    return extractorName;
  }

  bool XlsxDocumentExtractor::supports(const std::filesystem::path& path) const
  {
    return xml::lowerAscii(path.extension().string()) == ".xlsx";
  }

  DocumentExtractionSummary XlsxDocumentExtractor::extract(
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

    std::vector<std::string> sharedStringValues;

    // Shared strings are the common storage for repeated textual cell values in XLSX packages.
    if (const auto* sharedStringsEntry = findEntry(catalog, sharedStringsPath)) {
      const auto entryRead = reader.readEntry(path, *sharedStringsEntry, {}, stopToken);

      if (entryRead.status != archive::ZipArchiveReadStatus::completed) {
        summary.status = statusFromZipStatus(entryRead.status);
        summary.error = entryRead.error;

        return summary;
      }

      sharedStringValues = sharedStrings(bytesToString(entryRead.bytes));
    }

    std::vector<WorkbookSheet> sheets;

    // Workbook metadata gives user-facing sheet names; worksheet files remain the source of cell text.
    if (const auto* workbookEntry = findEntry(catalog, workbookPath)) {
      const auto entryRead = reader.readEntry(path, *workbookEntry, {}, stopToken);

      if (entryRead.status != archive::ZipArchiveReadStatus::completed) {
        summary.status = statusFromZipStatus(entryRead.status);
        summary.error = entryRead.error;

        return summary;
      }

      sheets = workbookSheets(bytesToString(entryRead.bytes));
    }

    std::vector<WorkbookRelationship> relationships;

    if (const auto* relationshipsEntry = findEntry(catalog, workbookRelationshipsPath)) {
      const auto entryRead = reader.readEntry(path, *relationshipsEntry, {}, stopToken);

      if (entryRead.status != archive::ZipArchiveReadStatus::completed) {
        summary.status = statusFromZipStatus(entryRead.status);
        summary.error = entryRead.error;

        return summary;
      }

      relationships = workbookRelationships(bytesToString(entryRead.bytes));
    }

    std::vector<const archive::ZipArchiveEntry*> worksheetEntries;

    // Worksheet entries are processed deterministically so repeated indexes produce stable segment ordering.
    for (const auto& entry : catalog.entries) {
      if (isWorksheetEntry(entry))
        worksheetEntries.push_back(&entry);
    }

    std::ranges::sort(worksheetEntries, {}, [](const auto* entry) { return entry->rawName; });

    if (worksheetEntries.empty()) {
      summary.status = DocumentExtractionStatus::parserFailed;

      return summary;
    }

    std::uintmax_t totalBytes = 0;
    std::vector<WorksheetText> worksheets;

    if (!sheets.empty() && !relationships.empty()) {
      appendWorkbookOrderedWorksheets(
        worksheets,
        sheets,
        relationships,
        worksheetEntryMap(worksheetEntries),
        sharedStringValues,
        reader,
        path,
        stopToken,
        summary);

      if (summary.status != DocumentExtractionStatus::completed)
        return summary;
    }

    if (worksheets.empty()) {
      for (std::size_t index = 0; index < worksheetEntries.size(); ++index) {
        if (stopToken.stop_requested()) {
          summary.status = DocumentExtractionStatus::cancelled;

          return summary;
        }

        const auto entryRead = reader.readEntry(path, *worksheetEntries[index], {}, stopToken);

        if (entryRead.status != archive::ZipArchiveReadStatus::completed) {
          summary.status = statusFromZipStatus(entryRead.status);
          summary.error = entryRead.error;

          return summary;
        }

        const auto hasWorkbookSheetName = index < sheets.size() && !sheets[index].name.empty();

        worksheets.push_back({
          .label = hasWorkbookSheetName ? sheets[index].name : defaultSheetLabel(index),
          .text = worksheetText(bytesToString(entryRead.bytes), sharedStringValues, stopToken),
        });
      }
    }

    for (std::size_t index = 0; index < worksheets.size(); ++index) {
      if (stopToken.stop_requested()) {
        summary.status = DocumentExtractionStatus::cancelled;

        return summary;
      }

      auto& worksheet = worksheets[index];

      if (worksheet.text.empty())
        continue;

      totalBytes += worksheet.text.size();

      const auto nextSegmentCount = summary.segmentsExtracted + 1;

      if (wouldExceedByteLimit(options, totalBytes) || wouldExceedSegmentLimit(options, nextSegmentCount)) {
        summary.status = DocumentExtractionStatus::safetyLimitExceeded;

        return summary;
      }

      ExtractedTextSegment segment;

      segment.text = std::move(worksheet.text);
      segment.location.kind = DocumentLocationKind::sheet;
      segment.location.primary = index + 1;
      segment.location.label = std::move(worksheet.label);

      if (!sink(segment)) {
        summary.status = DocumentExtractionStatus::cancelled;

        return summary;
      }

      ++summary.segmentsExtracted;
    }

    summary.bytesExtracted = totalBytes;

    return summary;
  }

} // namespace uburu::document
