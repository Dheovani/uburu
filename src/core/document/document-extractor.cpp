#include "core/document/document-extractor.hpp"

#include <utility>

namespace uburu::document
{

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
