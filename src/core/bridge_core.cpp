#include "ubridge/core/bridge_core.hpp"

#include <algorithm>
#include <utility>

namespace ubridge::core {

std::string to_string(SyncPolicy value) {
    switch (value) {
        case SyncPolicy::hardware_authoritative: return "hardware_authoritative";
        case SyncPolicy::daw_authoritative: return "daw_authoritative";
        case SyncPolicy::one_way_to_daw: return "one_way_to_daw";
        case SyncPolicy::one_way_to_hardware: return "one_way_to_hardware";
        case SyncPolicy::bidirectional: return "bidirectional";
        case SyncPolicy::render_only: return "render_only";
        case SyncPolicy::unsupported: return "unsupported";
    }
    return "unsupported";
}

std::string to_string(TransactionPhase value) {
    switch (value) {
        case TransactionPhase::planned: return "planned";
        case TransactionPhase::awaiting_approval: return "awaiting_approval";
        case TransactionPhase::running: return "running";
        case TransactionPhase::verified: return "verified";
        case TransactionPhase::committed: return "committed";
        case TransactionPhase::rolled_back: return "rolled_back";
        case TransactionPhase::failed: return "failed";
    }
    return "failed";
}

std::string to_string(DiagnosticSeverity value) {
    switch (value) {
        case DiagnosticSeverity::info: return "info";
        case DiagnosticSeverity::warning: return "warning";
        case DiagnosticSeverity::error: return "error";
    }
    return "error";
}

IntegrationPlan negotiate(
    const DeviceCapability& device,
    const PlatformCapability& platform,
    const DawCapability& daw,
    const ConnectionCapability& connection) {
    IntegrationPlan plan;
    plan.route_name = device.profile_id + "__" + platform.platform_id + "__" + daw.adapter_id + "__" + connection.transport_id;

    plan.safe_preflight = platform.local_file_access && (connection.storage || device.storage_access);
    plan.asset_exchange = plan.safe_preflight && daw.audio_import;
    plan.midi_exchange = plan.safe_preflight && daw.midi_import;

    plan.direct_daw_creation = platform.runtime_qualified && daw.direct_project_create && daw.track_create;
    plan.hardware_control = platform.runtime_qualified && platform.usb_device_access && connection.usb_midi &&
                            device.midi_input && device.midi_output && device.transport;
    plan.audio_capture = platform.runtime_qualified && platform.audio_backend &&
                         (connection.usb_audio || connection.analogue_audio) && device.audio_input;
    plan.bidirectional_sync = platform.runtime_qualified && plan.hardware_control && daw.parameter_feedback &&
                              device.parameter_read && device.parameter_write;
    plan.mobile_companion = platform.companion_route;

    if (!platform.runtime_qualified) {
        plan.limitations.push_back("The selected operating-system route is not qualified for real device, audio, MIDI, or host control.");
    }
    if (!plan.safe_preflight) {
        plan.limitations.push_back("No verified read-only project/storage intake route is available.");
    }
    if (!plan.direct_daw_creation) {
        plan.limitations.push_back("Direct DAW-project creation is unavailable; use a transparent exchange package.");
    }
    if (!plan.hardware_control) {
        plan.limitations.push_back("Live hardware control is unavailable until the device service and control profile are qualified.");
    }
    if (!plan.audio_capture) {
        plan.limitations.push_back("Live audio capture is unavailable for this capability combination.");
    }
    if (!plan.bidirectional_sync) {
        plan.limitations.push_back("Bidirectional parameter synchronization is unavailable for this capability combination.");
    }

    return plan;
}

std::vector<Conflict> detect_conflicts(
    const std::vector<Change>& hardware_changes,
    const std::vector<Change>& daw_changes) {
    std::vector<Conflict> conflicts;
    for (const auto& hardware_change : hardware_changes) {
        for (const auto& daw_change : daw_changes) {
            if (hardware_change.entity_id != daw_change.entity_id || hardware_change.field != daw_change.field) {
                continue;
            }
            if (hardware_change.after == daw_change.after) {
                continue;
            }

            Conflict conflict;
            conflict.hardware_change = hardware_change;
            conflict.daw_change = daw_change;
            if (hardware_change.policy == SyncPolicy::hardware_authoritative) {
                conflict.resolution_hint = "hardware_wins";
            } else if (daw_change.policy == SyncPolicy::daw_authoritative) {
                conflict.resolution_hint = "daw_wins";
            } else {
                conflict.resolution_hint = "require_user_review";
            }
            conflicts.push_back(std::move(conflict));
        }
    }
    return conflicts;
}

bool valid_transition(TransactionPhase from, TransactionPhase to) {
    if (from == to) {
        return true;
    }
    switch (from) {
        case TransactionPhase::planned:
            return to == TransactionPhase::awaiting_approval || to == TransactionPhase::failed || to == TransactionPhase::rolled_back;
        case TransactionPhase::awaiting_approval:
            return to == TransactionPhase::running || to == TransactionPhase::rolled_back || to == TransactionPhase::failed;
        case TransactionPhase::running:
            return to == TransactionPhase::verified || to == TransactionPhase::failed || to == TransactionPhase::rolled_back;
        case TransactionPhase::verified:
            return to == TransactionPhase::committed || to == TransactionPhase::rolled_back || to == TransactionPhase::failed;
        case TransactionPhase::committed:
        case TransactionPhase::rolled_back:
        case TransactionPhase::failed:
            return false;
    }
    return false;
}

Transaction TransactionJournal::begin(
    std::string id,
    std::string description,
    RevisionVector start_revision,
    std::vector<std::string> backup_locations) {
    Transaction transaction;
    transaction.id = std::move(id);
    transaction.description = std::move(description);
    transaction.phase = TransactionPhase::planned;
    transaction.start_revision = start_revision;
    transaction.backup_locations = std::move(backup_locations);
    entries_.push_back(transaction);
    return transaction;
}

bool TransactionJournal::transition(std::string_view id, TransactionPhase next, std::optional<Diagnostic> diagnostic) {
    const auto found = std::find_if(entries_.begin(), entries_.end(), [id](const Transaction& transaction) {
        return transaction.id == id;
    });
    if (found == entries_.end() || !valid_transition(found->phase, next)) {
        return false;
    }
    found->phase = next;
    if (diagnostic.has_value()) {
        found->diagnostics.push_back(std::move(*diagnostic));
    }
    return true;
}

std::optional<Transaction> TransactionJournal::find(std::string_view id) const {
    const auto found = std::find_if(entries_.begin(), entries_.end(), [id](const Transaction& transaction) {
        return transaction.id == id;
    });
    if (found == entries_.end()) {
        return std::nullopt;
    }
    return *found;
}

const std::vector<Transaction>& TransactionJournal::entries() const noexcept {
    return entries_;
}

} // namespace ubridge::core
