#pragma once

#include "ubridge/core/bridge_core.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ubridge::modules {

struct DeviceProfile {
    std::string id;
    std::string manufacturer;
    std::string display_name;
    std::string integration_tier;
    core::DeviceCapability capability;
    bool parser_implemented = false;
    bool control_qualified = false;
    std::vector<std::string> safe_fallbacks;
};

struct DawProfile {
    std::string id;
    std::string display_name;
    core::DawCapability capability;
    std::vector<std::string> safe_fallbacks;
};

struct PlatformProfile {
    std::string id;
    std::string display_name;
    std::string state;
    std::string host_mode;
    core::PlatformCapability capability;
};

struct AudioCapturePlan {
    bool allowed = false;
    bool sequential = false;
    int expected_input_channels = 0;
    std::string latency_policy;
    std::vector<std::string> required_checks;
    std::vector<std::string> limitations;
};

struct MidiRoutePlan {
    bool allowed = false;
    bool virtual_ports_required = false;
    bool supports_thru_merge_split = false;
    std::vector<std::string> required_checks;
    std::vector<std::string> limitations;
};

struct WorkflowStep {
    std::string id;
    std::string title;
    bool requires_user_approval = false;
    bool enabled = false;
    std::string rationale;
};

struct FinishWorkflow {
    std::string id;
    core::IntegrationPlan integration;
    std::vector<WorkflowStep> steps;
};

struct VirtualDeviceEvent {
    std::string type;
    std::string payload;
    std::int64_t timestamp_ticks = 0;
};

class VirtualDevice {
public:
    VirtualDevice(std::string id, core::DeviceCapability capability);

    [[nodiscard]] const std::string& id() const noexcept;
    [[nodiscard]] const core::DeviceCapability& capability() const noexcept;
    void enqueue(VirtualDeviceEvent event);
    [[nodiscard]] std::optional<VirtualDeviceEvent> next_event();
    [[nodiscard]] bool disconnected() const noexcept;
    void disconnect();
    void reconnect();

private:
    std::string id_;
    core::DeviceCapability capability_;
    bool disconnected_ = false;
    std::vector<VirtualDeviceEvent> events_;
};

[[nodiscard]] std::vector<DeviceProfile> builtin_device_profiles();
[[nodiscard]] std::vector<DawProfile> builtin_daw_profiles();
[[nodiscard]] std::vector<PlatformProfile> builtin_platform_profiles();
[[nodiscard]] std::optional<DeviceProfile> find_device_profile(std::string_view id);
[[nodiscard]] std::optional<DawProfile> find_daw_profile(std::string_view id);
[[nodiscard]] std::optional<PlatformProfile> find_platform_profile(std::string_view id);

[[nodiscard]] AudioCapturePlan plan_audio_capture(
    const DeviceProfile& device,
    const PlatformProfile& platform,
    const core::ConnectionCapability& connection);

[[nodiscard]] MidiRoutePlan plan_midi_route(
    const DeviceProfile& device,
    const PlatformProfile& platform,
    const core::ConnectionCapability& connection);

[[nodiscard]] FinishWorkflow plan_finish_in_daw(
    const DeviceProfile& device,
    const PlatformProfile& platform,
    const DawProfile& daw,
    const core::ConnectionCapability& connection);

} // namespace ubridge::modules
