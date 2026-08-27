#include "ubridge/core/performance_tools.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <utility>

namespace ubridge::performance {
namespace {

bool has_error(const std::vector<core::Diagnostic>& diagnostics) {
    return std::any_of(diagnostics.begin(), diagnostics.end(), [](const core::Diagnostic& diagnostic) {
        return diagnostic.severity == core::DiagnosticSeverity::error;
    });
}

} // namespace

std::string to_string(ScaleCurve curve) {
    switch (curve) {
        case ScaleCurve::linear: return "linear";
        case ScaleCurve::logarithmic: return "logarithmic";
        case ScaleCurve::bipolar_linear: return "bipolar_linear";
    }
    return "linear";
}

TimingMeasurement measure_timing(
    std::string route_id,
    double sample_rate_hz,
    std::int64_t emitted_sample,
    std::int64_t received_sample,
    std::int64_t expected_round_trip_samples) {
    TimingMeasurement measurement;
    measurement.route_id = std::move(route_id);
    measurement.sample_rate_hz = sample_rate_hz;
    measurement.emitted_sample = emitted_sample;
    measurement.received_sample = received_sample;
    measurement.expected_round_trip_samples = expected_round_trip_samples;

    if (sample_rate_hz <= 0.0) {
        measurement.diagnostics.push_back({core::DiagnosticSeverity::error, "invalid_sample_rate", "Timing measurement requires a positive sample rate.", "Provide the actual audio interface sample rate before calculating latency."});
        return measurement;
    }
    if (received_sample < emitted_sample) {
        measurement.diagnostics.push_back({core::DiagnosticSeverity::error, "negative_round_trip", "The received marker precedes the emitted marker.", "Verify timestamps and clock domain before applying any correction."});
        return measurement;
    }
    if (expected_round_trip_samples < 0) {
        measurement.diagnostics.push_back({core::DiagnosticSeverity::error, "invalid_expected_round_trip", "Expected round-trip sample count cannot be negative.", "Provide zero or a positive known-path offset."});
        return measurement;
    }

    const auto round_trip_samples = received_sample - emitted_sample;
    measurement.measured_round_trip_ms = (static_cast<double>(round_trip_samples) * 1000.0) / sample_rate_hz;
    measurement.estimated_one_way_ms = measurement.measured_round_trip_ms / 2.0;
    measurement.correction_samples = static_cast<double>(round_trip_samples - expected_round_trip_samples);
    measurement.valid = true;
    measurement.diagnostics.push_back({core::DiagnosticSeverity::info, "timing_measurement", "Timing measurement calculated from supplied marker positions; no physical calibration was performed.", "Apply the correction only after repeatable loopback validation on the exact route."});
    return measurement;
}

DriftReport analyze_drift(double sample_rate_hz, const std::vector<DriftObservation>& observations, double warning_ppm) {
    DriftReport report;
    if (sample_rate_hz <= 0.0 || observations.size() < 2) {
        report.diagnostics.push_back({core::DiagnosticSeverity::error, "insufficient_drift_data", "Drift analysis requires a positive sample rate and at least two observations.", "Collect timestamped markers at the beginning and end of the test interval."});
        return report;
    }
    const auto first = observations.front();
    const auto last = observations.back();
    const auto expected_span = last.expected_sample - first.expected_sample;
    const auto observed_span = last.observed_sample - first.observed_sample;
    if (expected_span <= 0 || last.elapsed_seconds <= first.elapsed_seconds) {
        report.diagnostics.push_back({core::DiagnosticSeverity::error, "invalid_drift_interval", "Drift observations must advance in both expected time and elapsed seconds.", "Sort and validate measurement points before analysis."});
        return report;
    }

    report.drift_samples = static_cast<double>(observed_span - expected_span);
    report.drift_ms = (report.drift_samples * 1000.0) / sample_rate_hz;
    report.drift_ppm = (report.drift_samples * 1000000.0) / static_cast<double>(expected_span);
    report.correction_recommended = std::abs(report.drift_ppm) >= std::abs(warning_ppm);
    report.diagnostics.push_back({
        report.correction_recommended ? core::DiagnosticSeverity::warning : core::DiagnosticSeverity::info,
        report.correction_recommended ? "timing_drift_detected" : "timing_drift_within_threshold",
        "Drift was calculated from supplied observations; no live clock correction was sent.",
        report.correction_recommended ? "Review the route and apply a user-approved non-destructive correction plan." : "Retain observations for future route calibration."
    });
    return report;
}

StemValidationReport validate_stem(const StemMetrics& metrics, const StemValidationPolicy& policy) {
    StemValidationReport report;
    if (metrics.stem_id.empty()) {
        report.diagnostics.push_back({core::DiagnosticSeverity::error, "empty_stem_id", "A stem validation record requires a stable stem ID.", "Assign a stable source track/pad/take identity."});
    }
    if (!metrics.completed) {
        report.diagnostics.push_back({core::DiagnosticSeverity::error, "incomplete_stem_capture", "The stem capture did not complete.", "Keep the partial take but do not place it into the final DAW arrangement."});
    }
    if (metrics.observed_length_samples <= 0) {
        report.diagnostics.push_back({core::DiagnosticSeverity::error, "empty_stem", "The observed stem contains no audio samples.", "Check capture routing and source playback before retrying."});
    }
    if (metrics.expected_length_samples > 0 && std::llabs(metrics.observed_length_samples - metrics.expected_length_samples) > policy.length_tolerance_samples) {
        report.diagnostics.push_back({core::DiagnosticSeverity::warning, "stem_length_mismatch", "The stem length differs from the expected timeline length.", "Preserve the take and review loop, tail, and recording-stop policy before alignment."});
    }
    if (std::llabs(metrics.observed_start_sample - metrics.expected_start_sample) > policy.start_tolerance_samples) {
        report.diagnostics.push_back({core::DiagnosticSeverity::warning, "stem_start_offset", "The stem start differs from the expected origin.", "Use a measured route correction instead of blindly moving all stems."});
    }
    if (metrics.peak_dbfs <= policy.silence_peak_threshold_dbfs) {
        report.diagnostics.push_back({core::DiagnosticSeverity::error, "silent_stem", "Stem peak is at or below the silence threshold.", "Confirm source solo/mute state and capture input before retrying."});
    }
    if (metrics.peak_dbfs >= policy.clipping_peak_threshold_dbfs) {
        report.diagnostics.push_back({core::DiagnosticSeverity::warning, "clipping_risk", "Stem peak is at or above the clipping warning threshold.", "Keep the raw take and correct gain staging before an approved recapture."});
    }
    if (metrics.tail_samples < 0) {
        report.diagnostics.push_back({core::DiagnosticSeverity::error, "invalid_tail", "Tail sample count cannot be negative.", "Record a non-negative tail policy value."});
    }
    if (policy.require_tail_metadata && metrics.tail_samples == 0) {
        report.diagnostics.push_back({core::DiagnosticSeverity::warning, "tail_not_declared", "No reverb/delay tail was declared for this stem.", "Confirm that a zero tail is intentional before finalizing the export."});
    }
    report.acceptable = !has_error(report.diagnostics);
    return report;
}

MidiRouteReport validate_midi_routes(const std::vector<MidiEndpoint>& endpoints, const std::vector<MidiRoute>& routes) {
    MidiRouteReport report;
    std::map<std::string, MidiEndpoint> endpoint_map;
    std::set<std::string> route_keys;
    for (const auto& endpoint : endpoints) {
        if (endpoint.id.empty()) {
            report.diagnostics.push_back({core::DiagnosticSeverity::error, "empty_midi_endpoint", "A MIDI endpoint has no stable identity.", "Use a backend-provided persistent endpoint ID before routing."});
        } else if (!endpoint_map.emplace(endpoint.id, endpoint).second) {
            report.diagnostics.push_back({core::DiagnosticSeverity::error, "duplicate_midi_endpoint", "More than one MIDI endpoint uses ID '" + endpoint.id + "'.", "Resolve duplicate endpoint identities before routing."});
        }
    }
    for (const auto& route : routes) {
        const auto source = endpoint_map.find(route.source_endpoint_id);
        const auto destination = endpoint_map.find(route.destination_endpoint_id);
        if (source == endpoint_map.end() || destination == endpoint_map.end()) {
            report.diagnostics.push_back({core::DiagnosticSeverity::error, "unknown_midi_endpoint", "Route '" + route.id + "' references an unknown endpoint.", "Refresh endpoint discovery and require user review before activating routes."});
            continue;
        }
        if (!source->second.output || !destination->second.input) {
            report.diagnostics.push_back({core::DiagnosticSeverity::error, "invalid_midi_direction", "Route '" + route.id + "' does not connect an output to an input.", "Reverse or repair the route direction."});
        }
        if (route.channel_filter < -1 || route.channel_filter > 15) {
            report.diagnostics.push_back({core::DiagnosticSeverity::error, "invalid_midi_channel", "Route '" + route.id + "' has an invalid channel filter.", "Use -1 for all channels or a MIDI channel from 0 through 15."});
        }
        if (route.source_endpoint_id == route.destination_endpoint_id && !route.allow_thru) {
            report.loop_route_ids.push_back(route.id);
            report.diagnostics.push_back({core::DiagnosticSeverity::error, "midi_feedback_loop", "Route '" + route.id + "' loops an endpoint to itself.", "Disable the route or explicitly configure a safely qualified Thru policy."});
        }
        const auto route_key = route.source_endpoint_id + "→" + route.destination_endpoint_id + "#" + std::to_string(route.channel_filter);
        if (!route_keys.insert(route_key).second) {
            report.collision_route_ids.push_back(route.id);
            report.diagnostics.push_back({core::DiagnosticSeverity::warning, "duplicate_midi_route", "Route '" + route.id + "' duplicates a source/destination/channel path.", "Merge duplicate routing rules to avoid doubled messages."});
        }
    }
    report.valid = !has_error(report.diagnostics);
    return report;
}

std::vector<core::Diagnostic> validate_midi_events(const std::vector<core::MusicalEvent>& events) {
    std::vector<core::Diagnostic> diagnostics;
    for (const auto& event : events) {
        if (event.id.empty() || event.track_id.empty()) {
            diagnostics.push_back({core::DiagnosticSeverity::error, "midi_event_identity_missing", "A MIDI event has no stable event or track identity.", "Assign event and track IDs before sync or export."});
        }
        if (event.channel < 0 || event.channel > 15) {
            diagnostics.push_back({core::DiagnosticSeverity::error, "midi_event_invalid_channel", "MIDI event '" + event.id + "' has an invalid channel.", "Use a channel from 0 through 15."});
        }
        if (event.note >= 0 && (event.note > 127 || event.velocity < 0 || event.velocity > 127)) {
            diagnostics.push_back({core::DiagnosticSeverity::error, "midi_note_out_of_range", "MIDI note event '" + event.id + "' has an invalid note or velocity.", "Use note and velocity values from 0 through 127."});
        }
        if (event.note < 0 && !event.cc.has_value()) {
            diagnostics.push_back({core::DiagnosticSeverity::warning, "unclassified_midi_event", "MIDI event '" + event.id + "' is neither a note nor a CC event.", "Preserve it as metadata until a profile defines its message semantics."});
        }
        if (event.duration_ticks < 0) {
            diagnostics.push_back({core::DiagnosticSeverity::error, "negative_note_duration", "MIDI event '" + event.id + "' has negative duration.", "Repair the source event ordering before export."});
        }
    }
    return diagnostics;
}

double scale_normalized(double normalized_value, const ParameterScale& scale) {
    const auto normalized = std::clamp(normalized_value, 0.0, 1.0);
    if (scale.maximum < scale.minimum) {
        return scale.minimum;
    }
    if (scale.curve == ScaleCurve::logarithmic && scale.minimum > 0.0 && scale.maximum > 0.0) {
        return scale.minimum * std::pow(scale.maximum / scale.minimum, normalized);
    }
    return scale.minimum + ((scale.maximum - scale.minimum) * normalized);
}

AutomationTranslationPlan plan_automation_translation(
    const session::AutomationCurve& curve,
    const std::string& target_parameter_id,
    bool target_supports_editable_automation) {
    AutomationTranslationPlan plan;
    plan.source_curve_id = curve.id;
    plan.source_parameter_id = curve.target_parameter_id;
    plan.target_parameter_id = target_parameter_id;
    plan.editable = curve.source_editable && target_supports_editable_automation && !target_parameter_id.empty();
    plan.render_fallback_required = !plan.editable;
    if (curve.id.empty() || curve.target_parameter_id.empty()) {
        plan.diagnostics.push_back({core::DiagnosticSeverity::error, "automation_source_incomplete", "Automation cannot be translated without a curve and source parameter identity.", "Retain the source data as metadata until identities are repaired."});
    }
    if (!plan.editable) {
        plan.diagnostics.push_back({core::DiagnosticSeverity::warning, "automation_render_fallback", "Editable automation translation is unavailable for this source/target combination.", "Preserve the curve in the neutral session and offer a user-approved wet/dry or rendered fallback."});
    }
    return plan;
}

MixerRebuildPlan plan_mixer_rebuild(const session::FullSession& session) {
    MixerRebuildPlan plan;
    std::set<std::string> channel_ids;
    for (const auto& channel : session.mixer) {
        if (!channel.id.empty()) {
            channel_ids.insert(channel.id);
            plan.channel_ids.push_back(channel.id);
        }
    }
    for (const auto& edge : session.routing) {
        if (!channel_ids.contains(edge.from_id) || !channel_ids.contains(edge.to_id)) {
            plan.unresolved_route_ids.push_back(edge.from_id + "→" + edge.to_id);
        }
    }
    plan.structurally_complete = !session.mixer.empty() && plan.unresolved_route_ids.empty();
    if (!plan.structurally_complete) {
        plan.diagnostics.push_back({core::DiagnosticSeverity::warning, "mixer_rebuild_incomplete", "The mixer/routing graph cannot yet be reconstructed without unresolved references.", "Retain the canonical mixer data and generate a reviewable host import plan."});
    } else {
        plan.diagnostics.push_back({core::DiagnosticSeverity::info, "mixer_graph_ready", "The canonical mixer and routing graph is structurally complete.", "Use a qualified DAW adapter before creating native tracks, buses, or effects."});
    }
    return plan;
}

} // namespace ubridge::performance
