#include "ubridge/core/parameter_sync.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace ubridge::core {
namespace {

bool valid_endpoint(const ParameterEndpointMapping& endpoint) noexcept {
    return !endpoint.endpoint_parameter_id.empty() && std::isfinite(endpoint.minimum) &&
           std::isfinite(endpoint.maximum) && endpoint.maximum > endpoint.minimum;
}

bool source_allowed(ParameterMappingDirection direction, StateOrigin source) noexcept {
    if (source == StateOrigin::hardware) {
        return direction == ParameterMappingDirection::hardware_to_daw ||
               direction == ParameterMappingDirection::bidirectional;
    }
    if (source == StateOrigin::daw) {
        return direction == ParameterMappingDirection::daw_to_hardware ||
               direction == ParameterMappingDirection::bidirectional;
    }
    return false;
}

double normalize_value(const ParameterEndpointMapping& endpoint, ParameterMappingCurve curve, double value) noexcept {
    auto normalized = (value - endpoint.minimum) / (endpoint.maximum - endpoint.minimum);
    normalized = std::clamp(normalized, 0.0, 1.0);
    if (curve == ParameterMappingCurve::stepped && endpoint.steps > 1) {
        const auto intervals = static_cast<double>(endpoint.steps - 1U);
        normalized = std::round(normalized * intervals) / intervals;
    }
    return normalized;
}

double denormalize_value(const ParameterEndpointMapping& endpoint, ParameterMappingCurve curve, double value) noexcept {
    auto target = endpoint.minimum + std::clamp(value, 0.0, 1.0) * (endpoint.maximum - endpoint.minimum);
    if (curve == ParameterMappingCurve::stepped && endpoint.steps > 1) {
        const auto interval = (endpoint.maximum - endpoint.minimum) / static_cast<double>(endpoint.steps - 1U);
        target = endpoint.minimum + std::round((target - endpoint.minimum) / interval) * interval;
    }
    return std::clamp(target, endpoint.minimum, endpoint.maximum);
}

std::string stable_number(double value) {
    std::ostringstream output;
    output << std::setprecision(17) << value;
    return output.str();
}

} // namespace

std::vector<Diagnostic> validate_parameter_mapping(const QualifiedParameterMapping& mapping) {
    std::vector<Diagnostic> diagnostics;
    if (mapping.mapping_id.empty() || mapping.canonical_parameter_id.empty() || mapping.unit.empty()) {
        diagnostics.push_back({DiagnosticSeverity::error, "parameter_mapping_identity_missing",
                               "A parameter mapping lacks a stable mapping ID, canonical parameter ID, or unit.",
                               "Complete the semantic mapping before it can plan live work."});
    }
    if (!valid_endpoint(mapping.hardware) || !valid_endpoint(mapping.daw)) {
        diagnostics.push_back({DiagnosticSeverity::error, "parameter_mapping_range_invalid",
                               "A hardware or DAW endpoint has no identity or a non-finite/empty range.",
                               "Record explicit endpoint identities and ordered finite ranges."});
    }
    if (mapping.curve == ParameterMappingCurve::stepped && (mapping.hardware.steps < 2 || mapping.daw.steps < 2)) {
        diagnostics.push_back({DiagnosticSeverity::error, "parameter_mapping_steps_invalid",
                               "A stepped mapping must declare at least two values on both sides.",
                               "Qualify discrete value counts and boundary behavior."});
    }
    if (mapping.direction == ParameterMappingDirection::hardware_to_daw &&
        (!mapping.hardware.readable || !mapping.daw.writable)) {
        diagnostics.push_back({DiagnosticSeverity::error, "parameter_mapping_direction_unavailable",
                               "The hardware-to-DAW direction lacks a readable source or writable target.",
                               "Keep the mapping disabled until both endpoint capabilities are observed."});
    }
    if (mapping.direction == ParameterMappingDirection::daw_to_hardware &&
        (!mapping.daw.readable || !mapping.hardware.writable)) {
        diagnostics.push_back({DiagnosticSeverity::error, "parameter_mapping_direction_unavailable",
                               "The DAW-to-hardware direction lacks a readable source or writable target.",
                               "Keep the mapping disabled until both endpoint capabilities are observed."});
    }
    if (mapping.direction == ParameterMappingDirection::bidirectional &&
        (!mapping.hardware.readable || !mapping.hardware.writable || !mapping.daw.readable || !mapping.daw.writable)) {
        diagnostics.push_back({DiagnosticSeverity::error, "parameter_mapping_bidirectional_unavailable",
                               "A bidirectional mapping requires read and write evidence on both endpoints.",
                               "Downgrade the direction or qualify all four endpoint operations."});
    }
    if (!mapping.acknowledgement_required) {
        diagnostics.push_back({DiagnosticSeverity::error, "parameter_mapping_acknowledgement_required",
                               "Live relay mappings must require an observed target acknowledgement.",
                               "Keep fire-and-forget messages outside the committed synchronization path."});
    }
    if (mapping.acknowledgement_timeout_microseconds == 0 ||
        mapping.acknowledgement_timeout_microseconds > 60'000'000ULL || mapping.maximum_updates_per_second == 0 ||
        mapping.maximum_updates_per_second > 1'000) {
        diagnostics.push_back({DiagnosticSeverity::error, "parameter_mapping_bounds_invalid",
                               "The mapping timeout or update-rate bound is unsafe.",
                               "Use a timeout up to one minute and a rate from 1 to 1000 updates per second."});
    }
    if (!evidence_at_least(mapping.evidence, EvidenceLevel::qualified)) {
        diagnostics.push_back({DiagnosticSeverity::warning, "parameter_mapping_unqualified",
                               "The mapping has not passed exact device and DAW qualification.",
                               "Retain it as disabled intent and collect range, direction, acknowledgement, echo, "
                               "saturation and recovery evidence."});
    }
    return diagnostics;
}

ParameterRelayPlan plan_parameter_relay(const QualifiedParameterMapping& mapping, StateOrigin source,
                                        double source_value, RevisionVector expected_revision,
                                        std::uint64_t now_microseconds) {
    ParameterRelayPlan plan;
    plan.source = source;
    plan.target = source == StateOrigin::hardware ? WriteTarget::daw : WriteTarget::hardware;
    plan.diagnostics = validate_parameter_mapping(mapping);
    if (std::any_of(plan.diagnostics.begin(), plan.diagnostics.end(),
                    [](const Diagnostic& diagnostic) { return diagnostic.severity == DiagnosticSeverity::error; }) ||
        !evidence_at_least(mapping.evidence, EvidenceLevel::qualified)) {
        return plan;
    }
    if (!source_allowed(mapping.direction, source)) {
        plan.diagnostics.push_back({DiagnosticSeverity::error, "parameter_relay_direction_blocked",
                                    "The observation origin is not authorized by this mapping direction.",
                                    "Use the declared direction or create a separately qualified mapping."});
        return plan;
    }
    const auto& source_endpoint = source == StateOrigin::hardware ? mapping.hardware : mapping.daw;
    const auto& target_endpoint = source == StateOrigin::hardware ? mapping.daw : mapping.hardware;
    if (!std::isfinite(source_value) || source_value < source_endpoint.minimum ||
        source_value > source_endpoint.maximum) {
        plan.diagnostics.push_back({DiagnosticSeverity::error, "parameter_relay_value_out_of_range",
                                    "The observed source value is non-finite or outside the qualified mapping range.",
                                    "Preserve the raw observation and do not clamp an unqualified value silently."});
        return plan;
    }
    plan.normalized_value = normalize_value(source_endpoint, mapping.curve, source_value);
    plan.target_value = denormalize_value(target_endpoint, mapping.curve, plan.normalized_value);
    plan.write = plan_parameter_write(mapping.mapping_id + "-" + std::to_string(now_microseconds),
                                      mapping.canonical_parameter_id, plan.target, stable_number(plan.target_value),
                                      expected_revision, mapping.evidence, target_endpoint.writable, now_microseconds,
                                      mapping.acknowledgement_timeout_microseconds);
    plan.allowed = plan.write.phase == WritePhase::planned;
    plan.diagnostics.insert(plan.diagnostics.end(), plan.write.diagnostics.begin(), plan.write.diagnostics.end());
    return plan;
}

std::string to_string(ParameterMappingDirection direction) {
    switch (direction) {
    case ParameterMappingDirection::hardware_to_daw: return "hardware_to_daw";
    case ParameterMappingDirection::daw_to_hardware: return "daw_to_hardware";
    case ParameterMappingDirection::bidirectional: return "bidirectional";
    }
    return "hardware_to_daw";
}

std::string to_string(ParameterMappingCurve curve) {
    return curve == ParameterMappingCurve::linear ? "linear" : "stepped";
}

} // namespace ubridge::core
