#include "ubridge/core/bridge_core.hpp"

#include <algorithm>
#include <utility>

namespace ubridge::core {
namespace {

EvidenceLevel stronger(EvidenceLevel left, EvidenceLevel right) noexcept {
    return evidence_at_least(left, right) ? left : right;
}

void add_decision(
    IntegrationPlan& plan,
    std::string capability,
    bool enabled,
    EvidenceLevel evidence,
    std::string rationale) {
    plan.decisions.push_back({std::move(capability), enabled, evidence, std::move(rationale)});
}

} // namespace

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

    const auto project_file_evidence = connection_evidence_level(
        connection, ProtocolKind::project_files, ProtocolDirection::input);
    const auto storage_evidence = stronger(
        project_file_evidence,
        connection_evidence_level(connection, ProtocolKind::mass_storage, ProtocolDirection::input));
    const auto midi_evidence = connection_evidence_level(
        connection, ProtocolKind::usb_midi, ProtocolDirection::duplex);
    const auto audio_evidence = stronger(
        connection_evidence_level(connection, ProtocolKind::usb_audio, ProtocolDirection::input),
        connection_evidence_level(connection, ProtocolKind::analogue_audio, ProtocolDirection::input));

    plan.safe_preflight = platform.local_file_access &&
                          evidence_at_least(storage_evidence, EvidenceLevel::declared) &&
                          device.storage_access;
    plan.asset_exchange = plan.safe_preflight && daw.audio_import;
    plan.midi_exchange = plan.safe_preflight && daw.midi_import;

    plan.direct_daw_creation = platform.runtime_qualified && daw.direct_project_create && daw.track_create;
    plan.hardware_control = platform.runtime_qualified && platform.usb_device_access &&
                            evidence_at_least(midi_evidence, EvidenceLevel::qualified) &&
                            device.midi_input && device.midi_output && device.transport;
    plan.audio_capture = platform.runtime_qualified && platform.audio_backend &&
                         evidence_at_least(audio_evidence, EvidenceLevel::qualified) && device.audio_input;
    plan.bidirectional_sync = platform.runtime_qualified && plan.hardware_control && daw.parameter_feedback &&
                              device.parameter_read && device.parameter_write;
    plan.mobile_companion = platform.companion_route;
    plan.mobile_bridge = platform.runtime_qualified && platform.direct_mobile_host_route &&
                         (plan.safe_preflight || plan.hardware_control || plan.audio_capture);

    add_decision(plan, "safe_preflight", plan.safe_preflight, storage_evidence,
        plan.safe_preflight ? "A declared-or-better read-only storage/project route is available."
                            : "Read-only storage/project evidence and local file access are both required.");
    add_decision(plan, "asset_exchange", plan.asset_exchange, storage_evidence,
        plan.asset_exchange ? "The host accepts audio assets through the safe project route."
                            : "Safe project intake and host audio import are both required.");
    add_decision(plan, "midi_exchange", plan.midi_exchange, storage_evidence,
        plan.midi_exchange ? "The host accepts MIDI files through the safe project route."
                           : "Safe project intake and host MIDI import are both required.");
    add_decision(plan, "hardware_control", plan.hardware_control, midi_evidence,
        plan.hardware_control ? "A qualified duplex USB MIDI route and qualified platform backend are available."
                              : "Live control requires qualified duplex MIDI evidence; declarations and observations are insufficient.");
    add_decision(plan, "audio_capture", plan.audio_capture, audio_evidence,
        plan.audio_capture ? "A qualified audio-input route and qualified platform backend are available."
                           : "Live capture requires qualified audio-input evidence; endpoint observation alone is insufficient.");
    add_decision(plan, "direct_daw_creation", plan.direct_daw_creation,
        plan.direct_daw_creation ? EvidenceLevel::qualified : EvidenceLevel::declared,
        plan.direct_daw_creation ? "The exact host project-creation route is qualified."
                                 : "No qualified direct host project-creation route is available.");
    add_decision(plan, "bidirectional_sync", plan.bidirectional_sync,
        plan.bidirectional_sync ? EvidenceLevel::qualified : EvidenceLevel::unavailable,
        plan.bidirectional_sync ? "Device read/write and host feedback routes are qualified."
                                : "Bidirectional sync requires qualified control plus device and host parameter feedback.");
    add_decision(plan, "mobile_bridge", plan.mobile_bridge,
        plan.mobile_bridge ? EvidenceLevel::qualified : EvidenceLevel::unavailable,
        plan.mobile_bridge ? "This mobile/tablet runtime is qualified as a direct bridge host."
                           : "The mobile/tablet direct-host route is not qualified for this combination.");
    add_decision(plan, "mobile_companion", plan.mobile_companion,
        plan.mobile_companion ? EvidenceLevel::declared : EvidenceLevel::unavailable,
        plan.mobile_companion ? "A non-authoritative companion route is declared."
                              : "No companion route is declared.");

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
    if (platform.companion_route && !plan.mobile_bridge) {
        plan.limitations.push_back("The mobile/tablet route is companion-only until native host I/O is qualified.");
    }

    return plan;
}

EvidenceLevel connection_evidence_level(
    const ConnectionCapability& connection,
    ProtocolKind protocol,
    ProtocolDirection direction) noexcept {
    const auto explicit_evidence = strongest_protocol_evidence(connection.protocol_evidence, protocol, direction);
    if (explicit_evidence != EvidenceLevel::unavailable) {
        return explicit_evidence;
    }

    bool declared = false;
    switch (protocol) {
        case ProtocolKind::project_files:
        case ProtocolKind::mass_storage: declared = connection.storage; break;
        case ProtocolKind::usb_midi: declared = connection.usb_midi; break;
        case ProtocolKind::usb_audio: declared = connection.usb_audio; break;
        case ProtocolKind::analogue_audio: declared = connection.analogue_audio; break;
        case ProtocolKind::network_midi:
        case ProtocolKind::ethernet: declared = connection.network; break;
        default: break;
    }
    return declared ? EvidenceLevel::declared : EvidenceLevel::unavailable;
}

std::optional<CapabilityDecision> find_capability_decision(
    const IntegrationPlan& plan,
    std::string_view capability) {
    const auto found = std::find_if(plan.decisions.begin(), plan.decisions.end(), [capability](const CapabilityDecision& decision) {
        return decision.capability == capability;
    });
    if (found == plan.decisions.end()) {
        return std::nullopt;
    }
    return *found;
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
