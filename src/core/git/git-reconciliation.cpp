#include "core/git/git-reconciliation.hpp"

#include <algorithm>

namespace uburu::git
{

  GitReconciliationPlan planReconciliation(const GitChangeState& before, const GitChangeState& after)
  {
    GitReconciliationPlan plan;

    if (before.branch != after.branch) {
      plan.reasons.push_back(GitReconciliationReason::branchChanged);
    }

    if (before.headOid != after.headOid) {
      plan.reasons.push_back(GitReconciliationReason::headChanged);
    }

    if (before.detachedHead != after.detachedHead) {
      plan.reasons.push_back(GitReconciliationReason::detachedHeadChanged);
    }

    if (before.indexSignature != after.indexSignature) {
      plan.reasons.push_back(GitReconciliationReason::indexChanged);
    }

    if (before.relevantRefsSignature != after.relevantRefsSignature) {
      plan.reasons.push_back(GitReconciliationReason::refsChanged);
    }

    plan.structuralReconciliationRequired =
      hasReason(plan, GitReconciliationReason::branchChanged) ||
      hasReason(plan, GitReconciliationReason::headChanged) ||
      hasReason(plan, GitReconciliationReason::detachedHeadChanged) ||
      hasReason(plan, GitReconciliationReason::refsChanged);

    plan.overlayReconciliationRequired =
      plan.structuralReconciliationRequired || hasReason(plan, GitReconciliationReason::indexChanged);

    plan.canReuseContentByBlob = plan.structuralReconciliationRequired;

    return plan;
  }

  bool hasReason(const GitReconciliationPlan& plan, GitReconciliationReason reason)
  {
    return std::ranges::find(plan.reasons, reason) != plan.reasons.end();
  }

} // namespace uburu::git
