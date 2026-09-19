#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace ubridge::platform {

inline constexpr std::uint32_t local_ipc_protocol_version = 1;
inline constexpr std::uint32_t local_ipc_maximum_frame_bytes = 64U * 1024U;

struct LocalIpcServerOptions {
    std::filesystem::path state_directory;
    std::string channel = "service";
    std::uint32_t maximum_requests = 0; // zero keeps accepting clients until an authenticated shutdown
};

struct LocalIpcClientOptions {
    std::filesystem::path state_directory;
    std::string channel = "service";
    std::uint32_t connect_timeout_ms = 5'000;
};

struct LocalIpcResponse {
    bool connected = false;
    bool authenticated = false;
    bool accepted = false;
    std::uint32_t selected_protocol_version = 0;
    std::uint64_t request_count = 0;
    std::uint64_t unclean_restart_count = 0;
    std::string instance_id;
    std::string status;
    std::string error;
};

[[nodiscard]] std::filesystem::path default_service_state_directory();
[[nodiscard]] bool local_ipc_supported() noexcept;
[[nodiscard]] int run_local_ipc_server(const LocalIpcServerOptions& options);
[[nodiscard]] LocalIpcResponse
send_local_ipc_command(const LocalIpcClientOptions& options, std::string command,
                       std::uint32_t minimum_protocol_version = local_ipc_protocol_version,
                       std::uint32_t maximum_protocol_version = local_ipc_protocol_version);
[[nodiscard]] bool run_local_ipc_self_test(const std::filesystem::path& test_root, std::string& report);

} // namespace ubridge::platform
