#include "core/document/plain-text-extractor.hpp"

#include "core/text/text-file-reader.hpp"

#include <string_view>

namespace uburu::document
{
  namespace
  {

    [[nodiscard]]
    DocumentExtractionStatus extractionStatusFromTextStatus(text::TextReadStatus status)
    {
      switch (status) {
      case text::TextReadStatus::completed:
        return DocumentExtractionStatus::completed;
      case text::TextReadStatus::cancelled:
        return DocumentExtractionStatus::cancelled;
      case text::TextReadStatus::openFailed:
        return DocumentExtractionStatus::openFailed;
      case text::TextReadStatus::readFailed:
        return DocumentExtractionStatus::readFailed;
      case text::TextReadStatus::binarySkipped:
        return DocumentExtractionStatus::binarySkipped;
      case text::TextReadStatus::invalidEncoding:
        return DocumentExtractionStatus::invalidEncoding;
      case text::TextReadStatus::lineTooLong:
        return DocumentExtractionStatus::safetyLimitExceeded;
      }

      return DocumentExtractionStatus::parserFailed;
    }

    [[nodiscard]]
    bool wouldExceedByteLimit(const DocumentExtractionOptions& options,
                              std::uintmax_t bytesExtracted,
                              std::size_t nextSegmentBytes)
    {
      return options.maximumExtractedBytes > 0 &&
             bytesExtracted + static_cast<std::uintmax_t>(nextSegmentBytes) > options.maximumExtractedBytes;
    }

    [[nodiscard]]
    bool reachedSegmentLimit(const DocumentExtractionOptions& options, std::size_t segmentsExtracted)
    {
      return options.maximumSegments > 0 && segmentsExtracted >= options.maximumSegments;
    }

  } // namespace

  std::string_view PlainTextExtractor::name() const
  {
    return "plain-text";
  }

  bool PlainTextExtractor::supports(const std::filesystem::path&) const
  {
    return true;
  }

  DocumentExtractionSummary PlainTextExtractor::extract(const std::filesystem::path& path,
                                                        const DocumentExtractionOptions& options,
                                                        const ExtractedTextSink& sink,
                                                        std::stop_token stopToken) const
  {
    DocumentExtractionSummary extractionSummary;

    const auto readSummary = text::readTextFileLines(
      path,
      options.textOptions,
      [&](const text::TextLine& line) {
        if (wouldExceedByteLimit(options, extractionSummary.bytesExtracted, line.text.size()) ||
            reachedSegmentLimit(options, extractionSummary.segmentsExtracted)) {
          extractionSummary.status = DocumentExtractionStatus::safetyLimitExceeded;

          return false;
        }

        ExtractedTextSegment segment;

        segment.text = line.text;
        segment.location.kind = DocumentLocationKind::line;
        segment.location.primary = line.lineNumber;
        segment.byteOffset = line.byteOffset;

        if (!sink(segment)) {
          extractionSummary.status = DocumentExtractionStatus::cancelled;

          return false;
        }

        ++extractionSummary.segmentsExtracted;
        extractionSummary.bytesExtracted += line.text.size();

        return true;
      },
      stopToken);

    extractionSummary.encoding = readSummary.encoding;
    extractionSummary.error = readSummary.error;
    extractionSummary.hadBom = readSummary.hadBom;
    extractionSummary.hadInvalidSequences = readSummary.hadInvalidSequences;

    if (extractionSummary.status == DocumentExtractionStatus::completed)
      extractionSummary.status = extractionStatusFromTextStatus(readSummary.status);

    return extractionSummary;
  }

} // namespace uburu::document
