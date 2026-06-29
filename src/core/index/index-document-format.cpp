#include "core/index/index-document-format.hpp"

namespace uburu::index
{
  namespace
  {

    constexpr std::uint32_t oldestSupportedIndexDocumentFormatVersion = 1;

  } // namespace

  IndexDocumentFormatDescriptor currentIndexDocumentFormat()
  {
    return IndexDocumentFormatDescriptor{};
  }

  bool isSupportedIndexDocumentFormatVersion(std::uint32_t version)
  {
    return version >= oldestSupportedIndexDocumentFormatVersion && version <= currentIndexDocumentFormatVersion;
  }

  std::optional<std::string> validateIndexDocumentFormat(const IndexDocument& document)
  {
    if (!isSupportedIndexDocumentFormatVersion(document.formatVersion))
      return "unsupported index document format version";

    if (document.contentHashAlgorithm != ContentHashAlgorithm::sha256)
      return "unsupported index content hash algorithm";

    if (document.contentHash.empty())
      return "index document content hash is empty";

    return std::nullopt;
  }

} // namespace uburu::index
