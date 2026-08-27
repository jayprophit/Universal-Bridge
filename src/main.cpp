#include "ubridge/platform/hardware_backends.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

namespace ubridge {

constexpr std::string_view kProductName = "Universal Hardware Session Bridge";
constexpr std::string_view kSchemaVersion = "0.1.0";
constexpr std::uintmax_t kLargeFileWarningBytes = 1ULL * 1024ULL * 1024ULL * 1024ULL;

struct Options {
    fs::path project;
    fs::path output;
    std::string daw;
    std::string platform = "windows";
    std::string device = "mpc-sample";
    bool create_backup = true;
    bool copy_assets = true;
};

struct Finding {
    std::string severity;
    std::string code;
    std::string message;
    std::string recommendation;
};

struct Asset {
    fs::path absolute_path;
    fs::path relative_path;
    std::string type;
    std::uintmax_t bytes = 0;
    std::string fingerprint;
    bool copied = false;
};

struct Inventory {
    std::vector<Asset> audio;
    std::vector<Asset> midi;
    std::vector<fs::path> projects;
    std::vector<fs::path> unknown;
    std::uintmax_t total_bytes = 0;
};

struct Capability {
    bool project_read = false;
    bool project_write = false;
    bool usb_midi = false;
    bool usb_audio = false;
    bool hardware_service_active = false;
    int audio_channels = 0;
    bool direct_daw_project_generation = false;
    bool vst3_client = false;
    bool sequential_stem_capture = false;
    bool bidirectional_parameters = false;
    std::string capture_route;
};

struct PlatformContract {
    std::string id;
    std::string state;
    std::string host_mode;
    bool local_preflight = false;
    bool runtime_qualified = false;
    bool desktop_plugin_route = false;
    bool mobile_companion_route = false;
    bool direct_mobile_host = false;
};

struct Session {
    std::string id;
    std::string created_at;
    std::string source_name;
    std::string daw;
    std::string device;
    std::string project_fingerprint;
    PlatformContract platform;
    Capability capability;
    Inventory inventory;
    std::vector<Finding> findings;
    bool backup_requested = false;
    bool backup_completed = false;
};

[[nodiscard]] std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

[[nodiscard]] std::string json_escape(std::string_view value) {
    std::ostringstream stream;
    for (const char character : value) {
        switch (character) {
            case '\\': stream << "\\\\"; break;
            case '\"': stream << "\\\""; break;
            case '\n': stream << "\\n"; break;
            case '\r': stream << "\\r"; break;
            case '\t': stream << "\\t"; break;
            default:
                if (static_cast<unsigned char>(character) < 0x20U) {
                    stream << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                           << static_cast<int>(static_cast<unsigned char>(character)) << std::dec;
                } else {
                    stream << character;
                }
        }
    }
    return stream.str();
}

[[nodiscard]] std::string iso8601_now() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    std::ostringstream stream;
    stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return stream.str();
}

[[nodiscard]] std::string fnv1a_file(const fs::path& path) {
    constexpr std::uint64_t offset = 14695981039346656037ULL;
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t hash = offset;

    std::ifstream source(path, std::ios::binary);
    if (!source) {
        return "unreadable";
    }

    std::array<char, 65536> buffer{};
    while (source.good()) {
        source.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto read = source.gcount();
        for (std::streamsize index = 0; index < read; ++index) {
            hash ^= static_cast<std::uint8_t>(buffer[static_cast<std::size_t>(index)]);
            hash *= prime;
        }
    }

    std::ostringstream stream;
    stream << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return stream.str();
}

[[nodiscard]] std::string kind_for_extension(const fs::path& path) {
    const auto extension = lower(path.extension().string());
    static const std::set<std::string> audio_extensions = {".wav", ".aif", ".aiff", ".flac", ".mp3", ".ogg"};
    static const std::set<std::string> midi_extensions = {".mid", ".midi"};
    static const std::set<std::string> project_extensions = {".xpj", ".xpm", ".xpn", ".akp", ".als", ".cpr", ".reason", ".rns"};

    if (audio_extensions.contains(extension)) {
        return "audio";
    }
    if (midi_extensions.contains(extension)) {
        return "midi";
    }
    if (project_extensions.contains(extension)) {
        return "project";
    }
    return "unknown";
}

[[nodiscard]] std::string path_to_posix(const fs::path& path) {
    return path.generic_string();
}

[[nodiscard]] bool is_same_or_nested(const fs::path& candidate, const fs::path& parent) {
    auto candidate_part = candidate.begin();
    for (auto parent_part = parent.begin(); parent_part != parent.end(); ++parent_part, ++candidate_part) {
        if (candidate_part == candidate.end() || *candidate_part != *parent_part) {
            return false;
        }
    }
    return true;
}

void ensure_parent(const fs::path& path) {
    std::error_code error;
    fs::create_directories(path.parent_path(), error);
    if (error) {
        throw std::runtime_error("Cannot create directory '" + path.parent_path().string() + "': " + error.message());
    }
}

void write_text(const fs::path& destination, const std::string& content) {
    ensure_parent(destination);
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Cannot write '" + destination.string() + "'.");
    }
    output << content;
    if (!output.good()) {
        throw std::runtime_error("Write failed for '" + destination.string() + "'.");
    }
}

void copy_safely(const fs::path& source, const fs::path& destination) {
    ensure_parent(destination);
    std::error_code error;
    fs::copy_file(source, destination, fs::copy_options::overwrite_existing, error);
    if (error) {
        throw std::runtime_error("Could not copy '" + source.string() + "': " + error.message());
    }
}

[[nodiscard]] bool valid_daw(const std::string& daw) {
    return daw == "cubase" || daw == "reason";
}

[[nodiscard]] bool valid_platform(const std::string& platform) {
    static const std::set<std::string> supported = {
        "windows", "macos", "linux", "android", "chromeos", "ipados", "ios"
    };
    return supported.contains(platform);
}

[[nodiscard]] std::string usage() {
    return R"(Universal Hardware Session Bridge — developer prototype

Usage:
  ubridge preflight --project <folder> --daw <cubase|reason> --output <folder> [options]
  ubridge devices

Options:
  --target-os <platform> Target capability profile: windows, macos, linux, android, chromeos, ipados, ios. Default: windows.
  --device <mpc-sample>  Reference hardware profile. Default: mpc-sample.
  --no-backup            Do not create a read-only project backup copy.
  --no-copy-assets       Inventory assets but do not copy audio/MIDI into Exchange/.
  --help                 Show this help text.

Safety model:
  The source project directory is read-only from the bridge's perspective. All output,
  backups, manifests, and exchange files are written to the selected output folder.
  Device inventory is read-only: matching interfaces are never opened or controlled.
)";
}

[[nodiscard]] Options parse_options(int argc, char* argv[]) {
    if (argc < 2 || std::string_view(argv[1]) != "preflight") {
        throw std::runtime_error(usage());
    }

    Options options;
    for (int index = 2; index < argc; ++index) {
        const std::string argument = argv[index];
        const auto require_value = [&](const std::string& option) -> std::string {
            if (index + 1 >= argc) {
                throw std::runtime_error("Missing value for " + option + ".\n\n" + usage());
            }
            ++index;
            return argv[index];
        };

        if (argument == "--project") {
            options.project = require_value(argument);
        } else if (argument == "--output") {
            options.output = require_value(argument);
        } else if (argument == "--daw") {
            options.daw = lower(require_value(argument));
        } else if (argument == "--target-os") {
            options.platform = lower(require_value(argument));
        } else if (argument == "--device") {
            options.device = lower(require_value(argument));
        } else if (argument == "--no-backup") {
            options.create_backup = false;
        } else if (argument == "--no-copy-assets") {
            options.copy_assets = false;
        } else if (argument == "--help") {
            throw std::runtime_error(usage());
        } else {
            throw std::runtime_error("Unknown option: " + argument + "\n\n" + usage());
        }
    }

    if (options.project.empty() || options.output.empty() || options.daw.empty()) {
        throw std::runtime_error("--project, --daw, and --output are required.\n\n" + usage());
    }
    if (!valid_daw(options.daw)) {
        throw std::runtime_error("The initial prototype supports only Cubase or Reason as targets.");
    }
    if (!valid_platform(options.platform)) {
        throw std::runtime_error("Supported target operating systems are windows, macos, linux, android, chromeos, ipados, and ios.");
    }
    if (options.device != "mpc-sample") {
        throw std::runtime_error("The initial reference profile is mpc-sample. Additional profiles are planned but not active.");
    }
    return options;
}

[[nodiscard]] Inventory build_inventory(const fs::path& project_root, std::vector<Finding>& findings) {
    Inventory inventory;
    std::error_code error;
    fs::recursive_directory_iterator iterator(project_root, fs::directory_options::skip_permission_denied, error);
    if (error) {
        findings.push_back({"warning", "directory_scan_limited", "The project directory could not be scanned completely.", "Check folder permissions and run the preflight again."});
    }

    for (const auto& entry : iterator) {
        if (entry.is_directory(error)) {
            continue;
        }
        if (error || !entry.is_regular_file(error)) {
            error.clear();
            continue;
        }

        const fs::path absolute_path = entry.path();
        const fs::path relative_path = fs::relative(absolute_path, project_root, error);
        if (error) {
            error.clear();
            continue;
        }
        const auto type = kind_for_extension(absolute_path);
        const auto size = entry.file_size(error);
        if (error) {
            error.clear();
            findings.push_back({"warning", "file_size_unavailable", "File size could not be determined for " + path_to_posix(relative_path) + ".", "Confirm this file is readable before transfer."});
            continue;
        }

        inventory.total_bytes += size;
        if (type == "project") {
            inventory.projects.push_back(relative_path);
            continue;
        }
        if (type == "unknown") {
            inventory.unknown.push_back(relative_path);
            continue;
        }

        Asset asset;
        asset.absolute_path = absolute_path;
        asset.relative_path = relative_path;
        asset.type = type;
        asset.bytes = size;
        asset.fingerprint = fnv1a_file(absolute_path);
        if (type == "audio") {
            inventory.audio.push_back(std::move(asset));
        } else {
            inventory.midi.push_back(std::move(asset));
        }
    }

    if (inventory.projects.empty()) {
        findings.push_back({"warning", "project_file_not_detected", "No recognized project file was found in the selected folder.", "Select the complete MPC project directory or continue with an asset-only bridge session."});
    }
    if (inventory.audio.empty()) {
        findings.push_back({"warning", "no_audio_assets", "No supported audio assets were found.", "Confirm that associated ProjectData or sample folders were included."});
    }
    if (inventory.total_bytes > kLargeFileWarningBytes) {
        findings.push_back({"info", "large_project", "The selected project contains more than 1 GiB of files.", "Choose a destination with sufficient free storage; copy operations can take time."});
    }

    return inventory;
}

[[nodiscard]] std::string session_fingerprint(const Inventory& inventory) {
    std::ostringstream joined;
    for (const auto& project : inventory.projects) {
        joined << path_to_posix(project) << '\n';
    }
    for (const auto& asset : inventory.audio) {
        joined << path_to_posix(asset.relative_path) << ':' << asset.fingerprint << '\n';
    }
    for (const auto& asset : inventory.midi) {
        joined << path_to_posix(asset.relative_path) << ':' << asset.fingerprint << '\n';
    }

    const auto staging_path = fs::temp_directory_path() / "ubridge-session-fingerprint.tmp";
    write_text(staging_path, joined.str());
    const auto fingerprint = fnv1a_file(staging_path);
    std::error_code error;
    fs::remove(staging_path, error);
    return fingerprint;
}

[[nodiscard]] PlatformContract platform_contract(const std::string& platform) {
    PlatformContract contract;
    contract.id = platform;

    if (platform == "windows") {
        contract.state = "reference_desktop";
        contract.host_mode = "desktop_bridge";
        contract.local_preflight = true;
        contract.runtime_qualified = true;
        contract.desktop_plugin_route = true;
        return contract;
    }

    if (platform == "macos" || platform == "linux") {
        contract.state = "portable_core_target";
        contract.host_mode = "desktop_bridge";
        contract.local_preflight = true;
        contract.runtime_qualified = false;
        contract.desktop_plugin_route = true;
        return contract;
    }

    contract.state = "future_mobile_host";
    contract.host_mode = "mobile_bridge_or_companion";
    contract.local_preflight = false;
    contract.runtime_qualified = false;
    contract.mobile_companion_route = true;
    contract.direct_mobile_host = false;
    return contract;
}

[[nodiscard]] Capability mpc_sample_capability(const PlatformContract& platform) {
    Capability capability;
    capability.project_read = true;
    capability.project_write = false;
    capability.usb_midi = true;
    capability.usb_audio = true;
    capability.hardware_service_active = false;
    capability.audio_channels = 2;
    capability.direct_daw_project_generation = false;
    capability.vst3_client = false;
    capability.sequential_stem_capture = false;
    capability.bidirectional_parameters = false;
    capability.capture_route = "stereo_usb_or_analogue_capture";

    if (!platform.desktop_plugin_route) {
        capability.usb_midi = false;
        capability.usb_audio = false;
        capability.audio_channels = 0;
        capability.capture_route = "future_mobile_or_companion_route";
    }
    return capability;
}

void add_capability_findings(Session& session) {
    session.findings.push_back({"info", "effective_route", "The selected route is MPC Sample → " + session.platform.id + " → " + session.daw + ".", "Use the generated Exchange package as the safe initial DAW hand-off."});
    if (session.platform.state == "portable_core_target") {
        session.findings.push_back({"warning", "platform_not_qualified", "The " + session.platform.id + " profile is hard-coded as a portable desktop-core target, not a certified runtime route.", "Use the output for planning or developer testing; qualify device, audio, MIDI, plug-in, and DAW behavior before claiming support."});
    }
    if (session.platform.state == "future_mobile_host") {
        session.findings.push_back({"info", "mobile_host_planned", "The " + session.platform.id + " profile is hard-coded for future mobile/tablet bridge and companion modes.", "No native mobile executable or direct hardware/DAW route is enabled in this prototype; treat the session as a capability plan and exchange record."});
        session.findings.push_back({"warning", "desktop_daw_handoff_only", "Cubase and Reason are retained as desktop hand-off targets in this mobile/tablet capability record.", "Do not interpret this as native Cubase or Reason hosting on the selected mobile/tablet platform."});
    }
    if (session.capability.audio_channels == 2) {
        session.findings.push_back({"warning", "stereo_capture_limit", "The reference device profile declares two audio channels; the current prototype does not open hardware audio endpoints.", "Do not expect simultaneous individual pad stems; use qualified sequential capture only after device-control validation."});
    }
    session.findings.push_back({"info", "daw_project_generation_gated", "Direct native " + session.daw + " project-file generation is intentionally disabled in this prototype.", "The bridge writes a transparent exchange bundle until the relevant supported adapter route is validated."});
    session.findings.push_back({"info", "writeback_disabled", "Hardware project write-back and live parameter synchronization are disabled.", "The source remains protected while parser, capability, and transaction modules mature."});
}

[[nodiscard]] std::string bool_json(bool value) {
    return value ? "true" : "false";
}

void write_asset_json(std::ostringstream& stream, const std::vector<Asset>& assets, int indent) {
    const std::string padding(static_cast<std::size_t>(indent), ' ');
    const std::string inner(static_cast<std::size_t>(indent + 2), ' ');
    stream << "[\n";
    for (std::size_t index = 0; index < assets.size(); ++index) {
        const auto& asset = assets[index];
        stream << inner << "{\n"
               << inner << "  \"path\": \"" << json_escape(path_to_posix(asset.relative_path)) << "\",\n"
               << inner << "  \"type\": \"" << asset.type << "\",\n"
               << inner << "  \"bytes\": " << asset.bytes << ",\n"
               << inner << "  \"fingerprint\": \"" << asset.fingerprint << "\",\n"
               << inner << "  \"copied_to_exchange\": " << bool_json(asset.copied) << "\n"
               << inner << "}";
        if (index + 1 < assets.size()) {
            stream << ',';
        }
        stream << '\n';
    }
    stream << padding << ']';
}

void write_findings_json(std::ostringstream& stream, const std::vector<Finding>& findings, int indent) {
    const std::string padding(static_cast<std::size_t>(indent), ' ');
    const std::string inner(static_cast<std::size_t>(indent + 2), ' ');
    stream << "[\n";
    for (std::size_t index = 0; index < findings.size(); ++index) {
        const auto& finding = findings[index];
        stream << inner << "{\n"
               << inner << "  \"severity\": \"" << finding.severity << "\",\n"
               << inner << "  \"code\": \"" << finding.code << "\",\n"
               << inner << "  \"message\": \"" << json_escape(finding.message) << "\",\n"
               << inner << "  \"recommendation\": \"" << json_escape(finding.recommendation) << "\"\n"
               << inner << "}";
        if (index + 1 < findings.size()) {
            stream << ',';
        }
        stream << '\n';
    }
    stream << padding << ']';
}

[[nodiscard]] std::string session_json(const Session& session) {
    const auto& capability = session.capability;
    std::ostringstream stream;
    stream << "{\n"
           << "  \"schema_version\": \"" << kSchemaVersion << "\",\n"
           << "  \"bridge_version\": \"" << UBRIDGE_VERSION << "\",\n"
           << "  \"session_id\": \"" << session.id << "\",\n"
           << "  \"created_at\": \"" << session.created_at << "\",\n"
           << "  \"safety_mode\": \"read_only_source\",\n"
           << "  \"source\": {\n"
           << "    \"display_name\": \"" << json_escape(session.source_name) << "\",\n"
           << "    \"fingerprint\": \"" << session.project_fingerprint << "\"\n"
           << "  },\n"
           << "  \"reference_route\": {\n"
           << "    \"device_profile\": \"" << session.device << "\",\n"
           << "    \"target_daw\": \"" << session.daw << "\",\n"
           << "    \"target_os\": \"" << session.platform.id << "\"\n"
           << "  },\n"
           << "  \"platform_capability\": {\n"
           << "    \"state\": \"" << session.platform.state << "\",\n"
           << "    \"host_mode\": \"" << session.platform.host_mode << "\",\n"
           << "    \"local_preflight\": " << bool_json(session.platform.local_preflight) << ",\n"
           << "    \"runtime_qualified\": " << bool_json(session.platform.runtime_qualified) << ",\n"
           << "    \"desktop_plugin_route\": " << bool_json(session.platform.desktop_plugin_route) << ",\n"
           << "    \"mobile_companion_route\": " << bool_json(session.platform.mobile_companion_route) << ",\n"
           << "    \"direct_mobile_host\": " << bool_json(session.platform.direct_mobile_host) << "\n"
           << "  },\n"
           << "  \"effective_capability\": {\n"
           << "    \"project_read\": " << bool_json(capability.project_read) << ",\n"
           << "    \"project_write\": " << bool_json(capability.project_write) << ",\n"
           << "    \"usb_midi\": " << bool_json(capability.usb_midi) << ",\n"
           << "    \"usb_audio\": " << bool_json(capability.usb_audio) << ",\n"
           << "    \"hardware_service_active\": " << bool_json(capability.hardware_service_active) << ",\n"
           << "    \"audio_channels\": " << capability.audio_channels << ",\n"
           << "    \"direct_daw_project_generation\": " << bool_json(capability.direct_daw_project_generation) << ",\n"
           << "    \"vst3_client\": " << bool_json(capability.vst3_client) << ",\n"
           << "    \"sequential_stem_capture\": " << bool_json(capability.sequential_stem_capture) << ",\n"
           << "    \"bidirectional_parameters\": " << bool_json(capability.bidirectional_parameters) << ",\n"
           << "    \"capture_route\": \"" << capability.capture_route << "\"\n"
           << "  },\n"
           << "  \"inventory\": {\n"
           << "    \"audio_assets\": ";
    write_asset_json(stream, session.inventory.audio, 4);
    stream << ",\n    \"midi_assets\": ";
    write_asset_json(stream, session.inventory.midi, 4);
    stream << ",\n    \"project_files\": [";
    for (std::size_t index = 0; index < session.inventory.projects.size(); ++index) {
        stream << "\"" << json_escape(path_to_posix(session.inventory.projects[index])) << "\"";
        if (index + 1 < session.inventory.projects.size()) {
            stream << ", ";
        }
    }
    stream << "],\n    \"total_bytes\": " << session.inventory.total_bytes << "\n"
           << "  },\n"
           << "  \"findings\": ";
    write_findings_json(stream, session.findings, 2);
    stream << "\n}\n";
    return stream.str();
}

[[nodiscard]] std::string diagnostics_json(const Session& session) {
    std::ostringstream stream;
    stream << "{\n"
           << "  \"bridge_version\": \"" << UBRIDGE_VERSION << "\",\n"
           << "  \"session_id\": \"" << session.id << "\",\n"
           << "  \"created_at\": \"" << session.created_at << "\",\n"
           << "  \"source_write_attempted\": false,\n"
           << "  \"source_backup_requested\": " << bool_json(session.backup_requested) << ",\n"
           << "  \"source_backup_created\": " << bool_json(session.backup_completed) << ",\n"
           << "  \"project_file_count\": " << session.inventory.projects.size() << ",\n"
           << "  \"audio_asset_count\": " << session.inventory.audio.size() << ",\n"
           << "  \"midi_asset_count\": " << session.inventory.midi.size() << ",\n"
           << "  \"unclassified_file_count\": " << session.inventory.unknown.size() << ",\n"
           << "  \"findings\": ";
    write_findings_json(stream, session.findings, 2);
    stream << "\n}\n";
    return stream.str();
}

[[nodiscard]] std::string markdown_report(const Session& session, const Options& options) {
    std::size_t warnings = 0;
    for (const auto& finding : session.findings) {
        if (finding.severity == "warning") {
            ++warnings;
        }
    }

    std::ostringstream stream;
    stream << "# Universal Bridge Preflight Report\n\n"
           << "**Session:** `" << session.id << "`  \n"
           << "**Created:** " << session.created_at << "  \n"
           << "**Selected workflow:** MPC Sample → " << session.platform.id << " → " << (session.daw == "cubase" ? "Cubase" : "Reason") << "  \n"
           << "**Platform state:** `" << session.platform.state << "` / `" << session.platform.host_mode << "`  \n"
           << "**Source safety:** Read-only source; all generated data is written below the selected output folder.\n\n"
           << "> This developer prototype creates a transparent exchange package. It does not modify the source hardware project, write back to the MPC, generate proprietary DAW project files, or claim live synchronization.\n\n"
           << "## Capability outcome\n\n"
           << "| Capability | Status |\n|---|---|\n"
           << "| Project intake | " << (session.platform.local_preflight ? "Supported by the platform contract" : "Recorded for a future native mobile/tablet host") << " |\n"
           << "| Platform runtime qualification | " << (session.platform.runtime_qualified ? "Windows reference route" : "Not qualified; capability profile only") << " |\n"
           << "| Asset inventory and fingerprinting | Supported in the shared core |\n"
           << "| USB MIDI route | " << (session.capability.usb_midi ? "Profile-declared; hardware service not yet active" : "Not active on this platform profile") << " |\n"
           << "| USB audio | " << (session.capability.usb_audio ? "Two-channel profile declaration; capture is not yet active" : "Not active on this platform profile") << " |\n"
           << "| Direct " << (session.daw == "cubase" ? "Cubase" : "Reason") << " project creation | Intentionally gated pending adapter validation |\n"
           << "| VST3 bridge client | " << (session.platform.desktop_plugin_route ? "Architecture target; not shipped in this CLI prototype" : "Not an active generic mobile/tablet route") << " |\n"
           << "| Sequential stem capture | Gated pending reliable device-control validation |\n"
           << "| Live bidirectional state synchronization | Not enabled |\n\n"
           << "## Inventory\n\n"
           << "| Item | Count |\n|---|---:|\n"
           << "| Recognized project files | " << session.inventory.projects.size() << " |\n"
           << "| Audio assets | " << session.inventory.audio.size() << " |\n"
           << "| MIDI assets | " << session.inventory.midi.size() << " |\n"
           << "| Unclassified files | " << session.inventory.unknown.size() << " |\n"
           << "| Total scanned bytes | " << session.inventory.total_bytes << " |\n"
           << "| Warnings | " << warnings << " |\n\n"
           << "## Generated output\n\n"
           << "| Path | Purpose |\n|---|---|\n"
           << "| `session.ubridge.json` | Versioned neutral-session snapshot with capabilities and fingerprints |\n"
           << "| `diagnostics.json` | Machine-readable preflight findings |\n"
           << "| `Exchange/Audio/` | Copied audio assets, retaining original relative structure |\n"
           << "| `Exchange/MIDI/` | Copied MIDI assets, retaining original relative structure |\n"
           << "| `Exchange/IMPORT_" << (session.daw == "cubase" ? "CUBASE" : "REASON") << ".md` | Target-specific import procedure and limitations |\n"
           << "| `Backup/` | Read-only project backup copy, if enabled |\n\n"
           << "## Findings\n\n"
           << "| Severity | Code | Detail | Recommendation |\n|---|---|---|---|\n";
    for (const auto& finding : session.findings) {
        stream << "| " << finding.severity << " | `" << finding.code << "` | " << finding.message << " | " << finding.recommendation << " |\n";
    }
    stream << "\n## Import path\n\n"
           << "Import the copied audio and MIDI assets into a new project in " << (session.daw == "cubase" ? "Cubase" : "Reason")
           << ". Preserve their relative names and consult the session manifest before recreating any routing, effects, or automation. The next adapter phase will convert this auditable exchange package into host-assisted creation where the DAW exposes a safe documented route.\n\n"
           << "## Source\n\n"
           << "Selected project folder: `" << path_to_posix(options.project) << "`\n";
    return stream.str();
}

[[nodiscard]] std::string daw_import_guide(const Session& session) {
    const std::string daw_name = session.daw == "cubase" ? "Cubase" : "Reason";
    std::ostringstream stream;
    stream << "# " << daw_name << " Exchange Package\n\n"
           << "This package was generated by the Universal Hardware Session Bridge developer prototype. It preserves source assets and metadata without modifying the original MPC project.\n\n"
           << "## Safe import procedure\n\n"
           << "1. Create a new " << daw_name << " project at the project tempo and meter documented in `../session.ubridge.json` when available.\n"
           << "2. Import the contents of `Audio/` and `MIDI/` while retaining source names and folder context.\n"
           << "3. Use `../session.ubridge.json` and `../preflight-report.md` as the authoritative record of supported, rendered, and unavailable data.\n"
           << "4. Do not delete or overwrite the original MPC project or its ProjectData folder.\n\n"
           << "## Current limitations\n\n"
           << "The selected platform profile is `" << session.platform.id << "` with state `" << session.platform.state << "`. The prototype does not parse proprietary MPC arrangement, effects, or automation structures; create proprietary " << daw_name << " project files; or perform hardware write-back. These features require validated parser and host-adapter work.\n";
    return stream.str();
}

void copy_inventory_assets(Session& session, const Options& options) {
    if (!options.copy_assets) {
        return;
    }
    const fs::path exchange_root = options.output / "Exchange";
    const auto copy_assets = [&](std::vector<Asset>& assets, const std::string& category) {
        for (auto& asset : assets) {
            const fs::path destination = exchange_root / category / asset.relative_path;
            copy_safely(asset.absolute_path, destination);
            asset.copied = true;
        }
    };
    copy_assets(session.inventory.audio, "Audio");
    copy_assets(session.inventory.midi, "MIDI");
}

void backup_project(const Options& options, Session& session) {
    if (!options.create_backup) {
        session.findings.push_back({"info", "backup_skipped", "Backup creation was explicitly disabled.", "Keep a separate verified copy of the original project before further work."});
        return;
    }

    const fs::path backup_root = options.output / "Backup" / options.project.filename();
    std::error_code error;
    fs::create_directories(backup_root.parent_path(), error);
    if (error) {
        session.findings.push_back({"warning", "backup_directory_unavailable", "The backup directory could not be created: " + error.message(), "Confirm the output folder is writable before proceeding."});
        return;
    }
    fs::copy(options.project, backup_root, fs::copy_options::recursive | fs::copy_options::copy_symlinks, error);
    if (error) {
        session.findings.push_back({"warning", "backup_partial", "The backup copy did not complete: " + error.message(), "Confirm the source project has an independent backup before proceeding."});
    } else {
        session.backup_completed = true;
        session.findings.push_back({"info", "backup_created", "A separate output-folder backup copy was created.", "Retain this backup until the imported DAW session is verified."});
    }
}

[[nodiscard]] Session create_session(const Options& options) {
    Session session;
    session.created_at = iso8601_now();
    session.source_name = options.project.filename().string();
    session.daw = options.daw;
    session.device = options.device;
    session.platform = platform_contract(options.platform);
    session.capability = mpc_sample_capability(session.platform);
    session.backup_requested = options.create_backup;
    session.inventory = build_inventory(options.project, session.findings);
    session.project_fingerprint = session_fingerprint(session.inventory);
    session.id = "ubridge-" + session.created_at.substr(0, 10) + "-" + session.project_fingerprint.substr(session.project_fingerprint.size() - 8);
    add_capability_findings(session);
    return session;
}

void preflight(const Options& options) {
    std::error_code error;
    if (!fs::exists(options.project, error) || !fs::is_directory(options.project, error)) {
        throw std::runtime_error("The project path must be an existing directory: " + options.project.string());
    }

    fs::create_directories(options.output, error);
    if (error) {
        throw std::runtime_error("Cannot create output directory: " + error.message());
    }

    const fs::path canonical_project = fs::weakly_canonical(options.project, error);
    if (error) {
        throw std::runtime_error("Cannot resolve source project path: " + error.message());
    }
    const fs::path canonical_output = fs::weakly_canonical(options.output, error);
    if (error) {
        throw std::runtime_error("Cannot resolve output directory: " + error.message());
    }
    if (is_same_or_nested(canonical_output, canonical_project)) {
        throw std::runtime_error("The output folder must be outside the source project folder to preserve source safety.");
    }

    Session session = create_session(options);
    backup_project(options, session);
    copy_inventory_assets(session, options);

    write_text(options.output / "session.ubridge.json", session_json(session));
    write_text(options.output / "diagnostics.json", diagnostics_json(session));
    write_text(options.output / "preflight-report.md", markdown_report(session, options));
    write_text(options.output / "Exchange" / (session.daw == "cubase" ? "IMPORT_CUBASE.md" : "IMPORT_REASON.md"), daw_import_guide(session));

    std::cout << "Preflight completed safely\n"
              << "Session: " << session.id << "\n"
              << "Platform: " << session.platform.id << " (" << session.platform.state << ")\n"
              << "Output:  " << options.output << "\n"
              << "Assets:  " << session.inventory.audio.size() << " audio, " << session.inventory.midi.size() << " MIDI\n"
              << "Warnings: " << std::count_if(session.findings.begin(), session.findings.end(), [](const Finding& finding) { return finding.severity == "warning"; }) << "\n";
}

void list_devices() {
    auto discovery = platform::make_system_device_discovery();
    const auto devices = discovery->enumerate();
    std::cout << "Universal Bridge read-only device inventory\n"
              << "Observed identity filter: VID 09E8 / PID 205C\n"
              << "Backend maturity: experimental\n"
              << "Matching device containers: " << devices.size() << "\n";
    for (const auto& device : devices) {
        std::cout << "\nDevice: " << device.display_name << "\n"
                  << "Container: " << (device.container_id.empty() ? "unavailable" : device.container_id) << "\n"
                  << "Interfaces: " << device.interfaces.size() << "\n";
        for (const auto& usb_interface : device.interfaces) {
            std::cout << "  MI_" << (usb_interface.interface_number.empty() ? "??" : usb_interface.interface_number)
                      << " service=" << (usb_interface.service.empty() ? "unknown" : usb_interface.service)
                      << " name=" << (usb_interface.friendly_name.empty() ? "unknown" : usb_interface.friendly_name) << "\n";
        }
    }
    std::cout << "\nNo interfaces were opened. Discovery does not claim protocol, MIDI, audio, storage, or CDC-NCM support.\n";
}

} // namespace ubridge

int main(int argc, char* argv[]) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--help") {
            std::cout << ubridge::usage();
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "devices") {
            ubridge::list_devices();
            return 0;
        }
        const auto options = ubridge::parse_options(argc, argv);
        ubridge::preflight(options);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Universal Bridge error: " << error.what() << '\n';
        return 1;
    }
}
