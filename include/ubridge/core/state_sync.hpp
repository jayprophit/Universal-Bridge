#pragma once

#include "ubridge/core/bridge_core.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ubridge::core {

enum class StateOrigin {
    hardware,
    daw,
    bridge,
};

enum class MirrorState {
    unknown,
    partially_observed,
    synchronized,
    pending_hardware,
    pending_daw,
    stale,
    conflict,
};

struct StateObservation {
    StateOrigin origin = StateOrigin::bridge;
    std::string value;
    RevisionVector revision;
    std::uint64_t observed_at_microseconds = 0;
    EvidenceLevel evidence = EvidenceLevel::unavailable;
};

struct ParameterMirror {
    std::string parameter_id;
    std::string desired_value;
    std::optional<StateObservation> hardware;
    std::optional<StateObservation> daw;
    MirrorState state = MirrorState::unknown;
};

enum class WriteTarget {
    hardware,
    daw,
};

enum class WritePhase {
    blocked,
    planned,
    dispatched,
    acknowledged,
    rejected,
    timed_out,
    conflict,
};

struct ParameterWriteIntent {
    std::string id;
    std::string parameter_id;
    WriteTarget target = WriteTarget::hardware;
    std::string desired_value;
    RevisionVector expected_revision;
    WritePhase phase = WritePhase::blocked;
    std::uint64_t created_at_microseconds = 0;
    std::uint64_t dispatched_at_microseconds = 0;
    std::uint64_t deadline_microseconds = 0;
    std::optional<StateObservation> acknowledgement;
    std::vector<Diagnostic> diagnostics;
};

enum class ClockDomainKind {
    transport,
    midi_clock,
    audio_sample_clock,
    word_clock,
    timecode,
    position,
    control_state,
};

enum class ClockAuthority {
    none,
    hardware,
    daw,
    bridge,
    external,
};

struct ClockDomainState {
    ClockDomainKind kind = ClockDomainKind::transport;
    ClockAuthority authority = ClockAuthority::none;
    EvidenceLevel evidence = EvidenceLevel::unavailable;
    std::string source_id;
    bool locked = false;
    double nominal_rate = 0.0;
    double measured_jitter_microseconds = 0.0;
    double measured_offset_samples = 0.0;
};

enum class RecordingDestination {
    hardware,
    daw,
    both,
};

enum class MonitorPath {
    hardware,
    daw,
    direct_hardware,
};

enum class RecordingProcessing {
    clean,
    processed,
    clean_with_monitor_effects,
};

struct RecordingPlan {
    RecordingDestination destination = RecordingDestination::daw;
    MonitorPath monitor_path = MonitorPath::direct_hardware;
    RecordingProcessing processing = RecordingProcessing::clean;
    EvidenceLevel route_evidence = EvidenceLevel::unavailable;
    bool simultaneous_capture_supported = false;
    bool clean_source_preserved = true;
    bool ready = false;
    std::vector<Diagnostic> diagnostics;
};

enum class LearnedControlKind {
    unknown,
    pad,
    key,
    encoder,
    fader,
    button,
    transport,
};

struct HardwareLearnObservation {
    std::string endpoint_id;
    std::string control_id;
    std::string message_signature;
    LearnedControlKind kind = LearnedControlKind::unknown;
    int minimum_value = 0;
    int maximum_value = 0;
    std::uint64_t sample_count = 0;
    EvidenceLevel evidence = EvidenceLevel::unavailable;
};

struct HardwareLearnReport {
    std::string profile_id;
    std::vector<HardwareLearnObservation> observations;
    bool feedback_test_requested = false;
    bool ready_to_save = false;
    std::vector<Diagnostic> diagnostics;
};

[[nodiscard]] std::string to_string(StateOrigin value);
[[nodiscard]] std::string to_string(MirrorState value);
[[nodiscard]] std::string to_string(WriteTarget value);
[[nodiscard]] std::string to_string(WritePhase value);
[[nodiscard]] std::string to_string(ClockDomainKind value);
[[nodiscard]] std::string to_string(ClockAuthority value);
[[nodiscard]] std::string to_string(RecordingDestination value);
[[nodiscard]] std::string to_string(MonitorPath value);
[[nodiscard]] std::string to_string(RecordingProcessing value);
[[nodiscard]] std::string to_string(LearnedControlKind value);

[[nodiscard]] MirrorState evaluate_parameter_mirror(ParameterMirror& mirror) noexcept;

[[nodiscard]] ParameterWriteIntent plan_parameter_write(
    std::string id,
    std::string parameter_id,
    WriteTarget target,
    std::string desired_value,
    RevisionVector expected_revision,
    EvidenceLevel mapping_evidence,
    bool target_writable,
    std::uint64_t now_microseconds,
    std::uint64_t timeout_microseconds);

[[nodiscard]] bool dispatch_parameter_write(
    ParameterWriteIntent& intent,
    RevisionVector actual_revision,
    std::uint64_t now_microseconds);

[[nodiscard]] bool acknowledge_parameter_write(
    ParameterWriteIntent& intent,
    StateObservation observation);

[[nodiscard]] bool expire_parameter_write(
    ParameterWriteIntent& intent,
    std::uint64_t now_microseconds);

[[nodiscard]] std::vector<Diagnostic> validate_clock_domains(
    const std::vector<ClockDomainState>& domains);

[[nodiscard]] RecordingPlan plan_recording(
    RecordingDestination destination,
    MonitorPath monitor_path,
    RecordingProcessing processing,
    EvidenceLevel route_evidence,
    bool simultaneous_capture_supported,
    bool clean_source_preserved);

[[nodiscard]] HardwareLearnReport assess_hardware_learn(
    std::string profile_id,
    std::vector<HardwareLearnObservation> observations,
    bool feedback_test_requested);

} // namespace ubridge::core
