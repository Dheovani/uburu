#pragma once

#include "core/git/git-service.hpp"

#include <vector>

namespace uburu::git
{

  /**
   * Explains which Git state changes caused a reconciliation decision.
   */
  enum class GitReconciliationReason
  {
    branchChanged,
    headChanged,
    detachedHeadChanged,
    indexChanged,
    refsChanged
  };

  /**
   * Describes the minimal index and overlay work required after a Git state change.
   */
  struct GitReconciliationPlan
  {
    std::vector<GitReconciliationReason> reasons;
    bool structuralReconciliationRequired{false};
    bool overlayReconciliationRequired{false};
    bool canReuseContentByBlob{false};
  };

  [[nodiscard]]
  GitReconciliationPlan planReconciliation(const GitChangeState& before, const GitChangeState& after);

  [[nodiscard]]
  bool hasReason(const GitReconciliationPlan& plan, GitReconciliationReason reason);

} // namespace uburu::git
