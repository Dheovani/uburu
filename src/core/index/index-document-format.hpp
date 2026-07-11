#pragma once

#include "shared/types/domain-types.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace uburu::index
{

  /**
   * Describes the current persisted index document format contract.
   */
  struct IndexDocumentFormatDescriptor
  {
    std::uint32_t version{latestIndexDocumentFormatVersion};
    ContentHashAlgorithm contentHashAlgorithm{ContentHashAlgorithm::sha256};
    bool contentAddressed{true};
    bool storesGitBlobHash{true};
    bool storesWorkingTreeOverlay{true};
  };

  [[nodiscard]]
  IndexDocumentFormatDescriptor currentIndexDocumentFormat();

  [[nodiscard]]
  bool isSupportedIndexDocumentFormatVersion(std::uint32_t version);

  /**
   * Validates document metadata before it is persisted or reused from storage.
   */
  [[nodiscard]]
  std::optional<std::string> validateIndexDocumentFormat(const IndexDocument& document);

} // namespace uburu::index
