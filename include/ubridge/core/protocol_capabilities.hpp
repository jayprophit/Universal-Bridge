#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ubridge::core {

// Evidence is deliberately monotonic. A profile declaration can describe a
// possible route, an observation can prove that an endpoint exists, and only a
// qualified route may activate live I/O or write-capable behavior.
enum class EvidenceLevel : std::uint8_t {
    unavailable = 0,
    declared = 1,
    observed = 2,
    qualified = 3,
};

enum class ProtocolKind {
    unknown,
    project_files,
    mass_storage,
    usb_composite,
    usb_midi,
    midi_1,
    midi_2,
    midi_sysex,
    usb_audio,
    analogue_audio,
    hid,
    bluetooth_midi,
    network_midi,
    osc,
    ethernet,
    manufacturer_protocol,
};

enum class ProtocolDirection {
    discovery,
    input,
    output,
    duplex,
};

struct ProtocolEvidence {
    ProtocolKind protocol = ProtocolKind::unknown;
    EvidenceLevel level = EvidenceLevel::unavailable;
    bool readable = false;
    bool writable = false;
    int input_channels = 0;
    int output_channels = 0;
    std::string endpoint_id;
    std::string source;
};

[[nodiscard]] std::string to_string(EvidenceLevel value);
[[nodiscard]] std::string to_string(ProtocolKind value);
[[nodiscard]] bool evidence_at_least(EvidenceLevel actual, EvidenceLevel required) noexcept;
[[nodiscard]] EvidenceLevel strongest_protocol_evidence(
    const std::vector<ProtocolEvidence>& evidence,
    ProtocolKind protocol,
    ProtocolDirection direction) noexcept;

} // namespace ubridge::core
