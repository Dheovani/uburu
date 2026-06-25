#pragma once

#include "core/git/git-service.hpp"

#include <vector>

namespace uburu::git
{

  enum class GitReconciliationReason
  {
    branchChanged,
    headChanged,
    detachedHeadChanged,
    indexChanged,
    refsChanged
  };

  struct GitReconciliationPlan
  {
    std::vector<GitReconciliationReason> reasons;
    bool structuralReconciliationRequired{false};
    bool overlayReconciliationRequired{false};
    bool canReuseContentByBlob{false};
  };

  [[nodiscard]] GitReconciliationPlan planReconciliation(const GitChangeState& before, const GitChangeState& after);

  [[nodiscard]] bool hasReason(const GitReconciliationPlan& plan, GitReconciliationReason reason);

} // namespace uburu::git
