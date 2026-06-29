#include "core/git/git-reconciliation.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>

namespace
{

  [[nodiscard]] uburu::git::GitChangeState cleanState()
  {
    return uburu::git::GitChangeState{.branch = "main",
                                      .headOid = "head-a",
                                      .detachedHead = false,
                                      .headSignature = "head-file-a",
                                      .indexSignature = "index-a",
                                      .relevantRefsSignature = "refs-a"};
  }

} // namespace

TEST_CASE("git reconciliation reports no work for identical snapshots")
{
  const auto before = cleanState();
  const auto after = before;

  const auto plan = uburu::git::planReconciliation(before, after);

  CHECK(plan.reasons.empty());
  CHECK_FALSE(plan.structuralReconciliationRequired);
  CHECK_FALSE(plan.overlayReconciliationRequired);
  CHECK_FALSE(plan.canReuseContentByBlob);
}

TEST_CASE("git reconciliation treats branch changes as structural incremental work")
{
  const auto before = cleanState();
  auto after = before;
  after.branch = "feature";
  after.headOid = "head-b";
  after.headSignature = "head-file-b";
  after.relevantRefsSignature = "refs-b";

  const auto plan = uburu::git::planReconciliation(before, after);

  CHECK(uburu::git::hasReason(plan, uburu::git::GitReconciliationReason::branchChanged));
  CHECK(uburu::git::hasReason(plan, uburu::git::GitReconciliationReason::headChanged));
  CHECK(plan.structuralReconciliationRequired);
  CHECK(plan.overlayReconciliationRequired);
  CHECK(plan.canReuseContentByBlob);
}

TEST_CASE("git reconciliation treats detached head changes as structural work")
{
  const auto before = cleanState();
  auto after = before;
  after.branch = std::nullopt;
  after.detachedHead = true;
  after.headOid = "head-detached";

  const auto plan = uburu::git::planReconciliation(before, after);

  CHECK(uburu::git::hasReason(plan, uburu::git::GitReconciliationReason::detachedHeadChanged));
  CHECK(uburu::git::hasReason(plan, uburu::git::GitReconciliationReason::headChanged));
  CHECK(plan.structuralReconciliationRequired);
  CHECK(plan.overlayReconciliationRequired);
  CHECK(plan.canReuseContentByBlob);
}

TEST_CASE("git reconciliation treats index-only changes as overlay work")
{
  const auto before = cleanState();
  auto after = before;
  after.indexSignature = "index-b";

  const auto plan = uburu::git::planReconciliation(before, after);

  CHECK(uburu::git::hasReason(plan, uburu::git::GitReconciliationReason::indexChanged));
  CHECK_FALSE(plan.structuralReconciliationRequired);
  CHECK(plan.overlayReconciliationRequired);
  CHECK_FALSE(plan.canReuseContentByBlob);
}

TEST_CASE("git reconciliation treats relevant ref changes conservatively as structural work")
{
  const auto before = cleanState();
  auto after = before;
  after.relevantRefsSignature = "refs-b";

  const auto plan = uburu::git::planReconciliation(before, after);

  CHECK(uburu::git::hasReason(plan, uburu::git::GitReconciliationReason::refsChanged));
  CHECK(plan.structuralReconciliationRequired);
  CHECK(plan.overlayReconciliationRequired);
  CHECK(plan.canReuseContentByBlob);
}
