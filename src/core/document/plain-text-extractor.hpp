#pragma once

#include "core/document/document-extractor.hpp"

namespace uburu::document
{

  class PlainTextExtractor final : public DocumentExtractor
  {
  public:
    /**
     * Returns the stable extractor name used by metrics and diagnostics.
     */
    [[nodiscard]]
    std::string_view name() const override;

    /**
     * Checks whether the path should be handled as plain text.
     */
    [[nodiscard]]
    bool supports(const std::filesystem::path& path) const override;

    /**
     * Streams line-based text segments from a plain text file.
     */
    [[nodiscard]]
    DocumentExtractionSummary extract(
      const std::filesystem::path& path,
      const DocumentExtractionOptions& options,
      const ExtractedTextSink& sink,
      std::stop_token stopToken = {}) const override;
  };

} // namespace uburu::document
