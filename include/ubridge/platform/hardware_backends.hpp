#pragma once

#include "ubridge/core/bridge_core.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ubridge::platform {

enum class BackendMaturity { unavailable, scaffold, experimental, qualified };
enum class EndpointDirection { input, output };

struct MidiEndpoint {
    std::string id;
    std::string name;
    std::string manufacturer;
    EndpointDirection direction = EndpointDirection::input;
    std::string backend;
    std::string protocol;
    bool available = false;
};

struct MidiMessage {
    std::vector<std::uint8_t> bytes;
    std::uint64_t timestamp_microseconds = 0;
};

struct AudioEndpoint {
    std::string id;
    std::string name;
    EndpointDirection direction = EndpointDirection::input;
    bool active = false;
    std::uint32_t sample_rate = 0;
    std::uint16_t channels = 0;
    std::uint16_t bits_per_sample = 0;
};

struct AudioSignalProbe {
    bool opened = false;
    std::uint64_t frames_observed = 0;
    float peak = 0.0F;
    std::string status;
};

// Persist portable intent, not an operating-system endpoint ID. Runtime IDs are
// deliberately resolved again whenever hardware is attached or the OS changes.
struct EndpointMatchRule {
    EndpointDirection direction = EndpointDirection::input;
    std::vector<std::string> name_hints;
    std::string backend_hint;
    std::string protocol_hint;
    std::uint16_t minimum_channels = 0;
};

struct EndpointMatch {
    std::string endpoint_id;
    std::string endpoint_name;
    int score = 0;
    bool ambiguous = false;
    std::string explanation;
};

struct UsbInterface {
    std::string instance_id;
    std::string container_id;
    std::string interface_number;
    std::string service;
    std::string friendly_name;
};

struct DiscoveredDevice {
    std::uint16_t vendor_id = 0;
    std::uint16_t product_id = 0;
    std::string container_id;
    std::string display_name;
    std::vector<UsbInterface> interfaces;
    BackendMaturity maturity = BackendMaturity::experimental;
    std::vector<core::Diagnostic> diagnostics;
};

class IDeviceDiscovery {
public:
    virtual ~IDeviceDiscovery() = default;
    [[nodiscard]] virtual std::vector<DiscoveredDevice> enumerate() = 0;
};

class IMidiBackend {
public:
    using ReceiveCallback = std::function<void(const MidiMessage&)>;
    virtual ~IMidiBackend() = default;
    [[nodiscard]] virtual BackendMaturity maturity() const noexcept = 0;
    [[nodiscard]] virtual std::vector<MidiEndpoint> enumerate_endpoints() const = 0;
    virtual bool open_input(std::string_view endpoint_id, ReceiveCallback callback) = 0;
    virtual bool open_output(std::string_view endpoint_id) = 0;
    virtual void close(std::string_view endpoint_id) noexcept = 0;
    [[nodiscard]] virtual bool connected(std::string_view endpoint_id) const noexcept = 0;
    virtual bool send(std::string_view endpoint_id, std::span<const std::uint8_t> message) = 0;
};

class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;
    [[nodiscard]] virtual BackendMaturity maturity() const noexcept = 0;
    [[nodiscard]] virtual std::vector<AudioEndpoint> enumerate_endpoints() const = 0;
    [[nodiscard]] virtual AudioSignalProbe probe_input(std::string_view endpoint_id, std::uint32_t duration_ms) = 0;
    virtual bool begin_capture(std::string_view endpoint_id, int sample_rate, int channels) = 0;
    virtual void stop_capture() noexcept = 0;
};

[[nodiscard]] std::unique_ptr<IDeviceDiscovery> make_system_device_discovery();
[[nodiscard]] std::unique_ptr<IMidiBackend> make_system_midi_backend();
[[nodiscard]] std::unique_ptr<IAudioBackend> make_system_audio_backend();
[[nodiscard]] std::string to_string(BackendMaturity maturity);
[[nodiscard]] std::string to_string(EndpointDirection direction);
[[nodiscard]] std::string midi_message_semantic(std::span<const std::uint8_t> message);
[[nodiscard]] EndpointMatch resolve_midi_endpoint(const std::vector<MidiEndpoint>& endpoints, const EndpointMatchRule& rule);
[[nodiscard]] EndpointMatch resolve_audio_endpoint(const std::vector<AudioEndpoint>& endpoints, const EndpointMatchRule& rule);
[[nodiscard]] std::vector<core::ProtocolEvidence> protocol_evidence_for(const DiscoveredDevice& device);

inline constexpr std::uint16_t mpc_sample_vendor_id = 0x09E8;
inline constexpr std::uint16_t mpc_sample_product_id = 0x205C;

} // namespace ubridge::platform
