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

  struct ContentHash
  {
    ContentHashAlgorithm algorithm{ContentHashAlgorithm::sha256};
    std::string value;
  };

  [[nodiscard]] ContentHash computeContentHash(std::span<const std::byte> bytes);
  [[nodiscard]] std::optional<ContentHash> computeFileContentHash(const std::filesystem::path& path,
                                                                  std::stop_token stopToken = {});

} // namespace uburu::index
