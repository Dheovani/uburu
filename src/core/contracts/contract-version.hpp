#pragma once

#include <cstdint>
#include <string_view>

namespace uburu::contracts
{

  enum class ContractStability
  {
    internal,
    experimental,
    stable
  };

  struct SemanticVersion
  {
    std::uint32_t major{0};
    std::uint32_t minor{0};
    std::uint32_t patch{0};
  };

  struct ContractVersion
  {
    std::string_view name;
    SemanticVersion apiVersion;
    std::uint32_t abiRevision{0};
    ContractStability stability{ContractStability::internal};
  };

  inline constexpr SemanticVersion initialInternalApiVersion{0, 1, 0};
  inline constexpr std::uint32_t initialAbiRevision = 0;

  inline constexpr ContractVersion indexBackendContract{
    .name = "uburu.index.backend",
    .apiVersion = initialInternalApiVersion,
    .abiRevision = initialAbiRevision,
    .stability = ContractStability::internal,
  };

  inline constexpr ContractVersion fileWatcherBackendContract{
    .name = "uburu.filesystem.file-watcher.backend",
    .apiVersion = initialInternalApiVersion,
    .abiRevision = initialAbiRevision,
    .stability = ContractStability::internal,
  };

  inline constexpr ContractVersion symbolParserContract{
    .name = "uburu.symbols.parser",
    .apiVersion = initialInternalApiVersion,
    .abiRevision = initialAbiRevision,
    .stability = ContractStability::internal,
  };

  [[nodiscard]]
  constexpr bool isStable(const ContractVersion& version)
  {
    return version.stability == ContractStability::stable;
  }

  [[nodiscard]]
  constexpr bool isCompatibleMajor(SemanticVersion expected, SemanticVersion actual)
  {
    return expected.major == actual.major;
  }

} // namespace uburu::contracts
