#include "ubridge/platform/local_service.hpp"

#include <utility>

namespace ubridge::platform {

std::string to_string(ServiceState state) {
    switch (state) {
        case ServiceState::stopped: return "stopped";
        case ServiceState::initializing: return "initializing";
        case ServiceState::ready_without_hardware: return "ready_without_hardware";
        case ServiceState::ready_with_hardware: return "ready_with_hardware";
        case ServiceState::unavailable: return "unavailable";
        case ServiceState::faulted: return "faulted";
    }
    return "faulted";
}

LocalBridgeService::LocalBridgeService(core::PlatformCapability platform)
    : platform_(std::move(platform)) {
    status_.platform_id = platform_.platform_id;
}

ServiceStatus LocalBridgeService::start() {
    status_.diagnostics.clear();
    status_.state = ServiceState::initializing;

    if (!platform_.local_file_access) {
        status_.state = ServiceState::unavailable;
        status_.diagnostics.push_back({
            core::DiagnosticSeverity::warning,
            "service_platform_not_qualified",
            "The selected platform has no qualified local bridge-service route in this prototype.",
            "Use the capability record for planning or develop a platform-specific service backend before enabling hardware access."
        });
        return status_;
    }

    status_.state = ServiceState::ready_without_hardware;
    status_.diagnostics.push_back({
        core::DiagnosticSeverity::info,
        "service_safe_mode",
        "The local bridge service scaffold is ready in safe mode without device access.",
        "Implement and qualify the platform USB/audio/MIDI backend before enabling physical hardware endpoints."
    });
    return status_;
}

ServiceStatus LocalBridgeService::stop() {
    status_.state = ServiceState::stopped;
    status_.diagnostics.push_back({
        core::DiagnosticSeverity::info,
        "service_stopped",
        "The local bridge service has stopped.",
        "No background device or audio operation remains active."
    });
    return status_;
}

const ServiceStatus& LocalBridgeService::status() const noexcept {
    return status_;
}

std::optional<core::Transaction> LocalBridgeService::begin_transaction(
    const core::IntegrationPlan& plan,
    std::string transaction_id,
    std::vector<std::string> backup_locations) {
    if (status_.state != ServiceState::ready_without_hardware && status_.state != ServiceState::ready_with_hardware) {
        return std::nullopt;
    }
    if (!plan.safe_preflight) {
        status_.diagnostics.push_back({
            core::DiagnosticSeverity::error,
            "transaction_preflight_required",
            "A bridge transaction cannot start without a safe preflight route.",
            "Resolve the project/storage capability route before trying again."
        });
        return std::nullopt;
    }

    auto transaction = journal_.begin(
        std::move(transaction_id),
        "Universal Bridge workflow transaction",
        {0, 0, 0},
        std::move(backup_locations));
    const bool advanced = journal_.transition(
        transaction.id,
        core::TransactionPhase::awaiting_approval,
        core::Diagnostic{
            core::DiagnosticSeverity::info,
            "transaction_awaiting_approval",
            "The transaction has been prepared and requires user approval before execution.",
            "Review the integration plan, output paths, limitations, and backup locations."
        });
    if (!advanced) {
        return std::nullopt;
    }
    return journal_.find(transaction.id);
}

bool LocalBridgeService::supports_hardware_backend() const noexcept {
    return platform_.runtime_qualified && platform_.usb_device_access && platform_.audio_backend && platform_.virtual_midi;
}

} // namespace ubridge::platform
