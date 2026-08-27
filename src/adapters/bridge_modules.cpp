#include "ubridge/bridge_modules.hpp"

#include <algorithm>
#include <utility>

namespace ubridge::modules {
namespace {

core::DeviceCapability device_capability(
    std::string profile_id,
    bool storage_access,
    bool midi_input,
    bool midi_output,
    bool audio_input,
    int audio_channels,
    bool parameter_read,
    bool parameter_write,
    bool transport,
    bool automation_read,
    bool automation_write) {
    core::DeviceCapability capability;
    capability.profile_id = std::move(profile_id);
    capability.project_read = storage_access;
    capability.project_write = false;
    capability.storage_access = storage_access;
    capability.midi_input = midi_input;
    capability.midi_output = midi_output;
    capability.audio_input = audio_input;
    capability.audio_channels = audio_channels;
    capability.parameter_read = parameter_read;
    capability.parameter_write = parameter_write;
    capability.transport = transport;
    capability.automation_read = automation_read;
    capability.automation_write = automation_write;
    return capability;
}

core::DawCapability daw_capability(std::string adapter_id, bool plugin_client) {
    core::DawCapability capability;
    capability.adapter_id = std::move(adapter_id);
    capability.audio_import = true;
    capability.midi_import = true;
    capability.plugin_client = plugin_client;
    capability.direct_project_create = false;
    capability.track_create = false;
    capability.parameter_feedback = false;
    capability.transport_control = false;
    return capability;
}

core::PlatformCapability platform_capability(
    std::string platform_id,
    bool runtime_qualified,
    bool local_file_access,
    bool usb_device_access,
    bool audio_backend,
    bool virtual_midi,
    bool desktop_plugin_host_route,
    bool companion_route,
    bool direct_mobile_host_route) {
    core::PlatformCapability capability;
    capability.platform_id = std::move(platform_id);
    capability.runtime_qualified = runtime_qualified;
    capability.local_file_access = local_file_access;
    capability.usb_device_access = usb_device_access;
    capability.audio_backend = audio_backend;
    capability.virtual_midi = virtual_midi;
    capability.desktop_plugin_host_route = desktop_plugin_host_route;
    capability.companion_route = companion_route;
    capability.direct_mobile_host_route = direct_mobile_host_route;
    return capability;
}

template <typename Profile>
std::optional<Profile> find_profile(const std::vector<Profile>& profiles, std::string_view id) {
    const auto found = std::find_if(profiles.begin(), profiles.end(), [id](const Profile& profile) {
        return profile.id == id;
    });
    if (found == profiles.end()) {
        return std::nullopt;
    }
    return *found;
}

} // namespace

std::vector<DeviceProfile> builtin_device_profiles() {
    return {
        {
            "akai.mpc-sample", "Akai", "MPC Sample", "B",
            device_capability("akai.mpc-sample", true, true, true, true, 2, false, false, true, false, false),
            false, false,
            {"read_only_project_intake", "audio_midi_exchange", "external_backup", "assisted_capture"}
        },
        {
            "akai.mpc-modern", "Akai", "Modern MPC family", "A",
            device_capability("akai.mpc-modern", true, true, true, true, 8, false, false, true, false, false),
            false, false,
            {"read_only_project_intake", "audio_midi_exchange", "capability_report"}
        },
        {
            "roland.sp-404mkii", "Roland", "SP-404MKII", "B",
            device_capability("roland.sp-404mkii", true, true, true, true, 2, false, false, true, false, false),
            false, false,
            {"read_only_project_intake", "audio_midi_exchange", "assisted_capture"}
        },
        {
            "elektron.digitakt", "Elektron", "Digitakt", "A",
            device_capability("elektron.digitakt", true, true, true, true, 8, false, false, true, false, false),
            false, false,
            {"read_only_project_intake", "audio_midi_exchange", "capability_report"}
        },
        {
            "native-instruments.maschine", "Native Instruments", "Maschine", "A",
            device_capability("native-instruments.maschine", true, true, true, true, 8, false, false, true, false, false),
            false, false,
            {"read_only_project_intake", "audio_midi_exchange", "capability_report"}
        },
        {
            "novation.circuit", "Novation", "Circuit / Circuit Tracks", "C",
            device_capability("novation.circuit", false, true, true, false, 0, false, false, true, false, false),
            false, false,
            {"midi_exchange", "analogue_audio_capture_plan", "capability_report"}
        },
        {
            "akai.mpc-legacy", "Akai", "Legacy MPC", "D",
            device_capability("akai.mpc-legacy", true, true, true, true, 2, false, false, true, false, false),
            false, false,
            {"storage_import", "midi_exchange", "analogue_audio_capture_plan"}
        }
    };
}

std::vector<DawProfile> builtin_daw_profiles() {
    return {
        {"cubase", "Cubase", daw_capability("cubase", true), {"audio_midi_exchange", "vst3_client_future"}},
        {"reason", "Reason", daw_capability("reason", true), {"audio_midi_exchange", "vst3_client_future", "rack_extension_feasibility"}},
        {"ableton-live", "Ableton Live", daw_capability("ableton-live", true), {"audio_midi_exchange", "vst3_client_future"}},
        {"logic-pro", "Logic Pro", daw_capability("logic-pro", false), {"audio_midi_exchange", "au_client_future"}},
        {"fl-studio", "FL Studio", daw_capability("fl-studio", true), {"audio_midi_exchange", "vst3_client_future"}},
        {"reaper", "Reaper", daw_capability("reaper", true), {"audio_midi_exchange", "vst3_client_future"}},
        {"studio-one", "Studio One", daw_capability("studio-one", true), {"audio_midi_exchange", "vst3_client_future"}},
        {"pro-tools", "Pro Tools", daw_capability("pro-tools", false), {"audio_midi_exchange", "aax_feasibility"}},
        {"mobile-generic", "Mobile / Tablet DAW", daw_capability("mobile-generic", false), {"audio_midi_exchange", "virtual_midi_or_file_exchange"}}
    };
}

std::vector<PlatformProfile> builtin_platform_profiles() {
    return {
        {"windows", "Microsoft Windows", "reference_desktop", "desktop_bridge", platform_capability("windows", true, true, false, false, false, true, false, false)},
        {"macos", "macOS", "portable_core_target", "desktop_bridge", platform_capability("macos", false, true, false, false, false, true, false, false)},
        {"linux", "Linux", "portable_core_target", "desktop_bridge", platform_capability("linux", false, true, false, false, false, true, false, false)},
        {"android", "Android", "future_mobile_host", "mobile_bridge_or_companion", platform_capability("android", false, false, false, false, false, false, true, false)},
        {"chromeos", "ChromeOS", "future_mobile_host", "mobile_bridge_or_companion", platform_capability("chromeos", false, false, false, false, false, false, true, false)},
        {"ipados", "iPadOS", "future_mobile_host", "mobile_bridge_or_companion", platform_capability("ipados", false, false, false, false, false, false, true, false)},
        {"ios", "iOS", "future_mobile_host", "mobile_bridge_or_companion", platform_capability("ios", false, false, false, false, false, false, true, false)}
    };
}

std::optional<DeviceProfile> find_device_profile(std::string_view id) {
    return find_profile(builtin_device_profiles(), id);
}

std::optional<DawProfile> find_daw_profile(std::string_view id) {
    return find_profile(builtin_daw_profiles(), id);
}

std::optional<PlatformProfile> find_platform_profile(std::string_view id) {
    return find_profile(builtin_platform_profiles(), id);
}

AudioCapturePlan plan_audio_capture(
    const DeviceProfile& device,
    const PlatformProfile& platform,
    const core::ConnectionCapability& connection) {
    AudioCapturePlan plan;
    plan.expected_input_channels = device.capability.audio_channels;
    plan.latency_policy = "calibrate_before_capture";
    plan.required_checks = {"user_approval", "source_backup", "timing_calibration", "silence_detection", "tail_policy", "clip_detection"};

    const bool transport_available = connection.usb_audio || connection.analogue_audio;
    plan.allowed = platform.capability.runtime_qualified && platform.capability.audio_backend &&
                   device.capability.audio_input && transport_available;
    plan.sequential = plan.allowed && device.capability.audio_channels <= 2 && device.control_qualified;

    if (!plan.allowed) {
        plan.limitations.push_back("Audio capture is disabled until the exact device, platform backend, and connection route are qualified.");
    }
    if (device.capability.audio_channels == 0) {
        plan.limitations.push_back("The profile declares no direct device audio route; use a qualified analogue capture plan if available.");
    } else if (device.capability.audio_channels <= 2) {
        plan.limitations.push_back("The profile is stereo-capable; simultaneous multichannel stem capture must not be claimed.");
    }
    if (!device.control_qualified) {
        plan.limitations.push_back("Automated per-part capture is disabled until the control profile is tested for safe solo/mute behavior.");
    }
    return plan;
}

MidiRoutePlan plan_midi_route(
    const DeviceProfile& device,
    const PlatformProfile& platform,
    const core::ConnectionCapability& connection) {
    MidiRoutePlan plan;
    plan.virtual_ports_required = true;
    plan.required_checks = {"endpoint_identity", "feedback_loop_detection", "channel_collision_detection", "user_approval"};
    plan.allowed = platform.capability.runtime_qualified && platform.capability.usb_device_access &&
                   platform.capability.virtual_midi && connection.usb_midi &&
                   device.capability.midi_input && device.capability.midi_output;
    plan.supports_thru_merge_split = plan.allowed;

    if (!plan.allowed) {
        plan.limitations.push_back("Live MIDI routing is disabled until the platform device service and virtual MIDI backend are qualified.");
    }
    return plan;
}

FinishWorkflow plan_finish_in_daw(
    const DeviceProfile& device,
    const PlatformProfile& platform,
    const DawProfile& daw,
    const core::ConnectionCapability& connection) {
    FinishWorkflow workflow;
    workflow.id = "finish-in-" + daw.id;
    workflow.integration = core::negotiate(device.capability, platform.capability, daw.capability, connection);

    const bool intake = workflow.integration.safe_preflight;
    const bool exchange = workflow.integration.asset_exchange || workflow.integration.midi_exchange;
    workflow.steps = {
        {"detect", "Resolve device, platform, DAW, and connection profiles", false, true, "Always required before any operation."},
        {"preflight", "Inspect project and report effective capabilities", false, intake, "Read-only project intake is required."},
        {"backup", "Create an external backup and transaction snapshot", false, intake, "Source projects remain protected."},
        {"approval", "Show planned output, limitations, and approvals", true, intake, "Required before future destructive actions."},
        {"parse", "Parse project into the canonical session graph", false, false, "Disabled until format-specific parser qualification."},
        {"assets", "Collect audio, MIDI, and project dependencies", false, exchange, "Exchange bundle is the safe current hand-off."},
        {"capture", "Capture/rerender required audio and validate takes", true, workflow.integration.audio_capture, "Enabled only after timing/capture qualification."},
        {"reconstruct", "Create DAW tracks, arrangement, mixer, and automation", true, workflow.integration.direct_daw_creation, "Requires verified host API or supported import route."},
        {"sync", "Start live state synchronization", true, workflow.integration.bidirectional_sync, "Requires explicit authority/conflict policies."},
        {"verify", "Verify output, save result, and preserve recovery record", false, intake, "Always required for a completed workflow."}
    };
    return workflow;
}

VirtualDevice::VirtualDevice(std::string id, core::DeviceCapability capability)
    : id_(std::move(id)), capability_(std::move(capability)) {}

const std::string& VirtualDevice::id() const noexcept {
    return id_;
}

const core::DeviceCapability& VirtualDevice::capability() const noexcept {
    return capability_;
}

void VirtualDevice::enqueue(VirtualDeviceEvent event) {
    events_.push_back(std::move(event));
}

std::optional<VirtualDeviceEvent> VirtualDevice::next_event() {
    if (disconnected_ || events_.empty()) {
        return std::nullopt;
    }
    auto event = events_.front();
    events_.erase(events_.begin());
    return event;
}

bool VirtualDevice::disconnected() const noexcept {
    return disconnected_;
}

void VirtualDevice::disconnect() {
    disconnected_ = true;
}

void VirtualDevice::reconnect() {
    disconnected_ = false;
}

} // namespace ubridge::modules
