#pragma once

#include "core/document/document-extractor.hpp"

namespace uburu::document
{

  class SubtitleDocumentExtractor final : public DocumentExtractor
  {
  public:
    /**
     * Returns the stable extractor name used by metrics and diagnostics.
     */
    [[nodiscard]]
    std::string_view name() const override;

    /**
     * Checks whether the file extension is supported by the subtitle extractor.
     */
    [[nodiscard]]
    bool supports(const std::filesystem::path& path) const override;

    /**
     * Streams subtitle cue text with timestamp-oriented locations when available.
     */
    [[nodiscard]]
    DocumentExtractionSummary extract(
      const std::filesystem::path& path,
      const DocumentExtractionOptions& options,
      const ExtractedTextSink& sink,
      std::stop_token stopToken = {}) const override;
  };

} // namespace uburu::document
