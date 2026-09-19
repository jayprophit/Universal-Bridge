#include "ubridge/core/midi_mediation.hpp"

#include <algorithm>

namespace ubridge::core {
namespace {

std::size_t endpoint_index(MidiRouteEndpoint endpoint) noexcept {
    return endpoint == MidiRouteEndpoint::hardware ? 0U : 1U;
}

MidiRouteEndpoint opposite(MidiRouteEndpoint endpoint) noexcept {
    return endpoint == MidiRouteEndpoint::hardware ? MidiRouteEndpoint::daw : MidiRouteEndpoint::hardware;
}

bool same_message(const ShortMidiPacket& left, const ShortMidiPacket& right) noexcept {
    if (left.size != right.size) return false;
    for (std::size_t index = 0; index < left.size; ++index) {
        if (left.bytes[index] != right.bytes[index]) return false;
    }
    return true;
}

bool elapsed_within(std::uint64_t earlier, std::uint64_t later, std::uint64_t window) noexcept {
    return later >= earlier && later - earlier <= window;
}

} // namespace

bool valid_short_midi_packet(const ShortMidiPacket& packet) noexcept {
    if (packet.size == 0 || packet.size > packet.bytes.size()) return false;
    const auto status = packet.bytes[0];
    if ((status & 0x80U) == 0) return false;
    if (status >= 0xF8U) return packet.size == 1;
    if (status == 0xF1U || status == 0xF3U) return packet.size == 2 && packet.bytes[1] < 0x80U;
    if (status == 0xF2U) return packet.size == 3 && packet.bytes[1] < 0x80U && packet.bytes[2] < 0x80U;
    if (status >= 0xF0U) return false; // SysEx and variable-length system messages use a different bounded path.
    const auto kind = status & 0xF0U;
    const auto expected = kind == 0xC0U || kind == 0xD0U ? 2U : 3U;
    if (packet.size != expected) return false;
    return packet.bytes[1] < 0x80U && (packet.size == 2 || packet.bytes[2] < 0x80U);
}

bool is_clock_message(const ShortMidiPacket& packet) noexcept { return packet.size == 1 && packet.bytes[0] == 0xF8U; }

bool is_transport_message(const ShortMidiPacket& packet) noexcept {
    if (packet.size == 1) {
        return packet.bytes[0] == 0xFAU || packet.bytes[0] == 0xFBU || packet.bytes[0] == 0xFCU;
    }
    return packet.size == 3 && packet.bytes[0] == 0xF2U;
}

MidiMediator::MidiMediator(MidiMediationPolicy policy) : policy_(policy) {
    transport_.transport_authority = policy_.transport_authority;
    transport_.clock_authority = policy_.clock_authority;
}

MidiRouteDecision MidiMediator::route(MidiRouteEndpoint source, ShortMidiPacket packet) noexcept {
    if (!valid_short_midi_packet(packet)) return MidiRouteDecision::invalid_message;
    const auto target = opposite(source);
    if ((source == MidiRouteEndpoint::hardware && !policy_.hardware_to_daw) ||
        (source == MidiRouteEndpoint::daw && !policy_.daw_to_hardware)) {
        return MidiRouteDecision::direction_blocked;
    }
    if (is_clock_message(packet) && source != policy_.clock_authority) {
        return MidiRouteDecision::authority_blocked;
    }
    if (is_transport_message(packet) && source != policy_.transport_authority) {
        return MidiRouteDecision::authority_blocked;
    }
    if (is_echo(source, packet)) return MidiRouteDecision::echo_suppressed;
    if (!consume_rate_budget(source, packet.timestamp_microseconds)) return MidiRouteDecision::rate_limited;

    const RoutedMidiPacket routed{packet, source, target};
    const bool queued =
        target == MidiRouteEndpoint::hardware ? to_hardware_.try_push(routed) : to_daw_.try_push(routed);
    if (!queued) return MidiRouteDecision::queue_full;
    remember_forward(routed);
    update_transport(source, packet);
    return MidiRouteDecision::forwarded;
}

bool MidiMediator::pop_for_hardware(RoutedMidiPacket& packet) noexcept { return to_hardware_.try_pop(packet); }

bool MidiMediator::pop_for_daw(RoutedMidiPacket& packet) noexcept { return to_daw_.try_pop(packet); }

const TransportMediationState& MidiMediator::transport_state() const noexcept { return transport_; }

const MidiMediationPolicy& MidiMediator::policy() const noexcept { return policy_; }

void MidiMediator::set_policy(MidiMediationPolicy policy) noexcept {
    policy_ = policy;
    transport_.transport_authority = policy_.transport_authority;
    transport_.clock_authority = policy_.clock_authority;
    for (auto& window : rate_windows_)
        window = {};
}

void MidiMediator::reset() noexcept {
    to_hardware_.reset();
    to_daw_.reset();
    echo_records_.fill({});
    next_echo_record_ = 0;
    rate_windows_.fill({});
    transport_ = {};
    transport_.transport_authority = policy_.transport_authority;
    transport_.clock_authority = policy_.clock_authority;
}

bool MidiMediator::is_echo(MidiRouteEndpoint source, const ShortMidiPacket& packet) const noexcept {
    return std::any_of(echo_records_.begin(), echo_records_.end(), [&](const EchoRecord& record) {
        return record.occupied && record.target == source && same_message(record.packet, packet) &&
               elapsed_within(record.packet.timestamp_microseconds, packet.timestamp_microseconds,
                              policy_.echo_window_microseconds);
    });
}

bool MidiMediator::consume_rate_budget(MidiRouteEndpoint source, std::uint64_t now_microseconds) noexcept {
    if (policy_.maximum_messages_per_window == 0 || policy_.rate_window_microseconds == 0) return false;
    auto& window = rate_windows_[endpoint_index(source)];
    if (window.messages == 0 || now_microseconds < window.started_at_microseconds ||
        now_microseconds - window.started_at_microseconds >= policy_.rate_window_microseconds) {
        window.started_at_microseconds = now_microseconds;
        window.messages = 1;
        return true;
    }
    if (window.messages >= policy_.maximum_messages_per_window) return false;
    ++window.messages;
    return true;
}

void MidiMediator::remember_forward(const RoutedMidiPacket& packet) noexcept {
    echo_records_[next_echo_record_] = {packet.packet, packet.target, true};
    next_echo_record_ = (next_echo_record_ + 1U) % echo_records_.size();
}

void MidiMediator::update_transport(MidiRouteEndpoint source, const ShortMidiPacket& packet) noexcept {
    const auto status = packet.bytes[0];
    if (status == 0xF8U) {
        ++transport_.clock_pulses;
        transport_.last_clock_timestamp_microseconds = packet.timestamp_microseconds;
        return;
    }
    if (status == 0xFAU)
        transport_.run_state = TransportRunState::running;
    else if (status == 0xFBU)
        transport_.run_state = TransportRunState::continuing;
    else if (status == 0xFCU)
        transport_.run_state = TransportRunState::stopped;
    else if (status == 0xF2U && packet.size == 3) {
        transport_.song_position_sixteenth_notes =
            static_cast<std::uint32_t>(packet.bytes[1]) | (static_cast<std::uint32_t>(packet.bytes[2]) << 7U);
    } else {
        return;
    }
    transport_.transport_authority = source;
    transport_.last_transport_timestamp_microseconds = packet.timestamp_microseconds;
}

std::string to_string(MidiRouteEndpoint endpoint) {
    return endpoint == MidiRouteEndpoint::hardware ? "hardware" : "daw";
}

std::string to_string(MidiRouteDecision decision) {
    switch (decision) {
    case MidiRouteDecision::forwarded: return "forwarded";
    case MidiRouteDecision::invalid_message: return "invalid_message";
    case MidiRouteDecision::direction_blocked: return "direction_blocked";
    case MidiRouteDecision::authority_blocked: return "authority_blocked";
    case MidiRouteDecision::echo_suppressed: return "echo_suppressed";
    case MidiRouteDecision::rate_limited: return "rate_limited";
    case MidiRouteDecision::queue_full: return "queue_full";
    }
    return "invalid_message";
}

std::string to_string(TransportRunState state) {
    switch (state) {
    case TransportRunState::stopped: return "stopped";
    case TransportRunState::running: return "running";
    case TransportRunState::continuing: return "continuing";
    }
    return "stopped";
}

} // namespace ubridge::core
