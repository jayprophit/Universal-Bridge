#include "ubridge/platform/hardware_backends.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

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
} // namespace

int main(int argc, char* argv[]) {
    PerUserSingleton singleton;
    if (!singleton.owns()) {
        std::cerr << "Another Universal Bridge per-user service instance is already running.\n";
        return 2;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--diagnose-once") return diagnose();
    if (argc != 2 || std::string_view(argv[1]) != "--run") {
        std::cout << "Usage: ubridge_service --diagnose-once | --run\n";
        return 1;
    }
    diagnose();
    std::cout << "Service running in diagnostic ownership mode. IPC and persistent hardware sessions are not yet enabled.\n";
    while (true) std::this_thread::sleep_for(std::chrono::seconds(1));
}
