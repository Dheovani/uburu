#pragma once

#include "core/document/document-extractor.hpp"

namespace uburu::document
{

  class RtfDocumentExtractor final : public DocumentExtractor
  {
  public:
    [[nodiscard]]
    std::string_view name() const override;

    [[nodiscard]]
    bool supports(const std::filesystem::path& path) const override;

    [[nodiscard]]
    DocumentExtractionSummary extract(
      const std::filesystem::path& path,
      const DocumentExtractionOptions& options,
      const ExtractedTextSink& sink,
      std::stop_token stopToken = {}) const override;
  };

} // namespace uburu::document
