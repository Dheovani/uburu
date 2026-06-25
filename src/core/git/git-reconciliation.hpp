#pragma once

#include "core/git/git-service.hpp"

#include <vector>

namespace uburu::git
{

  enum class GitReconciliationReason
  {
    branch_changed,
    head_changed,
    detached_head_changed,
    index_changed,
    refs_changed
  };

  struct GitReconciliationPlan
  {
    std::vector<GitReconciliationReason> reasons;
    bool structural_reconciliation_required{false};
    bool overlay_reconciliation_required{false};
    bool can_reuse_content_by_blob{false};
  };

  [[nodiscard]] GitReconciliationPlan plan_reconciliation(const GitChangeState& before, const GitChangeState& after);

  [[nodiscard]] bool has_reason(const GitReconciliationPlan& plan, GitReconciliationReason reason);

} // namespace uburu::git
