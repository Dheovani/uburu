#include "core/git/git-reconciliation.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>

namespace
{

  [[nodiscard]] uburu::git::GitChangeState clean_state()
  {
    return uburu::git::GitChangeState{
      .branch = "main",
      .head_oid = "head-a",
      .detached_head = false,
      .head_signature = "head-file-a",
      .index_signature = "index-a",
      .relevant_refs_signature = "refs-a"};
  }

} // namespace

TEST_CASE("git reconciliation reports no work for identical snapshots")
{
  const auto before = clean_state();
  const auto after = before;

  const auto plan = uburu::git::plan_reconciliation(before, after);

  CHECK(plan.reasons.empty());
  CHECK_FALSE(plan.structural_reconciliation_required);
  CHECK_FALSE(plan.overlay_reconciliation_required);
  CHECK_FALSE(plan.can_reuse_content_by_blob);
}

TEST_CASE("git reconciliation treats branch changes as structural incremental work")
{
  const auto before = clean_state();
  auto after = before;
  after.branch = "feature";
  after.head_oid = "head-b";
  after.head_signature = "head-file-b";
  after.relevant_refs_signature = "refs-b";

  const auto plan = uburu::git::plan_reconciliation(before, after);

  CHECK(uburu::git::has_reason(plan, uburu::git::GitReconciliationReason::branch_changed));
  CHECK(uburu::git::has_reason(plan, uburu::git::GitReconciliationReason::head_changed));
  CHECK(plan.structural_reconciliation_required);
  CHECK(plan.overlay_reconciliation_required);
  CHECK(plan.can_reuse_content_by_blob);
}

TEST_CASE("git reconciliation treats detached head changes as structural work")
{
  const auto before = clean_state();
  auto after = before;
  after.branch = std::nullopt;
  after.detached_head = true;
  after.head_oid = "head-detached";

  const auto plan = uburu::git::plan_reconciliation(before, after);

  CHECK(uburu::git::has_reason(plan, uburu::git::GitReconciliationReason::detached_head_changed));
  CHECK(uburu::git::has_reason(plan, uburu::git::GitReconciliationReason::head_changed));
  CHECK(plan.structural_reconciliation_required);
  CHECK(plan.overlay_reconciliation_required);
  CHECK(plan.can_reuse_content_by_blob);
}

TEST_CASE("git reconciliation treats index-only changes as overlay work")
{
  const auto before = clean_state();
  auto after = before;
  after.index_signature = "index-b";

  const auto plan = uburu::git::plan_reconciliation(before, after);

  CHECK(uburu::git::has_reason(plan, uburu::git::GitReconciliationReason::index_changed));
  CHECK_FALSE(plan.structural_reconciliation_required);
  CHECK(plan.overlay_reconciliation_required);
  CHECK_FALSE(plan.can_reuse_content_by_blob);
}

TEST_CASE("git reconciliation treats relevant ref changes conservatively as structural work")
{
  const auto before = clean_state();
  auto after = before;
  after.relevant_refs_signature = "refs-b";

  const auto plan = uburu::git::plan_reconciliation(before, after);

  CHECK(uburu::git::has_reason(plan, uburu::git::GitReconciliationReason::refs_changed));
  CHECK(plan.structural_reconciliation_required);
  CHECK(plan.overlay_reconciliation_required);
  CHECK(plan.can_reuse_content_by_blob);
}
