#pragma once

#include "ubridge/core/bridge_core.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ubridge::platform {

enum class BackendMaturity { unavailable, scaffold, experimental, qualified };

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
    virtual ~IMidiBackend() = default;
    [[nodiscard]] virtual BackendMaturity maturity() const noexcept = 0;
    [[nodiscard]] virtual std::vector<std::string> endpoint_ids() const = 0;
    virtual bool send(std::string_view endpoint_id, std::span<const std::uint8_t> message) = 0;
};

class IAudioBackend {
public:
    virtual ~IAudioBackend() = default;
    [[nodiscard]] virtual BackendMaturity maturity() const noexcept = 0;
    [[nodiscard]] virtual std::vector<std::string> endpoint_ids() const = 0;
    virtual bool begin_capture(std::string_view endpoint_id, int sample_rate, int channels) = 0;
    virtual void stop_capture() noexcept = 0;
};

[[nodiscard]] std::unique_ptr<IDeviceDiscovery> make_system_device_discovery();
[[nodiscard]] std::string to_string(BackendMaturity maturity);

inline constexpr std::uint16_t mpc_sample_vendor_id = 0x09E8;
inline constexpr std::uint16_t mpc_sample_product_id = 0x205C;

} // namespace ubridge::platform
