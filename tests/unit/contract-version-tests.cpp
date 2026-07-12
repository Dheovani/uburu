#include "core/contracts/contract-version.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("internal contracts start at version zero before plugin stability")
{
  CHECK(uburu::contracts::indexBackendContract.name == "uburu.index.backend");
  CHECK(uburu::contracts::indexBackendContract.apiVersion.major == 0);
  CHECK(uburu::contracts::indexBackendContract.stability == uburu::contracts::ContractStability::internal);
  CHECK_FALSE(uburu::contracts::isStable(uburu::contracts::indexBackendContract));
}

TEST_CASE("contract compatibility compares major versions")
{
  constexpr uburu::contracts::SemanticVersion expected{1, 2, 0};
  constexpr uburu::contracts::SemanticVersion compatible{1, 4, 3};
  constexpr uburu::contracts::SemanticVersion incompatible{2, 0, 0};

  CHECK(uburu::contracts::isCompatibleMajor(expected, compatible));
  CHECK_FALSE(uburu::contracts::isCompatibleMajor(expected, incompatible));
}
