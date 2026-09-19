#pragma once

#include "ubridge/core/bridge_core.hpp"

#include <string>
#include <vector>

namespace ubridge::core {

struct SyncRequest {
    RevisionVector expected;
    RevisionVector actual;
    std::vector<Change> hardware_changes;
    std::vector<Change> daw_changes;
    bool user_approved = false;
    bool backup_verified = false;
};

struct SyncDecision {
    bool may_execute = false;
    std::vector<Conflict> conflicts;
    std::vector<Diagnostic> diagnostics;
};

[[nodiscard]] SyncDecision authorize_sync(const SyncRequest& request);

} // namespace ubridge::core
