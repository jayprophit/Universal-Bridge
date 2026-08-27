#pragma once

#include "ubridge/core/bridge_core.hpp"
#include "ubridge/core/session_tools.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ubridge::performance {

struct TimingMeasurement {
    std::string route_id;
    double sample_rate_hz = 0.0;
    std::int64_t emitted_sample = 0;
    std::int64_t received_sample = 0;
    std::int64_t expected_round_trip_samples = 0;
    double measured_round_trip_ms = 0.0;
    double estimated_one_way_ms = 0.0;
    double correction_samples = 0.0;
    bool valid = false;
    std::vector<core::Diagnostic> diagnostics;
};

struct DriftObservation {
    std::int64_t expected_sample = 0;
    std::int64_t observed_sample = 0;
    double elapsed_seconds = 0.0;
};

struct DriftReport {
    double drift_samples = 0.0;
    double drift_ms = 0.0;
    double drift_ppm = 0.0;
    bool correction_recommended = false;
    std::vector<core::Diagnostic> diagnostics;
};

struct StemMetrics {
    std::string stem_id;
    std::int64_t expected_start_sample = 0;
    std::int64_t observed_start_sample = 0;
    std::int64_t expected_length_samples = 0;
    std::int64_t observed_length_samples = 0;
    std::int64_t tail_samples = 0;
    double peak_dbfs = -120.0;
    double loudness_lufs = -120.0;
    bool completed = false;
};

struct StemValidationPolicy {
    double silence_peak_threshold_dbfs = -90.0;
    double clipping_peak_threshold_dbfs = -0.1;
    std::int64_t start_tolerance_samples = 8;
    std::int64_t length_tolerance_samples = 16;
    bool require_tail_metadata = true;
};

struct StemValidationReport {
    bool acceptable = false;
    std::vector<core::Diagnostic> diagnostics;
};

struct MidiEndpoint {
    std::string id;
    bool input = false;
    bool output = false;
    bool virtual_port = false;
};

struct MidiRoute {
    std::string id;
    std::string source_endpoint_id;
    std::string destination_endpoint_id;
    int channel_filter = -1; // -1 means all channels
    bool allow_thru = false;
};

struct MidiRouteReport {
    bool valid = false;
    std::vector<std::string> loop_route_ids;
    std::vector<std::string> collision_route_ids;
    std::vector<core::Diagnostic> diagnostics;
};

enum class ScaleCurve {
    linear,
    logarithmic,
    bipolar_linear,
};

struct ParameterScale {
    std::string parameter_id;
    double minimum = 0.0;
    double maximum = 1.0;
    ScaleCurve curve = ScaleCurve::linear;
};

struct AutomationTranslationPlan {
    std::string source_curve_id;
    std::string source_parameter_id;
    std::string target_parameter_id;
    bool editable = false;
    bool render_fallback_required = false;
    std::vector<core::Diagnostic> diagnostics;
};

struct MixerRebuildPlan {
    bool structurally_complete = false;
    std::vector<std::string> channel_ids;
    std::vector<std::string> unresolved_route_ids;
    std::vector<core::Diagnostic> diagnostics;
};

[[nodiscard]] std::string to_string(ScaleCurve curve);
[[nodiscard]] TimingMeasurement measure_timing(
    std::string route_id,
    double sample_rate_hz,
    std::int64_t emitted_sample,
    std::int64_t received_sample,
    std::int64_t expected_round_trip_samples);
[[nodiscard]] DriftReport analyze_drift(double sample_rate_hz, const std::vector<DriftObservation>& observations, double warning_ppm);
[[nodiscard]] StemValidationReport validate_stem(const StemMetrics& metrics, const StemValidationPolicy& policy);
[[nodiscard]] MidiRouteReport validate_midi_routes(const std::vector<MidiEndpoint>& endpoints, const std::vector<MidiRoute>& routes);
[[nodiscard]] std::vector<core::Diagnostic> validate_midi_events(const std::vector<core::MusicalEvent>& events);
[[nodiscard]] double scale_normalized(double normalized_value, const ParameterScale& scale);
[[nodiscard]] AutomationTranslationPlan plan_automation_translation(
    const session::AutomationCurve& curve,
    const std::string& target_parameter_id,
    bool target_supports_editable_automation);
[[nodiscard]] MixerRebuildPlan plan_mixer_rebuild(const session::FullSession& session);

} // namespace ubridge::performance
