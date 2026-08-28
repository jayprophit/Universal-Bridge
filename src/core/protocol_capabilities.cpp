#include "ubridge/core/protocol_capabilities.hpp"

namespace ubridge::core {
namespace {

bool supports_direction(const ProtocolEvidence& evidence, ProtocolDirection direction) noexcept {
    switch (direction) {
        case ProtocolDirection::discovery: return true;
        case ProtocolDirection::input: return evidence.readable;
        case ProtocolDirection::output: return evidence.writable;
        case ProtocolDirection::duplex: return evidence.readable && evidence.writable;
    }
    return false;
}

} // namespace

std::string to_string(EvidenceLevel value) {
    switch (value) {
        case EvidenceLevel::unavailable: return "unavailable";
        case EvidenceLevel::declared: return "declared";
        case EvidenceLevel::observed: return "observed";
        case EvidenceLevel::qualified: return "qualified";
    }
    return "unavailable";
}

std::string to_string(ProtocolKind value) {
    switch (value) {
        case ProtocolKind::unknown: return "unknown";
        case ProtocolKind::project_files: return "project_files";
        case ProtocolKind::mass_storage: return "mass_storage";
        case ProtocolKind::usb_composite: return "usb_composite";
        case ProtocolKind::usb_midi: return "usb_midi";
        case ProtocolKind::midi_1: return "midi_1";
        case ProtocolKind::midi_2: return "midi_2";
        case ProtocolKind::midi_sysex: return "midi_sysex";
        case ProtocolKind::usb_audio: return "usb_audio";
        case ProtocolKind::analogue_audio: return "analogue_audio";
        case ProtocolKind::hid: return "hid";
        case ProtocolKind::bluetooth_midi: return "bluetooth_midi";
        case ProtocolKind::network_midi: return "network_midi";
        case ProtocolKind::osc: return "osc";
        case ProtocolKind::ethernet: return "ethernet";
        case ProtocolKind::manufacturer_protocol: return "manufacturer_protocol";
    }
    return "unknown";
}

bool evidence_at_least(EvidenceLevel actual, EvidenceLevel required) noexcept {
    return static_cast<std::uint8_t>(actual) >= static_cast<std::uint8_t>(required);
}

EvidenceLevel strongest_protocol_evidence(
    const std::vector<ProtocolEvidence>& evidence,
    ProtocolKind protocol,
    ProtocolDirection direction) noexcept {
    EvidenceLevel strongest = EvidenceLevel::unavailable;
    for (const auto& item : evidence) {
        if (item.protocol != protocol || !supports_direction(item, direction)) {
            continue;
        }
        if (evidence_at_least(item.level, strongest)) {
            strongest = item.level;
        }
    }
    return strongest;
}

} // namespace ubridge::core
