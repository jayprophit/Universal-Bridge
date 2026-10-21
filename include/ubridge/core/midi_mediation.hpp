#pragma once

#include "ubridge/core/realtime_buffer.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace ubridge::core {

enum class MidiRouteEndpoint {
    hardware,
    daw,
};

enum class MidiRouteDecision {
    forwarded,
    invalid_message,
    direction_blocked,
    authority_blocked,
    echo_suppressed,
    rate_limited,
    queue_full,
};

enum class TransportRunState {
    stopped,
    running,
    continuing,
};

struct ShortMidiPacket {
    std::array<std::uint8_t, 3> bytes{};
    std::uint8_t size = 0;
    std::uint64_t timestamp_microseconds = 0;
    std::uint64_t sequence = 0;
};

struct RoutedMidiPacket {
    ShortMidiPacket packet;
    MidiRouteEndpoint source = MidiRouteEndpoint::hardware;
    MidiRouteEndpoint target = MidiRouteEndpoint::daw;
};

struct MidiMediationPolicy {
    MidiRouteEndpoint transport_authority = MidiRouteEndpoint::daw;
    MidiRouteEndpoint clock_authority = MidiRouteEndpoint::daw;
    bool hardware_to_daw = true;
    bool daw_to_hardware = true;
    std::uint32_t maximum_messages_per_window = 512;
    std::uint64_t rate_window_microseconds = 10'000;
    std::uint64_t echo_window_microseconds = 5'000;
};

struct TransportMediationState {
    TransportRunState run_state = TransportRunState::stopped;
    MidiRouteEndpoint transport_authority = MidiRouteEndpoint::daw;
    MidiRouteEndpoint clock_authority = MidiRouteEndpoint::daw;
    std::uint64_t clock_pulses = 0;
    std::uint32_t song_position_sixteenth_notes = 0;
    std::uint64_t last_transport_timestamp_microseconds = 0;
    std::uint64_t last_clock_timestamp_microseconds = 0;
};

// Policy and routing state are owned by one mediation thread. Each destination
// queue may have its own consumer; do not call route/set_policy/reset concurrently.
class MidiMediator {
  public:
    explicit MidiMediator(MidiMediationPolicy policy = {});

    [[nodiscard]] MidiRouteDecision route(MidiRouteEndpoint source, ShortMidiPacket packet) noexcept;
    [[nodiscard]] bool pop_for_hardware(RoutedMidiPacket& packet) noexcept;
    [[nodiscard]] bool pop_for_daw(RoutedMidiPacket& packet) noexcept;
    [[nodiscard]] const TransportMediationState& transport_state() const noexcept;
    [[nodiscard]] const MidiMediationPolicy& policy() const noexcept;
    void set_policy(MidiMediationPolicy policy) noexcept;
    void reset() noexcept;

  private:
    struct EchoRecord {
        ShortMidiPacket packet;
        MidiRouteEndpoint target = MidiRouteEndpoint::hardware;
        bool occupied = false;
    };

    struct RateWindow {
        std::uint64_t started_at_microseconds = 0;
        std::uint32_t messages = 0;
    };

    [[nodiscard]] bool is_echo(MidiRouteEndpoint source, const ShortMidiPacket& packet) const noexcept;
    [[nodiscard]] bool consume_rate_budget(MidiRouteEndpoint source, std::uint64_t now_microseconds) noexcept;
    void remember_forward(const RoutedMidiPacket& packet) noexcept;
    void update_transport(MidiRouteEndpoint source, const ShortMidiPacket& packet) noexcept;

    MidiMediationPolicy policy_;
    TransportMediationState transport_;
    SpscRingBuffer<RoutedMidiPacket, 1024> to_hardware_;
    SpscRingBuffer<RoutedMidiPacket, 1024> to_daw_;
    std::array<EchoRecord, 64> echo_records_{};
    std::size_t next_echo_record_ = 0;
    std::array<RateWindow, 2> rate_windows_{};
};

[[nodiscard]] bool valid_short_midi_packet(const ShortMidiPacket& packet) noexcept;
[[nodiscard]] bool is_transport_message(const ShortMidiPacket& packet) noexcept;
[[nodiscard]] bool is_clock_message(const ShortMidiPacket& packet) noexcept;
[[nodiscard]] std::string to_string(MidiRouteEndpoint endpoint);
[[nodiscard]] std::string to_string(MidiRouteDecision decision);
[[nodiscard]] std::string to_string(TransportRunState state);

} // namespace ubridge::core
