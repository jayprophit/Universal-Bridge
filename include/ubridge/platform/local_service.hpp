#pragma once

#include "ubridge/core/bridge_core.hpp"

#include <optional>
#include <string>
#include <vector>

namespace ubridge::platform {

enum class ServiceState {
    stopped,
    initializing,
    ready_without_hardware,
    ready_with_hardware,
    unavailable,
    faulted,
};

struct ServiceStatus {
    ServiceState state = ServiceState::stopped;
    std::string platform_id;
    std::vector<core::Diagnostic> diagnostics;
};

class LocalBridgeService {
public:
    explicit LocalBridgeService(core::PlatformCapability platform);

    [[nodiscard]] ServiceStatus start();
    [[nodiscard]] ServiceStatus stop();
    [[nodiscard]] const ServiceStatus& status() const noexcept;

    [[nodiscard]] std::optional<core::Transaction> begin_transaction(
        const core::IntegrationPlan& plan,
        std::string transaction_id,
        std::vector<std::string> backup_locations);

    [[nodiscard]] bool supports_hardware_backend() const noexcept;

private:
    core::PlatformCapability platform_;
    ServiceStatus status_;
    core::TransactionJournal journal_;
};

[[nodiscard]] std::string to_string(ServiceState state);

} // namespace ubridge::platform
