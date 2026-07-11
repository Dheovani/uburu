#pragma once

#include "core/document/document-extractor.hpp"

namespace uburu::document
{

  class RtfDocumentExtractor final : public DocumentExtractor
  {
  public:
    /**
     * Returns the stable extractor name used by metrics and diagnostics.
     */
    [[nodiscard]]
    std::string_view name() const override;

    /**
     * Checks whether the file extension is supported by the RTF extractor.
     */
    [[nodiscard]]
    bool supports(const std::filesystem::path& path) const override;

    /**
     * Streams visible RTF text while ignoring embedded objects and image payloads.
     */
    [[nodiscard]]
    DocumentExtractionSummary extract(
      const std::filesystem::path& path,
      const DocumentExtractionOptions& options,
      const ExtractedTextSink& sink,
      std::stop_token stopToken = {}) const override;
  };

} // namespace uburu::document
