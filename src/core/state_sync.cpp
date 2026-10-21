#include "ubridge/core/state_sync.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <utility>

namespace ubridge::core {
namespace {

bool same_revision(const RevisionVector& left, const RevisionVector& right) noexcept {
    return left.session == right.session && left.hardware == right.hardware && left.daw == right.daw;
}

bool observation_is_usable(const std::optional<StateObservation>& observation) noexcept {
    return observation.has_value() && evidence_at_least(observation->evidence, EvidenceLevel::observed);
}

bool acknowledgement_matches_target(const ParameterWriteIntent& intent, const StateObservation& observation) noexcept {
    if (intent.target == WriteTarget::hardware && observation.origin != StateOrigin::hardware) return false;
    if (intent.target == WriteTarget::daw && observation.origin != StateOrigin::daw) return false;
    return true;
}

bool observation_is_current_for_target(const ParameterWriteIntent& intent, const StateObservation& observation) noexcept {
    if (observation.revision.session < intent.expected_revision.session) return false;
    if (intent.target == WriteTarget::hardware) return observation.revision.hardware >= intent.expected_revision.hardware;
    return observation.revision.daw >= intent.expected_revision.daw;
}

} // namespace

std::string to_string(StateOrigin value) {
    switch (value) {
        case StateOrigin::hardware: return "hardware";
        case StateOrigin::daw: return "daw";
        case StateOrigin::bridge: return "bridge";
    }
    return "bridge";
}

std::string to_string(MirrorState value) {
    switch (value) {
        case MirrorState::unknown: return "unknown";
        case MirrorState::partially_observed: return "partially_observed";
        case MirrorState::synchronized: return "synchronized";
        case MirrorState::pending_hardware: return "pending_hardware";
        case MirrorState::pending_daw: return "pending_daw";
        case MirrorState::stale: return "stale";
        case MirrorState::conflict: return "conflict";
    }
    return "unknown";
}

std::string to_string(WriteTarget value) {
    return value == WriteTarget::hardware ? "hardware" : "daw";
}

std::string to_string(WritePhase value) {
    switch (value) {
        case WritePhase::blocked: return "blocked";
        case WritePhase::planned: return "planned";
        case WritePhase::dispatched: return "dispatched";
        case WritePhase::acknowledged: return "acknowledged";
        case WritePhase::rejected: return "rejected";
        case WritePhase::timed_out: return "timed_out";
        case WritePhase::conflict: return "conflict";
    }
    return "blocked";
}

std::string to_string(ClockDomainKind value) {
    switch (value) {
        case ClockDomainKind::transport: return "transport";
        case ClockDomainKind::midi_clock: return "midi_clock";
        case ClockDomainKind::audio_sample_clock: return "audio_sample_clock";
        case ClockDomainKind::word_clock: return "word_clock";
        case ClockDomainKind::timecode: return "timecode";
        case ClockDomainKind::position: return "position";
        case ClockDomainKind::control_state: return "control_state";
    }
    return "transport";
}

std::string to_string(ClockAuthority value) {
    switch (value) {
        case ClockAuthority::none: return "none";
        case ClockAuthority::hardware: return "hardware";
        case ClockAuthority::daw: return "daw";
        case ClockAuthority::bridge: return "bridge";
        case ClockAuthority::external: return "external";
    }
    return "none";
}

std::string to_string(RecordingDestination value) {
    switch (value) {
        case RecordingDestination::hardware: return "hardware";
        case RecordingDestination::daw: return "daw";
        case RecordingDestination::both: return "both";
    }
    return "daw";
}

std::string to_string(MonitorPath value) {
    switch (value) {
        case MonitorPath::hardware: return "hardware";
        case MonitorPath::daw: return "daw";
        case MonitorPath::direct_hardware: return "direct_hardware";
    }
    return "direct_hardware";
}

std::string to_string(RecordingProcessing value) {
    switch (value) {
        case RecordingProcessing::clean: return "clean";
        case RecordingProcessing::processed: return "processed";
        case RecordingProcessing::clean_with_monitor_effects: return "clean_with_monitor_effects";
    }
    return "clean";
}

std::string to_string(LearnedControlKind value) {
    switch (value) {
        case LearnedControlKind::unknown: return "unknown";
        case LearnedControlKind::pad: return "pad";
        case LearnedControlKind::key: return "key";
        case LearnedControlKind::encoder: return "encoder";
        case LearnedControlKind::fader: return "fader";
        case LearnedControlKind::button: return "button";
        case LearnedControlKind::transport: return "transport";
    }
    return "unknown";
}

MirrorState evaluate_parameter_mirror(ParameterMirror& mirror) noexcept {
    const bool hardware = observation_is_usable(mirror.hardware);
    const bool daw = observation_is_usable(mirror.daw);
    if (!hardware && !daw) {
        mirror.state = MirrorState::unknown;
    } else if (!hardware) {
        mirror.state = mirror.daw->value == mirror.desired_value ? MirrorState::pending_hardware : MirrorState::partially_observed;
    } else if (!daw) {
        mirror.state = mirror.hardware->value == mirror.desired_value ? MirrorState::pending_daw : MirrorState::partially_observed;
    } else if (mirror.hardware->value != mirror.daw->value) {
        if (mirror.hardware->value == mirror.desired_value) {
            mirror.state = MirrorState::pending_daw;
        } else if (mirror.daw->value == mirror.desired_value) {
            mirror.state = MirrorState::pending_hardware;
        } else {
            mirror.state = MirrorState::conflict;
        }
    } else if (mirror.hardware->value != mirror.desired_value) {
        mirror.state = MirrorState::stale;
    } else {
        mirror.state = MirrorState::synchronized;
    }
    return mirror.state;
}

ParameterWriteIntent plan_parameter_write(
    std::string id,
    std::string parameter_id,
    WriteTarget target,
    std::string desired_value,
    RevisionVector expected_revision,
    EvidenceLevel mapping_evidence,
    bool target_writable,
    std::uint64_t now_microseconds,
    std::uint64_t timeout_microseconds) {
    ParameterWriteIntent intent;
    intent.id = std::move(id);
    intent.parameter_id = std::move(parameter_id);
    intent.target = target;
    intent.desired_value = std::move(desired_value);
    intent.expected_revision = expected_revision;
    intent.created_at_microseconds = now_microseconds;

    if (intent.id.empty() || intent.parameter_id.empty() || intent.desired_value.empty()) {
        intent.diagnostics.push_back({DiagnosticSeverity::error, "incomplete_parameter_write", "The parameter write lacks a stable ID, parameter ID, or desired value.", "Complete the normalized mapping before planning a write."});
        return intent;
    }
    if (!target_writable) {
        intent.diagnostics.push_back({DiagnosticSeverity::warning, "parameter_target_read_only", "The target is not declared writable.", "Preserve the desired value as pending state; do not transmit it."});
        return intent;
    }
    if (!evidence_at_least(mapping_evidence, EvidenceLevel::qualified)) {
        intent.diagnostics.push_back({DiagnosticSeverity::warning, "parameter_mapping_unqualified", "The parameter mapping has not passed its exact device and host qualification gate.", "Qualify range, direction, acknowledgement, recovery, and conflict behavior before transmission."});
        return intent;
    }
    if (timeout_microseconds == 0 || timeout_microseconds > 60'000'000ULL) {
        intent.diagnostics.push_back({DiagnosticSeverity::error, "invalid_parameter_timeout", "The acknowledgement timeout is zero or exceeds the bounded one-minute planning limit.", "Use a bounded timeout appropriate to the documented protocol."});
        return intent;
    }
    if (now_microseconds > std::numeric_limits<std::uint64_t>::max() - timeout_microseconds) {
        intent.diagnostics.push_back({DiagnosticSeverity::error, "parameter_deadline_overflow", "The acknowledgement deadline cannot be represented safely.", "Use a monotonic timestamp with sufficient remaining range."});
        return intent;
    }

    intent.phase = WritePhase::planned;
    intent.deadline_microseconds = now_microseconds + timeout_microseconds;
    intent.diagnostics.push_back({DiagnosticSeverity::info, "parameter_write_planned", "The write is planned but no hardware or DAW message has been sent.", "Dispatch only after the expected revision is revalidated."});
    return intent;
}

bool dispatch_parameter_write(
    ParameterWriteIntent& intent,
    RevisionVector actual_revision,
    std::uint64_t now_microseconds) {
    if (intent.phase != WritePhase::planned) return false;
    if (!same_revision(intent.expected_revision, actual_revision)) {
        intent.phase = WritePhase::conflict;
        intent.diagnostics.push_back({DiagnosticSeverity::warning, "parameter_revision_changed", "The canonical revision changed before dispatch.", "Rebuild the write plan against the current hardware and DAW branches."});
        return false;
    }
    if (now_microseconds < intent.created_at_microseconds || now_microseconds >= intent.deadline_microseconds) {
        intent.phase = WritePhase::timed_out;
        intent.diagnostics.push_back({DiagnosticSeverity::warning, "parameter_write_expired_before_dispatch", "The bounded write plan expired before dispatch.", "Create a new plan from current observations."});
        return false;
    }
    intent.phase = WritePhase::dispatched;
    intent.dispatched_at_microseconds = now_microseconds;
    return true;
}

bool acknowledge_parameter_write(
    ParameterWriteIntent& intent,
    StateObservation observation) {
    if (intent.phase != WritePhase::dispatched) return false;
    if (!acknowledgement_matches_target(intent, observation)) {
        intent.diagnostics.push_back({DiagnosticSeverity::warning, "acknowledgement_wrong_origin", "The observation came from a different side than the write target.", "Keep it as state evidence but do not acknowledge this write."});
        return false;
    }
    if (!evidence_at_least(observation.evidence, EvidenceLevel::observed)) {
        intent.diagnostics.push_back({DiagnosticSeverity::warning, "acknowledgement_unobserved", "The acknowledgement lacks an observed endpoint or project-read event.", "Wait for an actual target observation."});
        return false;
    }
    if (observation.observed_at_microseconds < intent.dispatched_at_microseconds) {
        intent.diagnostics.push_back({DiagnosticSeverity::warning, "acknowledgement_predates_dispatch", "The target observation predates the write dispatch.", "Keep it as historical state and wait for a new target observation."});
        return false;
    }
    if (observation.observed_at_microseconds >= intent.deadline_microseconds) {
        intent.phase = WritePhase::timed_out;
        intent.diagnostics.push_back({DiagnosticSeverity::warning, "acknowledgement_after_deadline", "The target observation arrived after the bounded acknowledgement deadline.", "Reconcile current state and create a new write plan instead of committing the expired write."});
        return false;
    }
    if (!observation_is_current_for_target(intent, observation)) {
        intent.phase = WritePhase::conflict;
        intent.diagnostics.push_back({DiagnosticSeverity::warning, "acknowledgement_stale_revision", "The target observation belongs to an older revision than the write plan.", "Preserve it as history and re-read the current target state."});
        return false;
    }
    intent.acknowledgement = std::move(observation);
    if (intent.acknowledgement->value != intent.desired_value) {
        intent.phase = WritePhase::conflict;
        intent.diagnostics.push_back({DiagnosticSeverity::warning, "acknowledgement_value_mismatch", "The target observation does not match the requested value.", "Preserve both values and open a conflict instead of retrying blindly."});
        return false;
    }
    intent.phase = WritePhase::acknowledged;
    intent.diagnostics.push_back({DiagnosticSeverity::info, "parameter_write_acknowledged", "The target was observed with the requested value.", "A separate workflow transaction may now verify and commit the change."});
    return true;
}

bool expire_parameter_write(ParameterWriteIntent& intent, std::uint64_t now_microseconds) {
    if (intent.phase != WritePhase::planned && intent.phase != WritePhase::dispatched) return false;
    if (now_microseconds < intent.deadline_microseconds) return false;
    intent.phase = WritePhase::timed_out;
    intent.diagnostics.push_back({DiagnosticSeverity::warning, "parameter_acknowledgement_timeout", "The target was not observed with the requested value before the deadline.", "Leave the desired value pending and require a fresh observation before retrying."});
    return true;
}

std::vector<Diagnostic> validate_clock_domains(const std::vector<ClockDomainState>& domains) {
    std::vector<Diagnostic> diagnostics;
    std::set<ClockDomainKind> seen;
    for (const auto& domain : domains) {
        if (!seen.insert(domain.kind).second) {
            diagnostics.push_back({DiagnosticSeverity::error, "duplicate_clock_domain", "More than one state record exists for clock domain '" + to_string(domain.kind) + "'.", "Choose one authority and preserve other sources as observations."});
        }
        if (domain.locked && domain.authority == ClockAuthority::none) {
            diagnostics.push_back({DiagnosticSeverity::error, "clock_lock_without_authority", "Clock domain '" + to_string(domain.kind) + "' is marked locked without an authority.", "Select exactly one documented authority before reporting lock."});
        }
        if (domain.locked && !evidence_at_least(domain.evidence, EvidenceLevel::qualified)) {
            diagnostics.push_back({DiagnosticSeverity::error, "unqualified_clock_lock", "Clock domain '" + to_string(domain.kind) + "' is marked locked without qualified timing evidence.", "Measure the exact route and retain it as observed until its timing and recovery gates pass."});
        }
        if ((domain.kind == ClockDomainKind::audio_sample_clock || domain.kind == ClockDomainKind::word_clock) &&
            domain.locked && domain.nominal_rate <= 0.0) {
            diagnostics.push_back({DiagnosticSeverity::error, "clock_rate_missing", "The locked sample/word-clock domain has no positive nominal rate.", "Record the negotiated sample rate before enabling synchronized audio."});
        }
        if (!std::isfinite(domain.measured_jitter_microseconds) || domain.measured_jitter_microseconds < 0.0 ||
            !std::isfinite(domain.measured_offset_samples)) {
            diagnostics.push_back({DiagnosticSeverity::error, "invalid_clock_measurement", "A clock-domain measurement is negative or non-finite.", "Discard the measurement and repeat the bounded timing probe."});
        }
    }
    return diagnostics;
}

RecordingPlan plan_recording(
    RecordingDestination destination,
    MonitorPath monitor_path,
    RecordingProcessing processing,
    EvidenceLevel route_evidence,
    bool simultaneous_capture_supported,
    bool clean_source_preserved) {
    RecordingPlan plan;
    plan.destination = destination;
    plan.monitor_path = monitor_path;
    plan.processing = processing;
    plan.route_evidence = route_evidence;
    plan.simultaneous_capture_supported = simultaneous_capture_supported;
    plan.clean_source_preserved = clean_source_preserved;

    if (!evidence_at_least(route_evidence, EvidenceLevel::qualified)) {
        plan.diagnostics.push_back({DiagnosticSeverity::warning, "record_route_unqualified", "The selected recording route is not qualified.", "Retain the selection as intent and run endpoint, channel, latency, dropout, and reopen tests."});
        return plan;
    }
    if (destination == RecordingDestination::both && !simultaneous_capture_supported) {
        plan.diagnostics.push_back({DiagnosticSeverity::error, "simultaneous_recording_unavailable", "The selected route cannot prove simultaneous hardware and DAW capture.", "Choose one destination or qualify a device/interface route that supports both."});
        return plan;
    }
    if (processing != RecordingProcessing::clean && !clean_source_preserved) {
        plan.diagnostics.push_back({DiagnosticSeverity::warning, "processed_recording_without_clean_source", "The plan would retain only processed audio.", "Confirm this destructive creative choice or preserve a clean source alongside it."});
        return plan;
    }
    plan.ready = true;
    plan.diagnostics.push_back({DiagnosticSeverity::info, "recording_plan_ready", "The recording destination and monitoring intent are ready for a separately owned audio transaction.", "Revalidate the endpoint and session revision immediately before capture."});
    return plan;
}

HardwareLearnReport assess_hardware_learn(
    std::string profile_id,
    std::vector<HardwareLearnObservation> observations,
    bool feedback_test_requested) {
    HardwareLearnReport report;
    report.profile_id = std::move(profile_id);
    report.observations = std::move(observations);
    report.feedback_test_requested = feedback_test_requested;
    if (report.profile_id.empty()) {
        report.diagnostics.push_back({DiagnosticSeverity::error, "learn_profile_missing", "The hardware-learn session has no profile identity.", "Assign a new user-profile ID before collecting mappings."});
        return report;
    }
    if (feedback_test_requested) {
        report.diagnostics.push_back({DiagnosticSeverity::warning, "learn_feedback_separate_gate", "Outbound LED, motor, display, or control feedback is not authorized by receive-only learning.", "Create a separate bounded write test using documented messages and explicit device evidence."});
    }
    if (report.observations.empty()) {
        report.diagnostics.push_back({DiagnosticSeverity::warning, "learn_observations_empty", "No controls were observed.", "Operate each pad, key, encoder, fader, button, and transport control during a receive-only capture."});
        return report;
    }

    bool complete = true;
    std::set<std::string> control_ids;
    for (const auto& observation : report.observations) {
        if (observation.endpoint_id.empty() || observation.control_id.empty() || observation.message_signature.empty() ||
            observation.sample_count == 0 || !evidence_at_least(observation.evidence, EvidenceLevel::observed)) {
            complete = false;
            report.diagnostics.push_back({DiagnosticSeverity::warning, "learn_observation_incomplete", "A learned control lacks endpoint, identity, message samples, or observed evidence.", "Collect another receive-only sample before saving the profile."});
        }
        if (!observation.control_id.empty() && !control_ids.insert(observation.control_id).second) {
            complete = false;
            report.diagnostics.push_back({DiagnosticSeverity::warning, "learn_control_ambiguous", "A control ID occurs more than once in the learning set.", "Resolve the message ambiguity before saving the profile."});
        }
        if (observation.kind == LearnedControlKind::unknown) {
            complete = false;
            report.diagnostics.push_back({DiagnosticSeverity::warning, "learn_control_unclassified", "A learned message has not been classified as a usable control.", "Classify it manually or retain it as unsupported evidence."});
        }
        if (observation.minimum_value > observation.maximum_value) {
            complete = false;
            report.diagnostics.push_back({DiagnosticSeverity::error, "learn_range_invalid", "A learned control has an invalid value range.", "Collect minimum and maximum values again without changing the device."});
        }
    }
    report.ready_to_save = complete;
    if (complete) {
        report.diagnostics.push_back({DiagnosticSeverity::info, "learn_profile_ready", "The receive-only mapping set is structurally ready to save as an unqualified user profile.", "Keep write feedback disabled until separately qualified."});
    }
    return report;
}

} // namespace ubridge::core
