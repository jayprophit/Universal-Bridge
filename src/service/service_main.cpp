#include "ubridge/platform/hardware_backends.hpp"
#include "ubridge/platform/local_ipc.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

#include <filesystem>
#include <iostream>
#include <string_view>

namespace {
class PerUserSingleton {
  public:
    PerUserSingleton() {
#ifdef _WIN32
        handle_ = CreateMutexW(nullptr, FALSE, L"Local\\UniversalBridge.UserService.v1");
        owns_ = handle_ != nullptr && GetLastError() != ERROR_ALREADY_EXISTS;
#else
        owns_ = true;
#endif
    }
    ~PerUserSingleton() {
#ifdef _WIN32
        if (handle_) CloseHandle(handle_);
#endif
    }
    [[nodiscard]] bool owns() const noexcept { return owns_; }

  private:
#ifdef _WIN32
    HANDLE handle_ = nullptr;
#endif
    bool owns_ = false;
};

int diagnose() {
    auto discovery = ubridge::platform::make_system_device_discovery();
    auto midi = ubridge::platform::make_system_midi_backend();
    auto audio = ubridge::platform::make_system_audio_backend();
    const auto devices = discovery->enumerate();
    const auto midi_endpoints = midi->enumerate_endpoints();
    const auto audio_endpoints = audio->enumerate_endpoints();
    std::cout << "USB containers: " << devices.size() << "\n"
              << "MIDI endpoints: " << midi_endpoints.size() << "\n"
              << "Audio endpoints: " << audio_endpoints.size() << "\n"
              << "MIDI backend: " << ubridge::platform::to_string(midi->maturity()) << "\n"
              << "Audio backend: " << ubridge::platform::to_string(audio->maturity()) << "\n"
              << "Universal Bridge per-user service diagnostic complete. No endpoint was opened.\n";
    return 0;
}

void print_response(const ubridge::platform::LocalIpcResponse& response) {
    std::cout << "connected=" << (response.connected ? "true" : "false")
              << " authenticated=" << (response.authenticated ? "true" : "false")
              << " accepted=" << (response.accepted ? "true" : "false")
              << " protocol=" << response.selected_protocol_version << " requests=" << response.request_count
              << " unclean_restarts=" << response.unclean_restart_count << " status=" << response.status;
    if (!response.error.empty()) std::cout << " error=" << response.error;
    std::cout << '\n';
}
} // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string_view(argv[1]) == "--diagnose-once") return diagnose();
    if (argc == 3 && std::string_view(argv[1]) == "--ipc-self-test") {
        std::string report;
        const bool passed = ubridge::platform::run_local_ipc_self_test(std::filesystem::path(argv[2]), report);
        std::cout << report << '\n';
        return passed ? 0 : 5;
    }
    if (argc == 2 && (std::string_view(argv[1]) == "--ping" || std::string_view(argv[1]) == "--status" ||
                      std::string_view(argv[1]) == "--stop")) {
        const auto command = std::string_view(argv[1]) == "--stop" ? "shutdown" : std::string(argv[1] + 2);
        const auto response = ubridge::platform::send_local_ipc_command({}, command);
        print_response(response);
        return response.accepted ? 0 : 6;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--run") {
        PerUserSingleton singleton;
        if (!singleton.owns()) {
            std::cerr << "Another Universal Bridge per-user service instance is already running.\n";
            return 2;
        }
        std::cout
            << "Starting authenticated per-user IPC service. No MIDI or audio endpoint is opened automatically.\n";
        return ubridge::platform::run_local_ipc_server({});
    }
    std::cout << "Usage: ubridge_service --diagnose-once | --run | --ping | --status | --stop\n";
    return 1;
}
