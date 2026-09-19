#include "ubridge/core/session_store.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace ubridge::core {
namespace {

bool valid_session_id(std::string_view value) noexcept {
    if (value.empty() || value.size() > 128 || value == "." || value == "..") return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isalnum(character) != 0 || character == '-' || character == '_' || character == '.';
    });
}

std::string checksum(std::string_view payload) {
    std::uint64_t value = 14'695'981'039'346'656'037ULL;
    for (const auto character : payload) {
        value ^= static_cast<unsigned char>(character);
        value *= 1'099'511'628'211ULL;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << value;
    return output.str();
}

std::optional<SyncPolicy> parse_sync_policy(std::string_view value) {
    if (value == "hardware_authoritative") return SyncPolicy::hardware_authoritative;
    if (value == "daw_authoritative") return SyncPolicy::daw_authoritative;
    if (value == "one_way_to_daw") return SyncPolicy::one_way_to_daw;
    if (value == "one_way_to_hardware") return SyncPolicy::one_way_to_hardware;
    if (value == "bidirectional") return SyncPolicy::bidirectional;
    if (value == "render_only") return SyncPolicy::render_only;
    if (value == "unsupported") return SyncPolicy::unsupported;
    return std::nullopt;
}

nlohmann::json serialize_session(const CanonicalSession& session) {
    nlohmann::json value{
        {"session_id", session.session_id},
        {"source_id", session.source_id},
        {"schema_version", session.schema_version},
        {"revision",
         {
             {"session", session.revision.session},
             {"hardware", session.revision.hardware},
             {"daw", session.revision.daw},
         }},
        {"metadata", session.metadata},
        {"assets", nlohmann::json::array()},
        {"midi_events", nlohmann::json::array()},
        {"parameters", nlohmann::json::array()},
    };
    for (const auto& asset : session.assets) {
        value["assets"].push_back({
            {"id", asset.id},
            {"source_path", asset.source_path},
            {"fingerprint", asset.fingerprint},
            {"bytes", asset.bytes},
            {"required", asset.required},
        });
    }
    for (const auto& event : session.midi_events) {
        value["midi_events"].push_back({
            {"id", event.id},
            {"track_id", event.track_id},
            {"tick", event.tick},
            {"duration_ticks", event.duration_ticks},
            {"channel", event.channel},
            {"note", event.note},
            {"velocity", event.velocity},
            {"cc", event.cc},
            {"value", event.value},
        });
    }
    for (const auto& parameter : session.parameters) {
        value["parameters"].push_back({
            {"id", parameter.id},
            {"semantic", parameter.semantic},
            {"normalized_value", parameter.normalized_value},
            {"policy", to_string(parameter.policy)},
        });
    }
    return value;
}

CanonicalSession parse_session(const nlohmann::json& value, const SessionStoreLimits& limits) {
    if (!value.is_object()) throw std::runtime_error("session payload must be an object");
    CanonicalSession session;
    session.session_id = value.value("session_id", std::string{});
    session.source_id = value.value("source_id", std::string{});
    session.schema_version = value.value("schema_version", std::string{});
    if (!valid_session_id(session.session_id) || session.schema_version.empty()) {
        throw std::runtime_error("session identity or schema is invalid");
    }
    if (!value.contains("revision") || !value["revision"].is_object())
        throw std::runtime_error("session revision is missing");
    session.revision = {
        value["revision"].value("session", 0ULL),
        value["revision"].value("hardware", 0ULL),
        value["revision"].value("daw", 0ULL),
    };
    if (value.contains("metadata")) {
        if (!value["metadata"].is_object() || value["metadata"].size() > limits.maximum_metadata_entries) {
            throw std::runtime_error("session metadata exceeds its bound");
        }
        session.metadata = value["metadata"].get<std::map<std::string, std::string>>();
    }
    const auto bounded_array = [&](const char* name, std::size_t maximum) -> const nlohmann::json& {
        if (!value.contains(name) || !value[name].is_array() || value[name].size() > maximum) {
            throw std::runtime_error(std::string("session array is missing or exceeds its bound: ") + name);
        }
        return value[name];
    };
    for (const auto& item : bounded_array("assets", limits.maximum_assets)) {
        session.assets.push_back({
            item.value("id", std::string{}),
            item.value("source_path", std::string{}),
            item.value("fingerprint", std::string{}),
            item.value("bytes", 0ULL),
            item.value("required", true),
        });
    }
    for (const auto& item : bounded_array("midi_events", limits.maximum_midi_events)) {
        MusicalEvent event;
        event.id = item.value("id", std::string{});
        event.track_id = item.value("track_id", std::string{});
        event.tick = item.value("tick", 0LL);
        event.duration_ticks = item.value("duration_ticks", 0LL);
        event.channel = item.value("channel", 0);
        event.note = item.value("note", -1);
        event.velocity = item.value("velocity", -1);
        if (item.contains("cc") && !item["cc"].is_null()) event.cc = item["cc"].get<int>();
        if (item.contains("value") && !item["value"].is_null()) event.value = item["value"].get<double>();
        session.midi_events.push_back(std::move(event));
    }
    for (const auto& item : bounded_array("parameters", limits.maximum_parameters)) {
        const auto policy = parse_sync_policy(item.value("policy", std::string{}));
        if (!policy) throw std::runtime_error("session parameter contains an unknown sync policy");
        session.parameters.push_back({
            item.value("id", std::string{}),
            item.value("semantic", std::string{}),
            item.value("normalized_value", 0.0),
            *policy,
        });
    }
    return session;
}

std::string make_envelope(const CanonicalSession& session) {
    const auto payload = serialize_session(session);
    const auto canonical = payload.dump();
    const nlohmann::json envelope{
        {"format", "ubridge.session-snapshot"},
        {"envelope_version", 1},
        {"integrity", {{"algorithm", "fnv1a64-corruption-check"}, {"value", checksum(canonical)}}},
        {"payload", payload},
    };
    return envelope.dump(2) + "\n";
}

CanonicalSession parse_envelope(const std::filesystem::path& path, const SessionStoreLimits& limits) {
    if (!std::filesystem::exists(path)) throw std::runtime_error("snapshot does not exist");
    const auto size = std::filesystem::file_size(path);
    if (size == 0 || size > limits.maximum_snapshot_bytes) throw std::runtime_error("snapshot size is invalid");
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("snapshot could not be opened");
    const auto envelope = nlohmann::json::parse(input);
    if (envelope.value("format", std::string{}) != "ubridge.session-snapshot" ||
        envelope.value("envelope_version", 0) != 1 || !envelope.contains("payload") ||
        !envelope.contains("integrity") || !envelope["integrity"].is_object()) {
        throw std::runtime_error("snapshot envelope is unsupported");
    }
    if (envelope["integrity"].value("algorithm", std::string{}) != "fnv1a64-corruption-check" ||
        envelope["integrity"].value("value", std::string{}) != checksum(envelope["payload"].dump())) {
        throw std::runtime_error("snapshot integrity check failed");
    }
    return parse_session(envelope["payload"], limits);
}

void write_pending(const std::filesystem::path& path, std::string_view payload) {
    std::filesystem::create_directories(path.parent_path());
#ifdef _WIN32
    const auto handle = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("pending snapshot could not be opened: " + std::to_string(GetLastError()));
    }
    std::size_t offset = 0;
    while (offset < payload.size()) {
        DWORD written = 0;
        const auto remaining = payload.size() - offset;
        const auto chunk = static_cast<DWORD>((std::min)(remaining, static_cast<std::size_t>(MAXDWORD)));
        if (!WriteFile(handle, payload.data() + offset, chunk, &written, nullptr) || written == 0) {
            const auto error = GetLastError();
            CloseHandle(handle);
            throw std::runtime_error("pending snapshot write failed: " + std::to_string(error));
        }
        offset += written;
    }
    if (!FlushFileBuffers(handle)) {
        const auto error = GetLastError();
        CloseHandle(handle);
        throw std::runtime_error("pending snapshot flush failed: " + std::to_string(error));
    }
    if (!CloseHandle(handle)) {
        throw std::runtime_error("pending snapshot close failed: " + std::to_string(GetLastError()));
    }
#else
    const auto handle = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (handle < 0) throw std::runtime_error("pending snapshot could not be opened: " + std::to_string(errno));
    std::size_t offset = 0;
    while (offset < payload.size()) {
        const auto written = ::write(handle, payload.data() + offset, payload.size() - offset);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) {
            const auto error = errno;
            ::close(handle);
            throw std::runtime_error("pending snapshot write failed: " + std::to_string(error));
        }
        offset += static_cast<std::size_t>(written);
    }
    if (::fsync(handle) != 0) {
        const auto error = errno;
        ::close(handle);
        throw std::runtime_error("pending snapshot flush failed: " + std::to_string(error));
    }
    if (::close(handle) != 0) {
        throw std::runtime_error("pending snapshot close failed: " + std::to_string(errno));
    }
#endif
}

void replace_file(const std::filesystem::path& source, const std::filesystem::path& destination) {
#ifdef _WIN32
    if (!MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        throw std::runtime_error("atomic snapshot replacement failed: " + std::to_string(GetLastError()));
    }
#else
    std::error_code error;
    std::filesystem::rename(source, destination, error);
    if (error) throw std::runtime_error("atomic snapshot replacement failed: " + error.message());
#endif
}

} // namespace

DurableSessionStore::DurableSessionStore(std::filesystem::path root, SessionStoreLimits limits)
    : root_(std::move(root)), limits_(limits) {}

bool DurableSessionStore::save(const CanonicalSession& session, std::vector<Diagnostic>& diagnostics) const {
    if (!valid_session_id(session.session_id)) {
        diagnostics.push_back({DiagnosticSeverity::error, "session_store_invalid_id",
                               "The session ID is empty, too long, or unsafe for a local path.",
                               "Use only letters, digits, period, hyphen and underscore in the stable session ID."});
        return false;
    }
    try {
        const auto payload = make_envelope(session);
        if (payload.size() > limits_.maximum_snapshot_bytes)
            throw std::runtime_error("serialized snapshot exceeds its bound");
        const auto current = snapshot_path(session.session_id);
        const auto pending = current.string() + ".pending";
        const auto previous = current.string() + ".previous";
        write_pending(pending, payload);
        (void)parse_envelope(pending, limits_);
        if (std::filesystem::exists(current)) replace_file(current, previous);
        replace_file(pending, current);
        diagnostics.push_back(
            {DiagnosticSeverity::info, "session_snapshot_saved",
             "The canonical session snapshot was validated and atomically replaced.",
             "Keep the previous snapshot until the new revision has passed its workflow verification gate."});
        return true;
    } catch (const std::exception& error) {
        diagnostics.push_back(
            {DiagnosticSeverity::error, "session_snapshot_save_failed", error.what(),
             "Preserve the current and previous snapshots and retry only after resolving the storage error."});
        return false;
    }
}

SessionLoadResult DurableSessionStore::load(std::string_view session_id) const {
    SessionLoadResult result;
    if (!valid_session_id(session_id)) {
        result.diagnostics.push_back({DiagnosticSeverity::error, "session_store_invalid_id",
                                      "The requested session ID is not a safe storage key.",
                                      "Use the stable ID recorded in the canonical project manifest."});
        return result;
    }
    const auto current = snapshot_path(session_id);
    const std::array candidates{
        std::pair{current, SessionLoadSource::current},
        std::pair{std::filesystem::path(current.string() + ".pending"), SessionLoadSource::pending},
        std::pair{std::filesystem::path(current.string() + ".previous"), SessionLoadSource::previous},
    };
    for (const auto& [path, source] : candidates) {
        if (!std::filesystem::exists(path)) continue;
        try {
            result.session = parse_envelope(path, limits_);
            result.source = source;
            result.recovered = source != SessionLoadSource::current;
            result.diagnostics.push_back({
                result.recovered ? DiagnosticSeverity::warning : DiagnosticSeverity::info,
                result.recovered ? "session_snapshot_recovered" : "session_snapshot_loaded",
                result.recovered
                    ? "The current snapshot was unavailable or invalid; a validated recovery snapshot was loaded."
                    : "The current canonical snapshot passed its envelope and integrity checks.",
                result.recovered ? "Review the recovered revision before resuming hardware or DAW writes."
                                 : "Revalidate attached endpoints before resuming live work.",
            });
            return result;
        } catch (const std::exception& error) {
            result.diagnostics.push_back({DiagnosticSeverity::warning, "session_snapshot_candidate_rejected",
                                          path.filename().string() + ": " + error.what(),
                                          "The store will try the next bounded recovery candidate."});
        }
    }
    result.diagnostics.push_back(
        {DiagnosticSeverity::error, "session_snapshot_unavailable",
         "No validated current, pending, or previous snapshot is available.",
         "Restore an approved backup or create a new session without overwriting the damaged files."});
    return result;
}

std::filesystem::path DurableSessionStore::snapshot_path(std::string_view session_id) const {
    return root_ / std::string(session_id) / "session.ubridge.json";
}

const std::filesystem::path& DurableSessionStore::root() const noexcept { return root_; }

std::string to_string(SessionLoadSource source) {
    switch (source) {
    case SessionLoadSource::none: return "none";
    case SessionLoadSource::current: return "current";
    case SessionLoadSource::pending: return "pending";
    case SessionLoadSource::previous: return "previous";
    }
    return "none";
}

} // namespace ubridge::core
