#include "core/document/document-extractor.hpp"

#include <utility>

namespace uburu::document
{

  std::string_view documentExtractionStatusName(DocumentExtractionStatus status)
  {
    switch (status) {
    case DocumentExtractionStatus::completed:
      return "completed";
    case DocumentExtractionStatus::cancelled:
      return "cancelled";
    case DocumentExtractionStatus::unsupportedFormat:
      return "unsupportedFormat";
    case DocumentExtractionStatus::openFailed:
      return "openFailed";
    case DocumentExtractionStatus::readFailed:
      return "readFailed";
    case DocumentExtractionStatus::binarySkipped:
      return "binarySkipped";
    case DocumentExtractionStatus::invalidEncoding:
      return "invalidEncoding";
    case DocumentExtractionStatus::safetyLimitExceeded:
      return "safetyLimitExceeded";
    case DocumentExtractionStatus::parserFailed:
      return "parserFailed";
    case DocumentExtractionStatus::encryptedOrProtected:
      return "encryptedOrProtected";
    }

    return "unknown";
  }

  DocumentContentAvailability documentContentAvailability(DocumentExtractionStatus status)
  {
    switch (status) {
    case DocumentExtractionStatus::completed:
      return DocumentContentAvailability::contentAvailable;
    case DocumentExtractionStatus::unsupportedFormat:
      return DocumentContentAvailability::nameOnlyUnsupported;
    case DocumentExtractionStatus::binarySkipped:
      return DocumentContentAvailability::nameOnlyBinary;
    case DocumentExtractionStatus::safetyLimitExceeded:
    case DocumentExtractionStatus::invalidEncoding:
      return DocumentContentAvailability::nameOnlySafetyLimited;
    case DocumentExtractionStatus::encryptedOrProtected:
      return DocumentContentAvailability::nameOnlyProtected;
    case DocumentExtractionStatus::cancelled:
      return DocumentContentAvailability::cancelled;
    case DocumentExtractionStatus::openFailed:
    case DocumentExtractionStatus::readFailed:
    case DocumentExtractionStatus::parserFailed:
      return DocumentContentAvailability::extractionFailed;
    }

    return DocumentContentAvailability::extractionFailed;
  }

  std::string_view documentContentAvailabilityName(DocumentContentAvailability availability)
  {
    switch (availability) {
    case DocumentContentAvailability::contentAvailable:
      return "contentAvailable";
    case DocumentContentAvailability::nameOnlyUnsupported:
      return "nameOnlyUnsupported";
    case DocumentContentAvailability::nameOnlyBinary:
      return "nameOnlyBinary";
    case DocumentContentAvailability::nameOnlySafetyLimited:
      return "nameOnlySafetyLimited";
    case DocumentContentAvailability::nameOnlyProtected:
      return "nameOnlyProtected";
    case DocumentContentAvailability::extractionFailed:
      return "extractionFailed";
    case DocumentContentAvailability::cancelled:
      return "cancelled";
    }

    return "unknown";
  }

  bool isNameOnlySearchable(DocumentContentAvailability availability)
  {
    return availability == DocumentContentAvailability::nameOnlyUnsupported ||
           availability == DocumentContentAvailability::nameOnlyBinary ||
           availability == DocumentContentAvailability::nameOnlySafetyLimited ||
           availability == DocumentContentAvailability::nameOnlyProtected;
  }

  void DocumentExtractorRegistry::add(std::shared_ptr<const DocumentExtractor> extractor)
  {
    if (!extractor)
      return;

    extractors.push_back(std::move(extractor));
  }

  const DocumentExtractor* DocumentExtractorRegistry::findExtractor(const std::filesystem::path& path) const
  {
    for (const auto& extractor : extractors) {
      if (extractor->supports(path))
        return extractor.get();
    }

    return nullptr;
  }

  DocumentExtractionSummary DocumentExtractorRegistry::extract(const std::filesystem::path& path,
                                                               const DocumentExtractionOptions& options,
                                                               const ExtractedTextSink& sink,
                                                               std::stop_token stopToken) const
  {
    const auto* extractor = findExtractor(path);

    if (!extractor) {
      DocumentExtractionSummary summary;

      summary.status = DocumentExtractionStatus::unsupportedFormat;

      return summary;
    }

    return extractor->extract(path, options, sink, stopToken);
  }

} // namespace uburu::document
