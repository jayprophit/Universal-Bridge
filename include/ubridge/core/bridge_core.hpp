#pragma once

#include "ubridge/core/protocol_capabilities.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ubridge::core {

enum class SyncPolicy {
    hardware_authoritative,
    daw_authoritative,
    one_way_to_daw,
    one_way_to_hardware,
    bidirectional,
    render_only,
    unsupported,
};

enum class TransactionPhase {
    planned,
    awaiting_approval,
    running,
    verified,
    committed,
    rolled_back,
    failed,
};

enum class DiagnosticSeverity {
    info,
    warning,
    error,
};

struct AssetReference {
    std::string id;
    std::string source_path;
    std::string fingerprint;
    std::uint64_t bytes = 0;
    bool required = true;
};

struct MusicalEvent {
    std::string id;
    std::string track_id;
    std::int64_t tick = 0;
    std::int64_t duration_ticks = 0;
    int channel = 0;
    int note = -1;
    int velocity = -1;
    std::optional<int> cc;
    std::optional<double> value;
};

struct CanonicalParameter {
    std::string id;
    std::string semantic; // e.g. mixer.volume, filter.cutoff, transport.play
    double normalized_value = 0.0;
    SyncPolicy policy = SyncPolicy::unsupported;
};

struct RevisionVector {
    std::uint64_t session = 0;
    std::uint64_t hardware = 0;
    std::uint64_t daw = 0;
};

struct CanonicalSession {
    std::string session_id;
    std::string source_id;
    std::string schema_version;
    RevisionVector revision;
    std::vector<AssetReference> assets;
    std::vector<MusicalEvent> midi_events;
    std::vector<CanonicalParameter> parameters;
    std::map<std::string, std::string> metadata;
};

struct DeviceCapability {
    std::string profile_id;
    bool project_read = false;
    bool project_write = false;
    bool storage_access = false;
    bool midi_input = false;
    bool midi_output = false;
    bool audio_input = false;
    int audio_channels = 0;
    bool parameter_read = false;
    bool parameter_write = false;
    bool transport = false;
    bool automation_read = false;
    bool automation_write = false;
};

struct PlatformCapability {
    std::string platform_id;
    bool runtime_qualified = false;
    bool local_file_access = false;
    bool usb_device_access = false;
    bool audio_backend = false;
    bool virtual_midi = false;
    bool desktop_plugin_host_route = false;
    bool companion_route = false;
    bool direct_mobile_host_route = false;
};

struct DawCapability {
    std::string adapter_id;
    bool audio_import = false;
    bool midi_import = false;
    bool plugin_client = false;
    bool direct_project_create = false;
    bool track_create = false;
    bool parameter_feedback = false;
    bool transport_control = false;
};

struct ConnectionCapability {
    std::string transport_id;
    bool storage = false;
    bool usb_midi = false;
    bool usb_audio = false;
    bool analogue_audio = false;
    bool network = false;
    std::vector<ProtocolEvidence> protocol_evidence;
};

struct CapabilityDecision {
    std::string capability;
    bool enabled = false;
    EvidenceLevel evidence = EvidenceLevel::unavailable;
    std::string rationale;
};

struct IntegrationPlan {
    std::string route_name;
    bool safe_preflight = false;
    bool asset_exchange = false;
    bool midi_exchange = false;
    bool direct_daw_creation = false;
    bool hardware_control = false;
    bool audio_capture = false;
    bool bidirectional_sync = false;
    bool mobile_companion = false;
    bool mobile_bridge = false;
    std::vector<CapabilityDecision> decisions;
    std::vector<std::string> limitations;
};

struct Diagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::info;
    std::string code;
    std::string message;
    std::string recommendation;
};

struct Change {
    std::string entity_id;
    std::string field;
    std::string before;
    std::string after;
    SyncPolicy policy = SyncPolicy::unsupported;
};

struct Conflict {
    Change hardware_change;
    Change daw_change;
    std::string resolution_hint;
};

struct Transaction {
    std::string id;
    std::string description;
    TransactionPhase phase = TransactionPhase::planned;
    RevisionVector start_revision;
    std::vector<Diagnostic> diagnostics;
    std::vector<std::string> backup_locations;
};

[[nodiscard]] std::string to_string(SyncPolicy value);
[[nodiscard]] std::string to_string(TransactionPhase value);
[[nodiscard]] std::string to_string(DiagnosticSeverity value);

[[nodiscard]] IntegrationPlan negotiate(
    const DeviceCapability& device,
    const PlatformCapability& platform,
    const DawCapability& daw,
    const ConnectionCapability& connection);

[[nodiscard]] EvidenceLevel connection_evidence_level(
    const ConnectionCapability& connection,
    ProtocolKind protocol,
    ProtocolDirection direction) noexcept;

[[nodiscard]] std::optional<CapabilityDecision> find_capability_decision(
    const IntegrationPlan& plan,
    std::string_view capability);

[[nodiscard]] std::vector<Conflict> detect_conflicts(
    const std::vector<Change>& hardware_changes,
    const std::vector<Change>& daw_changes);

class TransactionJournal {
public:
    [[nodiscard]] Transaction begin(
        std::string id,
        std::string description,
        RevisionVector start_revision,
        std::vector<std::string> backup_locations);

    bool transition(std::string_view id, TransactionPhase next, std::optional<Diagnostic> diagnostic = std::nullopt);
    [[nodiscard]] std::optional<Transaction> find(std::string_view id) const;
    [[nodiscard]] const std::vector<Transaction>& entries() const noexcept;

private:
    std::vector<Transaction> entries_;
};

[[nodiscard]] bool valid_transition(TransactionPhase from, TransactionPhase to);

} // namespace ubridge::core
