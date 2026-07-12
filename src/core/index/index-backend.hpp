#pragma once

#include "core/index/index-service.hpp"
#include "core/storage/storage-service.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <variant>

namespace uburu::index
{

  enum class IndexBackendOpenErrorCode
  {
    unavailable,
    invalidConfiguration,
    openFailed,
    incompatibleVersion
  };

  struct IndexBackendCapabilities
  {
    std::string name;
    std::string version;
    bool persistent{false};
    bool contentAddressedDocuments{false};
    bool gitAwareGenerations{false};
    bool textSearch{false};
    bool symbolSearch{false};
    bool incrementalUpdate{false};
  };

  struct IndexBackendOpenRequest
  {
    std::filesystem::path storagePath;
    bool createIfMissing{true};
    bool readOnly{false};
  };

  struct IndexBackendHandle
  {
    std::shared_ptr<IndexService> indexService;
    std::shared_ptr<storage::StorageService> storageService;
    IndexBackendCapabilities capabilities;
  };

  struct IndexBackendOpenError
  {
    IndexBackendOpenErrorCode code{IndexBackendOpenErrorCode::unavailable};
    std::string message;
  };

  using IndexBackendOpenResult = std::variant<IndexBackendHandle, IndexBackendOpenError>;

  class IndexBackendFactory
  {
  public:
    virtual ~IndexBackendFactory() = default;

    [[nodiscard]]
    virtual IndexBackendCapabilities capabilities() const = 0;

    [[nodiscard]]
    virtual IndexBackendOpenResult open(const IndexBackendOpenRequest& request) const = 0;
  };

} // namespace uburu::index
