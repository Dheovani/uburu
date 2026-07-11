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

  enum class DocumentContentAvailability
  {
    contentAvailable,
    nameOnlyUnsupported,
    nameOnlyBinary,
    nameOnlySafetyLimited,
    nameOnlyProtected,
    extractionFailed,
    cancelled
  };

  /**
   * Identifies where an extracted text segment came from inside a document.
   */
  struct DocumentLocation
  {
    DocumentLocationKind kind{DocumentLocationKind::none};
    std::size_t primary{0};
    std::size_t secondary{0};
    std::string label;
  };

  /**
   * Carries one extracted text unit to direct search, indexing, or preview.
   */
  struct ExtractedTextSegment
  {
    std::string text;
    DocumentLocation location;
    std::uintmax_t byteOffset{0};
  };

  /**
   * Provides safety and text-reading limits to document extractors.
   */
  struct DocumentExtractionOptions
  {
    SearchOptions textOptions;
    std::uintmax_t maximumExtractedBytes{0};
    std::size_t maximumSegments{0};
  };

  /**
   * Summarizes extraction work for diagnostics, indexing decisions, and UI status.
   */
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

  /**
   * Returns a stable diagnostic name for an extraction status.
   */
  [[nodiscard]]
  std::string_view documentExtractionStatusName(DocumentExtractionStatus status);

  /**
   * Converts an extraction status into the corresponding search/indexing availability.
   */
  [[nodiscard]]
  DocumentContentAvailability documentContentAvailability(DocumentExtractionStatus status);

  /**
   * Returns a stable diagnostic name for a content availability value.
   */
  [[nodiscard]]
  std::string_view documentContentAvailabilityName(DocumentContentAvailability availability);

  /**
   * Returns whether a file should still be searchable by path/name when content is unavailable.
   */
  [[nodiscard]]
  bool isNameOnlySearchable(DocumentContentAvailability availability);

  class DocumentExtractor
  {
  public:
    virtual ~DocumentExtractor() = default;

    /**
     * Returns the stable extractor name used by metrics and diagnostics.
     */
    [[nodiscard]]
    virtual std::string_view name() const = 0;

    /**
     * Checks whether this extractor can handle the given path.
     */
    [[nodiscard]]
    virtual bool supports(const std::filesystem::path& path) const = 0;

    /**
     * Streams extracted text segments through the sink using cooperative cancellation.
     */
    [[nodiscard]]
    virtual DocumentExtractionSummary extract(
      const std::filesystem::path& path,
      const DocumentExtractionOptions& options,
      const ExtractedTextSink& sink,
      std::stop_token stopToken = {}) const = 0;
  };

  class DocumentExtractorRegistry
  {
  public:
    /**
     * Adds an extractor to the registry in priority order.
     */
    void add(std::shared_ptr<const DocumentExtractor> extractor);

    /**
     * Finds the first extractor that supports the given path.
     */
    [[nodiscard]]
    const DocumentExtractor* findExtractor(const std::filesystem::path& path) const;

    /**
     * Extracts a file through the first supporting extractor.
     */
    [[nodiscard]]
    DocumentExtractionSummary extract(
      const std::filesystem::path& path,
      const DocumentExtractionOptions& options,
      const ExtractedTextSink& sink,
      std::stop_token stopToken = {}) const;

  private:
    std::vector<std::shared_ptr<const DocumentExtractor>> extractors;
  };

} // namespace uburu::document
