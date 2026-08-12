#include "core/document/default-document-extractors.hpp"

#include "core/document/docx-document-extractor.hpp"
#include "core/document/html-document-extractor.hpp"
#include "core/document/open-document-extractor.hpp"
#include "core/document/pdf-document-extractor.hpp"
#include "core/document/plain-text-extractor.hpp"
#include "core/document/pptx-document-extractor.hpp"
#include "core/document/rtf-document-extractor.hpp"
#include "core/document/subtitle-document-extractor.hpp"
#include "core/document/xlsx-document-extractor.hpp"

#include <memory>

namespace uburu::document
{
  namespace
  {

    [[nodiscard]]
    DocumentExtractorRegistry makeStructuredRegistry()
    {
      DocumentExtractorRegistry registry;

      registry.add(std::make_shared<DocxDocumentExtractor>());
      registry.add(std::make_shared<HtmlDocumentExtractor>());
      registry.add(std::make_shared<OpenDocumentExtractor>());
      registry.add(std::make_shared<PdfDocumentExtractor>());
      registry.add(std::make_shared<PptxDocumentExtractor>());
      registry.add(std::make_shared<RtfDocumentExtractor>());
      registry.add(std::make_shared<SubtitleDocumentExtractor>());
      registry.add(std::make_shared<XlsxDocumentExtractor>());

      return registry;
    }

  } // namespace

  const DocumentExtractorRegistry& structuredDocumentExtractorRegistry()
  {
    static const auto registry = makeStructuredRegistry();

    return registry;
  }

  const DocumentExtractorRegistry& defaultDocumentExtractorRegistry()
  {
    static const auto registry = [] {
      auto configuredRegistry = makeStructuredRegistry();

      configuredRegistry.add(std::make_shared<PlainTextExtractor>());

      return configuredRegistry;
    }();

    return registry;
  }

} // namespace uburu::document
