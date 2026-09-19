#pragma once

#include "ubridge/core/state_sync.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ubridge::core {

enum class ParameterMappingDirection {
    hardware_to_daw,
    daw_to_hardware,
    bidirectional,
};

enum class ParameterMappingCurve {
    linear,
    stepped,
};

struct ParameterEndpointMapping {
    std::string endpoint_parameter_id;
    double minimum = 0.0;
    double maximum = 1.0;
    std::uint32_t steps = 0;
    bool readable = false;
    bool writable = false;
};

struct QualifiedParameterMapping {
    std::string mapping_id;
    std::string canonical_parameter_id;
    std::string unit;
    ParameterMappingDirection direction = ParameterMappingDirection::hardware_to_daw;
    ParameterMappingCurve curve = ParameterMappingCurve::linear;
    ParameterEndpointMapping hardware;
    ParameterEndpointMapping daw;
    EvidenceLevel evidence = EvidenceLevel::unavailable;
    bool acknowledgement_required = true;
    std::uint64_t acknowledgement_timeout_microseconds = 250'000;
    std::uint32_t maximum_updates_per_second = 60;
};

struct ParameterRelayPlan {
    bool allowed = false;
    StateOrigin source = StateOrigin::bridge;
    WriteTarget target = WriteTarget::hardware;
    double normalized_value = 0.0;
    double target_value = 0.0;
    ParameterWriteIntent write;
    std::vector<Diagnostic> diagnostics;
};

[[nodiscard]] std::vector<Diagnostic> validate_parameter_mapping(const QualifiedParameterMapping& mapping);
[[nodiscard]] ParameterRelayPlan plan_parameter_relay(const QualifiedParameterMapping& mapping, StateOrigin source,
                                                      double source_value, RevisionVector expected_revision,
                                                      std::uint64_t now_microseconds);
[[nodiscard]] std::string to_string(ParameterMappingDirection direction);
[[nodiscard]] std::string to_string(ParameterMappingCurve curve);

} // namespace ubridge::core
