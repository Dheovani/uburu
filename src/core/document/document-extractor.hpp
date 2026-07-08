#pragma once

#include "shared/types/domain-types.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace uburu::document
{

  enum class DocumentExtractionStatus
  {
    completed,
    cancelled,
    unsupportedFormat,
    openFailed,
    readFailed,
    binarySkipped,
    invalidEncoding,
    safetyLimitExceeded,
    parserFailed,
    encryptedOrProtected
  };

  enum class DocumentLocationKind
  {
    none,
    line,
    page,
    sheet,
    slide,
    timestamp
  };

  struct DocumentLocation
  {
    DocumentLocationKind kind{DocumentLocationKind::none};
    std::size_t primary{0};
    std::size_t secondary{0};
    std::string label;
  };

  struct ExtractedTextSegment
  {
    std::string text;
    DocumentLocation location;
    std::uintmax_t byteOffset{0};
  };

  struct DocumentExtractionOptions
  {
    SearchOptions textOptions;
    std::uintmax_t maximumExtractedBytes{0};
    std::size_t maximumSegments{0};
  };

  struct DocumentExtractionSummary
  {
    DocumentExtractionStatus status{DocumentExtractionStatus::completed};
    TextEncoding encoding{TextEncoding::utf8};
    std::error_code error;
    std::size_t segmentsExtracted{0};
    std::uintmax_t bytesExtracted{0};
    bool hadBom{false};
    bool hadInvalidSequences{false};
  };

  using ExtractedTextSink = std::function<bool(const ExtractedTextSegment&)>;

  class DocumentExtractor
  {
  public:
    virtual ~DocumentExtractor() = default;

    [[nodiscard]]
    virtual std::string_view name() const = 0;

    [[nodiscard]]
    virtual bool supports(const std::filesystem::path& path) const = 0;

    [[nodiscard]]
    virtual DocumentExtractionSummary extract(const std::filesystem::path& path,
                                              const DocumentExtractionOptions& options,
                                              const ExtractedTextSink& sink,
                                              std::stop_token stopToken = {}) const = 0;
  };

  class DocumentExtractorRegistry
  {
  public:
    void add(std::shared_ptr<const DocumentExtractor> extractor);

    [[nodiscard]]
    const DocumentExtractor* findExtractor(const std::filesystem::path& path) const;

    [[nodiscard]]
    DocumentExtractionSummary extract(const std::filesystem::path& path,
                                      const DocumentExtractionOptions& options,
                                      const ExtractedTextSink& sink,
                                      std::stop_token stopToken = {}) const;

  private:
    std::vector<std::shared_ptr<const DocumentExtractor>> extractors;
  };

} // namespace uburu::document
