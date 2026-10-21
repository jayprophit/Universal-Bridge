#include "ubridge/platform/local_ipc.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <aclapi.h>
#include <bcrypt.h>
#include <dpapi.h>
#include <sddl.h>
#include <windows.h>
#endif

namespace ubridge::platform {
namespace {

#ifdef _WIN32

struct PersistentServiceState {
    std::uint32_t schema_version = 1;
    std::string instance_id;
    std::uint64_t server_start_count = 0;
    std::uint64_t request_count = 0;
    std::uint64_t unclean_restart_count = 0;
    bool clean_shutdown = true;
    std::string last_command;
    std::uint64_t updated_at_microseconds = 0;
};

std::uint64_t unix_microseconds() noexcept {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

std::string make_instance_id() {
    std::ostringstream output;
    output << std::hex << unix_microseconds();
#ifdef _WIN32
    output << '-' << GetCurrentProcessId();
#endif
    return output.str();
}

nlohmann::json serialize_state(const PersistentServiceState& state) {
    return {
        {"schema_version", state.schema_version},
        {"instance_id", state.instance_id},
        {"server_start_count", state.server_start_count},
        {"request_count", state.request_count},
        {"unclean_restart_count", state.unclean_restart_count},
        {"clean_shutdown", state.clean_shutdown},
        {"last_command", state.last_command},
        {"updated_at_microseconds", state.updated_at_microseconds},
    };
}

PersistentServiceState parse_state(const nlohmann::json& value) {
    PersistentServiceState state;
    state.schema_version = value.value("schema_version", 0U);
    if (state.schema_version != 1) throw std::runtime_error("unsupported service-state schema");
    state.instance_id = value.value("instance_id", std::string{});
    state.server_start_count = value.value("server_start_count", 0ULL);
    state.request_count = value.value("request_count", 0ULL);
    state.unclean_restart_count = value.value("unclean_restart_count", 0ULL);
    state.clean_shutdown = value.value("clean_shutdown", false);
    state.last_command = value.value("last_command", std::string{});
    state.updated_at_microseconds = value.value("updated_at_microseconds", 0ULL);
    return state;
}

PersistentServiceState load_state(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return {};
    if (std::filesystem::file_size(path) > local_ipc_maximum_frame_bytes) {
        throw std::runtime_error("service-state file exceeds the bounded size");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("service-state file could not be opened");
    return parse_state(nlohmann::json::parse(input));
}

class LocalHandle {
  public:
    explicit LocalHandle(HLOCAL handle = nullptr) noexcept : handle_(handle) {}
    ~LocalHandle() {
        if (handle_) LocalFree(handle_);
    }
    LocalHandle(const LocalHandle&) = delete;
    LocalHandle& operator=(const LocalHandle&) = delete;
    [[nodiscard]] HLOCAL get() const noexcept { return handle_; }

  private:
    HLOCAL handle_ = nullptr;
};

class WinHandle {
  public:
    explicit WinHandle(HANDLE handle = INVALID_HANDLE_VALUE) noexcept : handle_(handle) {}
    ~WinHandle() { reset(); }
    WinHandle(const WinHandle&) = delete;
    WinHandle& operator=(const WinHandle&) = delete;
    WinHandle(WinHandle&& other) noexcept : handle_(other.handle_) { other.handle_ = INVALID_HANDLE_VALUE; }
    WinHandle& operator=(WinHandle&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = other.handle_;
            other.handle_ = INVALID_HANDLE_VALUE;
        }
        return *this;
    }
    [[nodiscard]] HANDLE get() const noexcept { return handle_; }
    [[nodiscard]] bool valid() const noexcept { return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE; }
    void reset(HANDLE replacement = INVALID_HANDLE_VALUE) noexcept {
        if (valid()) CloseHandle(handle_);
        handle_ = replacement;
    }

  private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

std::string windows_error(DWORD code = GetLastError()) {
    LPWSTR message = nullptr;
    const DWORD count =
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       nullptr, code, 0, reinterpret_cast<LPWSTR>(&message), 0, nullptr);
    std::string result = "Windows error " + std::to_string(code);
    if (count && message) {
        const int bytes =
            WideCharToMultiByte(CP_UTF8, 0, message, static_cast<int>(count), nullptr, 0, nullptr, nullptr);
        if (bytes > 0) {
            result.assign(static_cast<std::size_t>(bytes), '\0');
            WideCharToMultiByte(CP_UTF8, 0, message, static_cast<int>(count), result.data(), bytes, nullptr, nullptr);
            while (!result.empty() && (result.back() == '\r' || result.back() == '\n' || result.back() == ' '))
                result.pop_back();
        }
        LocalFree(message);
    }
    return result;
}

std::wstring current_user_sid() {
    WinHandle token;
    HANDLE raw = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw)) throw std::runtime_error(windows_error());
    token.reset(raw);
    DWORD bytes = 0;
    GetTokenInformation(token.get(), TokenUser, nullptr, 0, &bytes);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || bytes == 0) throw std::runtime_error(windows_error());
    std::vector<std::byte> storage(bytes);
    if (!GetTokenInformation(token.get(), TokenUser, storage.data(), bytes, &bytes))
        throw std::runtime_error(windows_error());
    const auto* user = reinterpret_cast<const TOKEN_USER*>(storage.data());
    LPWSTR sid_text = nullptr;
    if (!ConvertSidToStringSidW(user->User.Sid, &sid_text)) throw std::runtime_error(windows_error());
    LocalHandle owner(sid_text);
    return static_cast<const wchar_t*>(owner.get());
}

std::wstring channel_name(std::string channel) {
    for (auto& character : channel) {
        const bool safe = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
                          (character >= '0' && character <= '9') || character == '-' || character == '_';
        if (!safe) character = '-';
    }
    std::wstring wide(channel.begin(), channel.end());
    return L"\\\\.\\pipe\\UniversalBridge.v1." + current_user_sid() + L"." + wide;
}

bool write_all(HANDLE handle, const void* data, std::size_t bytes) {
    const auto* cursor = static_cast<const std::byte*>(data);
    while (bytes > 0) {
        const auto chunk =
            static_cast<DWORD>((std::min)(bytes, static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
        DWORD written = 0;
        if (!WriteFile(handle, cursor, chunk, &written, nullptr) || written == 0) return false;
        cursor += written;
        bytes -= written;
    }
    return true;
}

bool read_all(HANDLE handle, void* data, std::size_t bytes) {
    auto* cursor = static_cast<std::byte*>(data);
    while (bytes > 0) {
        const auto chunk =
            static_cast<DWORD>((std::min)(bytes, static_cast<std::size_t>((std::numeric_limits<DWORD>::max)())));
        DWORD read = 0;
        if (!ReadFile(handle, cursor, chunk, &read, nullptr) || read == 0) return false;
        cursor += read;
        bytes -= read;
    }
    return true;
}

bool write_frame(HANDLE handle, const nlohmann::json& value) {
    const auto payload = value.dump();
    if (payload.empty() || payload.size() > local_ipc_maximum_frame_bytes) return false;
    const auto size = static_cast<std::uint32_t>(payload.size());
    const std::array<std::uint8_t, 4> header{
        static_cast<std::uint8_t>(size & 0xFFU),
        static_cast<std::uint8_t>((size >> 8U) & 0xFFU),
        static_cast<std::uint8_t>((size >> 16U) & 0xFFU),
        static_cast<std::uint8_t>((size >> 24U) & 0xFFU),
    };
    return write_all(handle, header.data(), header.size()) && write_all(handle, payload.data(), payload.size());
}

bool read_frame(HANDLE handle, nlohmann::json& value) {
    std::array<std::uint8_t, 4> header{};
    if (!read_all(handle, header.data(), header.size())) return false;
    const auto size = static_cast<std::uint32_t>(header[0]) | (static_cast<std::uint32_t>(header[1]) << 8U) |
                      (static_cast<std::uint32_t>(header[2]) << 16U) | (static_cast<std::uint32_t>(header[3]) << 24U);
    if (size == 0 || size > local_ipc_maximum_frame_bytes) return false;
    std::string payload(size, '\0');
    if (!read_all(handle, payload.data(), payload.size())) return false;
    value = nlohmann::json::parse(payload, nullptr, false);
    return !value.is_discarded() && value.is_object();
}

void atomic_write_bytes(const std::filesystem::path& destination, const std::byte* data, std::size_t size) {
    std::filesystem::create_directories(destination.parent_path());
    const auto temporary = destination.wstring() + L".tmp";
    WinHandle file(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr));
    if (!file.valid()) throw std::runtime_error(windows_error());
    if (!write_all(file.get(), data, size) || !FlushFileBuffers(file.get())) throw std::runtime_error(windows_error());
    file.reset();
    if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary.c_str());
        throw std::runtime_error(windows_error());
    }
}

void save_state(const std::filesystem::path& path, PersistentServiceState& state) {
    state.updated_at_microseconds = unix_microseconds();
    const auto payload = serialize_state(state).dump(2) + "\n";
    atomic_write_bytes(path, reinterpret_cast<const std::byte*>(payload.data()), payload.size());
}

std::vector<std::byte> load_or_create_credential(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) {
        const auto bytes = std::filesystem::file_size(path);
        if (bytes == 0 || bytes > 4096) throw std::runtime_error("credential file has an invalid size");
        std::vector<std::byte> protected_bytes(static_cast<std::size_t>(bytes));
        std::ifstream input(path, std::ios::binary);
        input.read(reinterpret_cast<char*>(protected_bytes.data()),
                   static_cast<std::streamsize>(protected_bytes.size()));
        if (!input) throw std::runtime_error("credential file could not be read");
        DATA_BLOB encrypted{static_cast<DWORD>(protected_bytes.size()),
                            reinterpret_cast<BYTE*>(protected_bytes.data())};
        DATA_BLOB plaintext{};
        if (!CryptUnprotectData(&encrypted, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN,
                                &plaintext)) {
            throw std::runtime_error(windows_error());
        }
        LocalHandle owner(plaintext.pbData);
        if (plaintext.cbData != 32) throw std::runtime_error("credential plaintext has an invalid size");
        const auto* begin = reinterpret_cast<const std::byte*>(plaintext.pbData);
        return {begin, begin + plaintext.cbData};
    }

    std::vector<std::byte> credential(32);
    if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(credential.data()), static_cast<ULONG>(credential.size()),
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
        throw std::runtime_error("BCryptGenRandom failed");
    }
    DATA_BLOB plaintext{static_cast<DWORD>(credential.size()), reinterpret_cast<BYTE*>(credential.data())};
    DATA_BLOB encrypted{};
    if (!CryptProtectData(&plaintext, L"Universal Bridge local IPC credential", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_UI_FORBIDDEN, &encrypted)) {
        throw std::runtime_error(windows_error());
    }
    LocalHandle owner(encrypted.pbData);
    atomic_write_bytes(path, reinterpret_cast<const std::byte*>(encrypted.pbData), encrypted.cbData);
    return credential;
}

std::string hex(const std::vector<std::byte>& value) {
    static constexpr char alphabet[] = "0123456789abcdef";
    std::string result(value.size() * 2, '0');
    for (std::size_t index = 0; index < value.size(); ++index) {
        const auto byte = std::to_integer<unsigned int>(value[index]);
        result[index * 2] = alphabet[(byte >> 4U) & 0xFU];
        result[index * 2 + 1] = alphabet[byte & 0xFU];
    }
    return result;
}

bool constant_time_equal(std::string_view left, std::string_view right) noexcept {
    std::size_t difference = left.size() ^ right.size();
    const auto count = (std::max)(left.size(), right.size());
    for (std::size_t index = 0; index < count; ++index) {
        const auto lhs = index < left.size() ? static_cast<unsigned char>(left[index]) : 0U;
        const auto rhs = index < right.size() ? static_cast<unsigned char>(right[index]) : 0U;
        difference |= lhs ^ rhs;
    }
    return difference == 0;
}

LocalHandle make_pipe_security_descriptor() {
    const auto sddl = L"D:P(A;;GA;;;" + current_user_sid() + L")";
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr)) {
        throw std::runtime_error(windows_error());
    }
    return LocalHandle(descriptor);
}

LocalIpcResponse parse_response(const nlohmann::json& value) {
    LocalIpcResponse response;
    response.connected = true;
    response.authenticated = value.value("authenticated", false);
    response.accepted = value.value("accepted", false);
    response.selected_protocol_version = value.value("selected_protocol_version", 0U);
    response.request_count = value.value("request_count", 0ULL);
    response.unclean_restart_count = value.value("unclean_restart_count", 0ULL);
    response.instance_id = value.value("instance_id", std::string{});
    response.status = value.value("status", std::string{});
    response.error = value.value("error", std::string{});
    return response;
}

#endif

} // namespace

std::filesystem::path default_service_state_directory() {
#ifdef _WIN32
    std::array<wchar_t, 32'768> local_app_data{};
    const auto characters =
        GetEnvironmentVariableW(L"LOCALAPPDATA", local_app_data.data(), static_cast<DWORD>(local_app_data.size()));
    if (characters > 0 && characters < local_app_data.size()) {
        return std::filesystem::path(local_app_data.data()) / "UniversalBridge" / "service-v1";
    }
#endif
    return std::filesystem::temp_directory_path() / "UniversalBridge" / "service-v1-unavailable";
}

bool local_ipc_supported() noexcept {
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

int run_local_ipc_server(const LocalIpcServerOptions& supplied_options) {
#ifdef _WIN32
    try {
        auto options = supplied_options;
        if (options.state_directory.empty()) options.state_directory = default_service_state_directory();
        std::filesystem::create_directories(options.state_directory);
        const auto descriptor = make_pipe_security_descriptor();
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), descriptor.get(), FALSE};
        const auto pipe_name = channel_name(options.channel);
        WinHandle pipe(CreateNamedPipeW(pipe_name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1,
                                        local_ipc_maximum_frame_bytes + 4U, local_ipc_maximum_frame_bytes + 4U, 0,
                                        &security));
        if (!pipe.valid()) throw std::runtime_error(windows_error());

        const auto credential = load_or_create_credential(options.state_directory / "credential.bin");
        const auto credential_hex = hex(credential);
        const auto state_path = options.state_directory / "service-state.json";
        auto state = load_state(state_path);
        if (!state.clean_shutdown) ++state.unclean_restart_count;
        state.instance_id = make_instance_id();
        ++state.server_start_count;
        state.clean_shutdown = false;
        state.last_command = "startup";
        save_state(state_path, state);

        bool shutdown = false;
        std::uint32_t handled = 0;
        while (!shutdown && (options.maximum_requests == 0 || handled < options.maximum_requests)) {
            const bool connected =
                ConnectNamedPipe(pipe.get(), nullptr) != FALSE || GetLastError() == ERROR_PIPE_CONNECTED;
            if (!connected) throw std::runtime_error(windows_error());

            nlohmann::json request;
            nlohmann::json response{
                {"authenticated", false},
                {"accepted", false},
                {"selected_protocol_version", 0},
                {"instance_id", state.instance_id},
                {"request_count", state.request_count},
                {"unclean_restart_count", state.unclean_restart_count},
                {"status", "rejected"},
            };
            if (!read_frame(pipe.get(), request)) {
                response["error"] = "invalid_or_oversized_frame";
            } else if (!request.contains("protocol") || !request["protocol"].is_string() ||
                       !request.contains("minimum_protocol_version") ||
                       !request["minimum_protocol_version"].is_number_unsigned() ||
                       !request.contains("maximum_protocol_version") ||
                       !request["maximum_protocol_version"].is_number_unsigned() ||
                       request["minimum_protocol_version"].get<std::uint64_t>() >
                           (std::numeric_limits<std::uint32_t>::max)() ||
                       request["maximum_protocol_version"].get<std::uint64_t>() >
                           (std::numeric_limits<std::uint32_t>::max)() ||
                       !request.contains("credential") || !request["credential"].is_string() ||
                       !request.contains("command") || !request["command"].is_string()) {
                response["error"] = "invalid_request_shape";
            } else {
                const auto protocol = request["protocol"].get<std::string>();
                const auto minimum =
                    static_cast<std::uint32_t>(request["minimum_protocol_version"].get<std::uint64_t>());
                const auto maximum =
                    static_cast<std::uint32_t>(request["maximum_protocol_version"].get<std::uint64_t>());
                const auto supplied_credential = request["credential"].get<std::string>();
                if (protocol != "ubridge.local-ipc") {
                    response["error"] = "protocol_identity_not_supported";
                } else if (minimum > local_ipc_protocol_version || maximum < local_ipc_protocol_version ||
                           minimum > maximum) {
                    response["error"] = "protocol_version_not_supported";
                } else if (!constant_time_equal(supplied_credential, credential_hex)) {
                    response["error"] = "authentication_failed";
                } else {
                    response["authenticated"] = true;
                    response["selected_protocol_version"] = local_ipc_protocol_version;
                    const auto command = request["command"].get<std::string>();
                    if (command == "ping" || command == "status" || command == "shutdown") {
                        ++state.request_count;
                        ++handled;
                        state.last_command = command;
                        response["accepted"] = true;
                        response["status"] = command == "shutdown" ? "stopping" : "ready";
                        response["request_count"] = state.request_count;
                        response["unclean_restart_count"] = state.unclean_restart_count;
                        save_state(state_path, state);
                        shutdown = command == "shutdown";
                    } else {
                        response["error"] = "command_not_supported";
                    }
                }
            }
            write_frame(pipe.get(), response);
            FlushFileBuffers(pipe.get());
            DisconnectNamedPipe(pipe.get());
        }
        state.clean_shutdown = true;
        state.last_command = "shutdown";
        save_state(state_path, state);
        return 0;
    } catch (const std::exception&) {
        return 3;
    }
#else
    (void)supplied_options;
    return 4;
#endif
}

LocalIpcResponse send_local_ipc_command(const LocalIpcClientOptions& supplied_options, std::string command,
                                        std::uint32_t minimum_protocol_version,
                                        std::uint32_t maximum_protocol_version) {
    LocalIpcResponse response;
#ifdef _WIN32
    try {
        auto options = supplied_options;
        if (options.state_directory.empty()) options.state_directory = default_service_state_directory();
        const auto credential = load_or_create_credential(options.state_directory / "credential.bin");
        const auto pipe_name = channel_name(options.channel);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(options.connect_timeout_ms);
        WinHandle pipe;
        do {
            pipe.reset(
                CreateFileW(pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr));
            if (pipe.valid()) break;
            if (GetLastError() != ERROR_PIPE_BUSY && GetLastError() != ERROR_FILE_NOT_FOUND) {
                response.error = windows_error();
                return response;
            }
            WaitNamedPipeW(pipe_name.c_str(), 50);
        } while (std::chrono::steady_clock::now() < deadline);
        if (!pipe.valid()) {
            response.error = "service_pipe_unavailable";
            return response;
        }
        response.connected = true;
        const nlohmann::json request{
            {"protocol", "ubridge.local-ipc"},
            {"minimum_protocol_version", minimum_protocol_version},
            {"maximum_protocol_version", maximum_protocol_version},
            {"request_id", make_instance_id()},
            {"credential", hex(credential)},
            {"command", std::move(command)},
        };
        if (!write_frame(pipe.get(), request)) {
            response.error = "request_write_failed";
            return response;
        }
        nlohmann::json payload;
        if (!read_frame(pipe.get(), payload)) {
            response.error = "response_read_failed";
            return response;
        }
        return parse_response(payload);
    } catch (const std::exception& error) {
        response.error = error.what();
    }
#else
    (void)supplied_options;
    (void)command;
    (void)minimum_protocol_version;
    (void)maximum_protocol_version;
    response.error = "local_ipc_unavailable_on_this_platform";
#endif
    return response;
}

bool run_local_ipc_self_test(const std::filesystem::path& test_root, std::string& report) {
#ifdef _WIN32
    const auto run_root = test_root / ("run-" + make_instance_id());
    const auto channel = "self-test-" + make_instance_id();
    LocalIpcServerOptions server_options{run_root, channel, 3};
    int server_exit = -1;
    std::thread server([&] { server_exit = run_local_ipc_server(server_options); });
    const auto credential_path = run_root / "credential.bin";
    const auto credential_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!std::filesystem::exists(credential_path) && std::chrono::steady_clock::now() < credential_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    const LocalIpcClientOptions client_options{run_root, channel, 5'000};
    const auto malformed = [&] {
        nlohmann::json result;
        WinHandle pipe;
        const auto pipe_name = channel_name(channel);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        do {
            pipe.reset(
                CreateFileW(pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr));
            if (pipe.valid()) break;
            WaitNamedPipeW(pipe_name.c_str(), 50);
        } while (std::chrono::steady_clock::now() < deadline);
        const nlohmann::json request{{"protocol", 7},
                                     {"minimum_protocol_version", "invalid"},
                                     {"maximum_protocol_version", 1},
                                     {"credential", false},
                                     {"command", nlohmann::json::array()}};
        if (pipe.valid() && write_frame(pipe.get(), request)) (void)read_frame(pipe.get(), result);
        return result;
    }();
    const auto incompatible = send_local_ipc_command(client_options, "ping", 2, 2);
    const auto ping = send_local_ipc_command(client_options, "ping");
    const auto stop = send_local_ipc_command(client_options, "shutdown");
    server.join();

    const auto state_path = run_root / "service-state.json";
    const bool state_valid = std::filesystem::exists(state_path) && load_state(state_path).clean_shutdown;
    const bool credential_protected =
        std::filesystem::exists(credential_path) && std::filesystem::file_size(credential_path) > 32;
    const bool passed = server_exit == 0 && malformed.value("error", std::string{}) == "invalid_request_shape" &&
                        incompatible.connected && !incompatible.accepted &&
                        incompatible.error == "protocol_version_not_supported" && ping.connected &&
                        ping.authenticated && ping.accepted &&
                        ping.selected_protocol_version == local_ipc_protocol_version && stop.accepted && state_valid &&
                        credential_protected;
    report = passed ? "authenticated IPC self-test passed" : "authenticated IPC self-test failed";
    return passed;
#else
    (void)test_root;
    report = "authenticated Windows IPC unavailable on this platform";
    return false;
#endif
}

} // namespace ubridge::platform
