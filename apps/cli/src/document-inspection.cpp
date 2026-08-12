#include "document-inspection.hpp"

#include "core/document/default-document-extractors.hpp"
#include "core/filesystem/recursive-file-scanner.hpp"

namespace uburu::cli
{

  DocumentInspectionSummary inspectDocuments(
    const std::filesystem::path& root,
    const SearchOptions& options,
    std::stop_token stopToken)
  {
    DocumentInspectionSummary summary;
    filesystem::RecursiveFileScanner scanner;

    scanner.scan(
      root,
      options,
      [&](FileEntry file) {
        if (stopToken.stop_requested()) {
          summary.cancelled = true;

          return false;
        }

        ++summary.filesScanned;
        summary.sourceBytes += file.size;

        const auto* extractor = document::defaultDocumentExtractorRegistry().findExtractor(file.absolutePath);

        if (extractor == nullptr) {
          ++summary.statuses[document::DocumentExtractionStatus::unsupportedFormat];

          return true;
        }

        ++summary.supportedFiles;

        document::DocumentExtractionOptions extractionOptions;
        extractionOptions.textOptions = options;

        const auto extraction = extractor->extract(
          file.absolutePath,
          extractionOptions,
          [](const document::ExtractedTextSegment&) { return true; },
          stopToken);

        ++summary.statuses[extraction.status];

        if (extraction.issue != document::DocumentExtractionIssue::none)
          ++summary.issues[extraction.issue];

        if (extraction.status == document::DocumentExtractionStatus::cancelled) {
          summary.cancelled = true;

          return false;
        }

        return true;
      },
      stopToken);

    summary.cancelled = summary.cancelled || stopToken.stop_requested();

    return summary;
  }

} // namespace uburu::cli
