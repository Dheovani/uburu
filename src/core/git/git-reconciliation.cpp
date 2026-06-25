#include "core/git/git-reconciliation.hpp"

#include <algorithm>

namespace uburu::git
{

  GitReconciliationPlan plan_reconciliation(const GitChangeState& before, const GitChangeState& after)
  {
    GitReconciliationPlan plan;

    if (before.branch != after.branch) {
      plan.reasons.push_back(GitReconciliationReason::branch_changed);
    }

    if (before.head_oid != after.head_oid) {
      plan.reasons.push_back(GitReconciliationReason::head_changed);
    }

    if (before.detached_head != after.detached_head) {
      plan.reasons.push_back(GitReconciliationReason::detached_head_changed);
    }

    if (before.index_signature != after.index_signature) {
      plan.reasons.push_back(GitReconciliationReason::index_changed);
    }

    if (before.relevant_refs_signature != after.relevant_refs_signature) {
      plan.reasons.push_back(GitReconciliationReason::refs_changed);
    }

    plan.structural_reconciliation_required =
      has_reason(plan, GitReconciliationReason::branch_changed) ||
      has_reason(plan, GitReconciliationReason::head_changed) ||
      has_reason(plan, GitReconciliationReason::detached_head_changed) ||
      has_reason(plan, GitReconciliationReason::refs_changed);

    plan.overlay_reconciliation_required =
      plan.structural_reconciliation_required || has_reason(plan, GitReconciliationReason::index_changed);

    plan.can_reuse_content_by_blob = plan.structural_reconciliation_required;

    return plan;
  }

  bool has_reason(const GitReconciliationPlan& plan, GitReconciliationReason reason)
  {
    return std::ranges::find(plan.reasons, reason) != plan.reasons.end();
  }

} // namespace uburu::git
