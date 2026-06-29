 #pragma once

#include "shared/types/domain-types.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace uburu::index
{

  struct IndexDocumentFormatDescriptor
  {
    std::uint32_t version{currentIndexDocumentFormatVersion};
    ContentHashAlgorithm contentHashAlgorithm{ContentHashAlgorithm::sha256};
    bool contentAddressed{true};
    bool storesGitBlobHash{true};
    bool storesWorkingTreeOverlay{true};
  };

  [[nodiscard]] IndexDocumentFormatDescriptor currentIndexDocumentFormat();
  [[nodiscard]] bool isSupportedIndexDocumentFormatVersion(std::uint32_t version);
  [[nodiscard]] std::optional<std::string> validateIndexDocumentFormat(const IndexDocument& document);

} // namespace uburu::index
