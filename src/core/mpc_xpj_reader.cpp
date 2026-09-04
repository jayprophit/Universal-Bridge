#include "ubridge/core/mpc_xpj_reader.hpp"

#include <nlohmann/json.hpp>
#include <zlib.h>

#include <array>
#include <fstream>
#include <sstream>

namespace ubridge::mpc {
namespace {

std::string read_gzip(const std::filesystem::path& path) {
    gzFile file = gzopen(path.string().c_str(), "rb");
    if (!file) return {};
    std::string result;
    std::array<char, 64 * 1024> buffer{};
    for (;;) {
        const int count = gzread(file, buffer.data(), static_cast<unsigned int>(buffer.size()));
        if (count < 0) { result.clear(); break; }
        if (count == 0) break;
        result.append(buffer.data(), static_cast<std::size_t>(count));
        if (result.size() > 128U * 1024U * 1024U) { result.clear(); break; }
    }
    gzclose(file);
    return result;
}

std::string sample_name(const nlohmann::json& value) {
    for (const auto* key : {"path", "fileName", "filename", "name"}) {
        if (value.contains(key) && value[key].is_string()) return value[key].get<std::string>();
    }
    return {};
}

} // namespace

XpjInspection inspect_xpj(const std::filesystem::path& project_file) {
    XpjInspection result;
    std::ifstream header(project_file, std::ios::binary);
    unsigned char signature[2]{};
    if (!header.read(reinterpret_cast<char*>(signature), 2)) {
        result.diagnostics.push_back({core::DiagnosticSeverity::error, "xpj_unreadable", "The XPJ could not be read.", "Select a readable working copy; do not modify the device project."});
        return result;
    }
    result.readable = true;
    result.gzip_container = signature[0] == 0x1f && signature[1] == 0x8b;
    if (!result.gzip_container) {
        result.diagnostics.push_back({core::DiagnosticSeverity::error, "xpj_not_gzip", "The selected file does not use the observed XPJ gzip container.", "Keep it unchanged and use a separately qualified format adapter."});
        return result;
    }
    const auto payload = read_gzip(project_file);
    const auto json_start = payload.find('{');
    if (payload.empty() || json_start == std::string::npos) {
        result.diagnostics.push_back({core::DiagnosticSeverity::error, "xpj_payload_unreadable", "The XPJ payload could not be decompressed or has no JSON object.", "Preserve the source and inspect it as an unsupported version."});
        return result;
    }
    std::istringstream prefix(payload.substr(0, json_start));
    for (std::string line; std::getline(prefix, line);) if (!line.empty()) result.preamble.push_back(line);
    try {
        const auto root = nlohmann::json::parse(payload.begin() + static_cast<std::ptrdiff_t>(json_start), payload.end());
        if (!root.contains("data") || !root["data"].is_object()) throw nlohmann::json::type_error::create(302, "missing data object", &root);
        const auto& data = root["data"];
        result.json_payload = true;
        result.schema_version = data.value("version", 0);
        result.master_tempo = data.value("masterTempo", 0.0);
        const auto count = [&data](const char* key) { return data.contains(key) && data[key].is_array() ? data[key].size() : 0U; };
        result.sample_count = count("samples");
        result.track_count = count("tracks");
        result.sequence_count = count("sequences");
        result.song_slot_count = count("songs");
        if (data.contains("samples") && data["samples"].is_array()) {
            for (const auto& sample : data["samples"]) result.sample_names.push_back(sample_name(sample));
        }
        auto stem = project_file.stem().string();
        const auto project_data = project_file.parent_path() / (stem + "_[ProjectData]");
        for (const auto& name : result.sample_names) {
            if (!name.empty() && std::filesystem::exists(project_data / std::filesystem::path(name).filename())) ++result.available_asset_count;
            else ++result.missing_asset_count;
        }
        result.diagnostics.push_back({core::DiagnosticSeverity::info, "xpj_read_only_observed", "The XPJ container and JSON metadata were parsed without changing the source.", "Translate fields into a new canonical branch; keep unknown fields preserved as unsupported metadata."});
        if (result.missing_asset_count) result.diagnostics.push_back({core::DiagnosticSeverity::warning, "xpj_assets_unresolved", "Some sample references were not resolved in the sibling ProjectData folder.", "Search approved sample roots read-only before offering a relink plan."});
    } catch (const std::exception& error) {
        result.diagnostics.push_back({core::DiagnosticSeverity::error, "xpj_json_invalid", std::string("The XPJ JSON payload was not accepted: ") + error.what(), "Preserve the source and add a version-specific parser fixture."});
    }
    return result;
}

} // namespace ubridge::mpc
