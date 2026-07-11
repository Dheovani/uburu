#pragma once

#include "shared/types/domain-types.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <stop_token>
#include <string>

namespace uburu::index
{

  /**
   * Holds a content digest together with its algorithm for future migrations.
   */
  struct ContentHash
  {
    ContentHashAlgorithm algorithm{ContentHashAlgorithm::sha256};
    std::string value;
  };

  [[nodiscard]]
  ContentHash computeContentHash(std::span<const std::byte> bytes);

  /**
   * Streams a file into the content hash function while observing cancellation.
   */
  [[nodiscard]]
  std::optional<ContentHash> computeFileContentHash(
    const std::filesystem::path& path,
    std::stop_token stopToken = {});

} // namespace uburu::index
