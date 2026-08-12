#pragma once

#include "core/document/document-extractor.hpp"

namespace uburu::document
{

  /**
   * Returns the shared registry for structured formats handled before plain-text reading.
   */
  [[nodiscard]]
  const DocumentExtractorRegistry& structuredDocumentExtractorRegistry();

  /**
   * Returns the shared registry containing every built-in document extractor.
   */
  [[nodiscard]]
  const DocumentExtractorRegistry& defaultDocumentExtractorRegistry();

} // namespace uburu::document
