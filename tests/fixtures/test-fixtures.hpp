#pragma once

#include "helpers/temporary-paths.hpp"

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace uburu::tests::fixtures
{

  inline constexpr std::string_view literalNeedle = "needle";
  inline constexpr std::string_view rootGitIgnoreContent = "*.log\n!important.log\nbuild/\n";
  inline constexpr std::string_view nestedGitIgnoreContent = "*.generated.cpp\n";
  inline constexpr std::uint32_t testZipLocalFileHeaderSignature = 0x0403'4B50U;
  inline constexpr std::uint32_t testZipCentralDirectoryFileHeaderSignature = 0x0201'4B50U;
  inline constexpr std::uint32_t testZipEndOfCentralDirectorySignature = 0x0605'4B50U;
  inline constexpr std::uint16_t testZipVersionNeeded = 20;
  inline constexpr std::uint16_t testZipStoreCompressionMethod = 0;
  inline constexpr std::uint16_t testZipNoFlags = 0;
  inline constexpr std::uint16_t testZipNoTimestamp = 0;
  inline constexpr std::uint32_t testZipNoCrc = 0;
  inline constexpr std::uint16_t testZipNoDisk = 0;
  inline constexpr std::uint16_t testZipNoAttributes = 0;
  inline constexpr std::uint32_t testZipNoExternalAttributes = 0;

  struct StoredZipEntryFixture
  {
    std::string name;
    std::string content;
  };

  /**
   * Returns Portuguese text encoded with precomposed Unicode scalars.
   */
  inline std::string portuguesePrecomposedText()
  {
    return "a gera\xC3\xA7\xC3\xA3o e a corrup\xC3\xA7\xC3\xA3o da mat\xC3\xA9ria";
  }

  /**
   * Returns the same Portuguese text using decomposed combining marks.
   */
  inline std::string portugueseDecomposedText()
  {
    return "a gerac\xCC\xA7"
           "a\xCC\x83"
           "o e a corrupc\xCC\xA7"
           "a\xCC\x83"
           "o da mate\xCC\x81"
           "ria";
  }

  /**
   * Appends one little-endian 16-bit value to a byte buffer.
   */
  inline void appendLittleEndian16(std::vector<unsigned char>& bytes, std::uint16_t value)
  {
    bytes.push_back(static_cast<unsigned char>(value & 0xFFU));
    bytes.push_back(static_cast<unsigned char>((value >> 8U) & 0xFFU));
  }

  /**
   * Appends one little-endian 32-bit value to a byte buffer.
   */
  inline void appendLittleEndian32(std::vector<unsigned char>& bytes, std::uint32_t value)
  {
    bytes.push_back(static_cast<unsigned char>(value & 0xFFU));
    bytes.push_back(static_cast<unsigned char>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<unsigned char>((value >> 16U) & 0xFFU));
    bytes.push_back(static_cast<unsigned char>((value >> 24U) & 0xFFU));
  }

  inline void appendZipText(std::vector<unsigned char>& bytes, std::string_view text)
  {
    for (const auto character : text)
      bytes.push_back(static_cast<unsigned char>(character));
  }

  /**
   * Builds a minimal stored-only ZIP archive for document extractor tests.
   */
  [[nodiscard]]
  inline std::vector<unsigned char> storedZipBytes(std::vector<StoredZipEntryFixture> entries)
  {
    std::vector<unsigned char> bytes;
    std::vector<std::uint32_t> localOffsets;

    for (const auto& entry : entries) {
      localOffsets.push_back(static_cast<std::uint32_t>(bytes.size()));
      appendLittleEndian32(bytes, testZipLocalFileHeaderSignature);
      appendLittleEndian16(bytes, testZipVersionNeeded);
      appendLittleEndian16(bytes, testZipNoFlags);
      appendLittleEndian16(bytes, testZipStoreCompressionMethod);
      appendLittleEndian16(bytes, testZipNoTimestamp);
      appendLittleEndian16(bytes, testZipNoTimestamp);
      appendLittleEndian32(bytes, testZipNoCrc);
      appendLittleEndian32(bytes, static_cast<std::uint32_t>(entry.content.size()));
      appendLittleEndian32(bytes, static_cast<std::uint32_t>(entry.content.size()));
      appendLittleEndian16(bytes, static_cast<std::uint16_t>(entry.name.size()));
      appendLittleEndian16(bytes, 0);
      appendZipText(bytes, entry.name);
      appendZipText(bytes, entry.content);
    }

    const auto centralDirectoryOffset = static_cast<std::uint32_t>(bytes.size());

    for (std::size_t index = 0; index < entries.size(); ++index) {
      const auto& entry = entries[index];
      appendLittleEndian32(bytes, testZipCentralDirectoryFileHeaderSignature);
      appendLittleEndian16(bytes, testZipVersionNeeded);
      appendLittleEndian16(bytes, testZipVersionNeeded);
      appendLittleEndian16(bytes, testZipNoFlags);
      appendLittleEndian16(bytes, testZipStoreCompressionMethod);
      appendLittleEndian16(bytes, testZipNoTimestamp);
      appendLittleEndian16(bytes, testZipNoTimestamp);
      appendLittleEndian32(bytes, testZipNoCrc);
      appendLittleEndian32(bytes, static_cast<std::uint32_t>(entry.content.size()));
      appendLittleEndian32(bytes, static_cast<std::uint32_t>(entry.content.size()));
      appendLittleEndian16(bytes, static_cast<std::uint16_t>(entry.name.size()));
      appendLittleEndian16(bytes, 0);
      appendLittleEndian16(bytes, 0);
      appendLittleEndian16(bytes, testZipNoDisk);
      appendLittleEndian16(bytes, testZipNoAttributes);
      appendLittleEndian32(bytes, testZipNoExternalAttributes);
      appendLittleEndian32(bytes, localOffsets[index]);
      appendZipText(bytes, entry.name);
    }

    const auto centralDirectorySize = static_cast<std::uint32_t>(bytes.size() - centralDirectoryOffset);
    appendLittleEndian32(bytes, testZipEndOfCentralDirectorySignature);
    appendLittleEndian16(bytes, testZipNoDisk);
    appendLittleEndian16(bytes, testZipNoDisk);
    appendLittleEndian16(bytes, static_cast<std::uint16_t>(entries.size()));
    appendLittleEndian16(bytes, static_cast<std::uint16_t>(entries.size()));
    appendLittleEndian32(bytes, centralDirectorySize);
    appendLittleEndian32(bytes, centralDirectoryOffset);
    appendLittleEndian16(bytes, 0);

    return bytes;
  }

  /**
   * Builds a minimal DOCX-like archive containing only the main document XML.
   */
  [[nodiscard]]
  inline std::vector<unsigned char> minimalDocxBytes(std::string_view documentXml)
  {
    return storedZipBytes({StoredZipEntryFixture{.name = "[Content_Types].xml",
                                                 .content = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                                                            "<Types xmlns=\"http://schemas.openxmlformats.org/"
                                                            "package/2006/content-types\"/>"},
                           StoredZipEntryFixture{.name = "word/document.xml", .content = std::string{documentXml}}});
  }

  /**
   * Builds a minimal XLSX-like archive with workbook, shared strings, and one worksheet.
   */
  [[nodiscard]]
  inline std::vector<unsigned char> minimalXlsxBytes(
    std::string_view worksheetXml,
    std::string_view sharedStringsXml,
    std::string_view workbookXml =
      "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
      "<sheets><sheet name=\"Sheet One\" sheetId=\"1\"/></sheets></workbook>")
  {
    return storedZipBytes({
      StoredZipEntryFixture{.name = "[Content_Types].xml",
                            .content = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                                      "<Types xmlns=\"http://schemas.openxmlformats.org/"
                                      "package/2006/content-types\"/>"},
      StoredZipEntryFixture{.name = "xl/workbook.xml", .content = std::string{workbookXml}},
      StoredZipEntryFixture{.name = "xl/sharedStrings.xml",
                            .content = std::string{sharedStringsXml}},
      StoredZipEntryFixture{.name = "xl/worksheets/sheet1.xml",
                            .content = std::string{worksheetXml}}});
  }

  /**
   * Builds a minimal PPTX-like archive with one slide and optional speaker notes.
   */
  [[nodiscard]]
  inline std::vector<unsigned char> minimalPptxBytes(
    std::string_view slideXml,
    std::string_view notesXml = {})
  {
    std::vector<StoredZipEntryFixture> entries{
      StoredZipEntryFixture{.name = "[Content_Types].xml",
                            .content = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                                      "<Types xmlns=\"http://schemas.openxmlformats.org/"
                                      "package/2006/content-types\"/>"},
      StoredZipEntryFixture{.name = "ppt/presentation.xml",
                            .content = "<p:presentation xmlns:p=\"http://schemas.openxmlformats.org/"
                                      "presentationml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/"
                                      "officeDocument/2006/relationships\"><p:sldIdLst><p:sldId id=\"256\" "
                                      "r:id=\"rId1\"/></p:sldIdLst></p:presentation>"},
      StoredZipEntryFixture{.name = "ppt/_rels/presentation.xml.rels",
                            .content = "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/"
                                      "relationships\"><Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats."
                                      "org/officeDocument/2006/relationships/slide\" Target=\"slides/slide1.xml\"/>"
                                      "</Relationships>"},
      StoredZipEntryFixture{.name = "ppt/slides/slide1.xml", .content = std::string{slideXml}},
    };

    if (!notesXml.empty()) {
      entries.push_back(StoredZipEntryFixture{
        .name = "ppt/slides/_rels/slide1.xml.rels",
        .content = "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                  "<Relationship Id=\"rIdNotes\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
                  "relationships/notesSlide\" Target=\"../notesSlides/notesSlide1.xml\"/></Relationships>"});
      entries.push_back(StoredZipEntryFixture{
        .name = "ppt/notesSlides/notesSlide1.xml",
        .content = std::string{notesXml}});
    }

    return storedZipBytes(std::move(entries));
  }

  /**
   * Builds a minimal OpenDocument-like archive with content and optional metadata XML.
   */
  [[nodiscard]]
  inline std::vector<unsigned char> minimalOpenDocumentBytes(
    std::string_view contentXml,
    std::string_view metaXml = {})
  {
    std::vector<StoredZipEntryFixture> entries{
      StoredZipEntryFixture{.name = "content.xml", .content = std::string{contentXml}},
    };

    if (!metaXml.empty()) {
      entries.push_back(StoredZipEntryFixture{.name = "meta.xml", .content = std::string{metaXml}});
    }

    return storedZipBytes(std::move(entries));
  }

  [[nodiscard]]
  inline std::vector<unsigned char> utf8BomMixedLineEndingBytes()
  {
    return {0xEFU, 0xBBU, 0xBFU, 'o', 'n', 'e', '\n', 't', 'w', 'o', '\r',
            '\n',  't',   'h',   'r', 'e', 'e', '\r', 'f', 'o', 'u', 'r'};
  }

  [[nodiscard]]
  inline std::vector<unsigned char> utf16LittleEndianPortugueseBytes()
  {
    return {0xFFU, 0xFEU, 'a', 0x00U, 0xE7U, 0x00U, 0xE3U, 0x00U, 'o', 0x00U};
  }

  [[nodiscard]]
  inline std::vector<unsigned char> utf16BigEndianPortugueseBytes()
  {
    return {0xFEU, 0xFFU, 0x00U, 'a', 0x00U, 0xE7U, 0x00U, 0xE3U, 0x00U, 'o'};
  }

  [[nodiscard]]
  inline std::vector<unsigned char> latin1PortugueseBytes()
  {
    return {'a', 0xE7U, 0xE3U, 'o'};
  }

  [[nodiscard]]
  inline std::vector<unsigned char> binaryTextLikeBytes()
  {
    return {'t', 'e', 'x', 't', 0x00U, 'm', 'o', 'r', 'e'};
  }

  inline void writeRootGitIgnoreFixture(const std::filesystem::path& root)
  {
    writeFile(root / ".gitignore", rootGitIgnoreContent);
    writeFile(root / "debug.log", "ignored\n");
    writeFile(root / "important.log", "kept\n");
    writeFile(root / "build" / "output.txt", "ignored\n");
    writeFile(root / "src" / "main.cpp", "kept\n");
  }

  inline void writeNestedGitIgnoreFixture(const std::filesystem::path& root)
  {
    writeFile(root / "src" / ".gitignore", nestedGitIgnoreContent);
    writeFile(root / "src" / "main.cpp", "kept\n");
    writeFile(root / "src" / "main.generated.cpp", "ignored\n");
    writeFile(root / "tests" / "main.generated.cpp", "kept\n");
  }

  inline void writeBasicGitWorkingTreeFixture(const std::filesystem::path& repositoryRoot)
  {
    writeFile(repositoryRoot / "tracked.txt", "tracked\n");
    writeFile(repositoryRoot / "modify-me.txt", "modify me\n");
    writeFile(repositoryRoot / "delete-me.txt", "delete me\n");
    writeFile(repositoryRoot / ".gitignore", "*.ignored\n");
  }

} // namespace uburu::tests::fixtures
