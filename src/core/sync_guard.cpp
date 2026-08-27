#include "ubridge/core/sync_guard.hpp"

namespace ubridge::core {
SyncDecision authorize_sync(const SyncRequest& request) {
    SyncDecision decision;
    decision.conflicts = detect_conflicts(request.hardware_changes, request.daw_changes);
    if (request.expected.session != request.actual.session || request.expected.hardware != request.actual.hardware || request.expected.daw != request.actual.daw) {
        decision.diagnostics.push_back({DiagnosticSeverity::error, "stale_revision", "The session changed after this sync was planned.", "Re-read both sides and preview a new diff."});
    }
    if (!decision.conflicts.empty()) decision.diagnostics.push_back({DiagnosticSeverity::error, "unresolved_conflict", "Concurrent hardware and DAW edits require explicit resolution.", "Choose an authority per conflicting field."});
    if (!request.backup_verified) decision.diagnostics.push_back({DiagnosticSeverity::error, "backup_required", "No verified recovery snapshot is attached.", "Create and verify an immutable backup before execution."});
    if (!request.user_approved) decision.diagnostics.push_back({DiagnosticSeverity::error, "approval_required", "The user has not approved this write plan.", "Review the exact diff and approve it explicitly."});
    decision.may_execute = decision.diagnostics.empty();
    return decision;
}
} // namespace ubridge::core
