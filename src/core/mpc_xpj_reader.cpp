#include "ubridge/core/mpc_xpj_reader.hpp"

#include <nlohmann/json.hpp>
#include <zlib.h>

#include <array>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <cmath>

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

std::string stable_id(std::string value) {
    for (auto& c : value) c = std::isalnum(static_cast<unsigned char>(c)) ? static_cast<char>(std::tolower(static_cast<unsigned char>(c))) : '-';
    value.erase(std::unique(value.begin(), value.end(), [](char a, char b) { return a == '-' && b == '-'; }), value.end());
    while (!value.empty() && value.front() == '-') value.erase(value.begin());
    while (!value.empty() && value.back() == '-') value.pop_back();
    return value.empty() ? "unnamed" : value;
}

nlohmann::json parse_root(const std::filesystem::path& path) {
    const auto payload = read_gzip(path);
    const auto start = payload.find('{');
    if (payload.empty() || start == std::string::npos) return {};
    return nlohmann::json::parse(payload.begin() + static_cast<std::ptrdiff_t>(start), payload.end());
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

XpjCanonicalImport import_xpj(const std::filesystem::path& project_file) {
    XpjCanonicalImport imported;
    try {
        const auto root = parse_root(project_file);
        if (!root.contains("data") || !root["data"].is_object()) throw std::runtime_error("missing project data object");
        const auto& data = root["data"];
        auto& output = imported.session;
        output.canonical.session_id = "mpc-" + stable_id(project_file.stem().string());
        output.canonical.source_id = project_file.string();
        output.canonical.schema_version = "ubridge-0.1/mpc-xpj-" + std::to_string(data.value("version", 0));
        output.canonical.revision = {1, 1, 0};
        output.canonical.metadata["source_format"] = "MPC XPJ";
        output.canonical.metadata["source_mode"] = "read-only working copy";
        output.canonical.metadata["mpc_schema_version"] = std::to_string(data.value("version", 0));
        output.tempo_bpm = data.value("masterTempo", 120.0);
        const auto project_data = project_file.parent_path() / (project_file.stem().string() + "_[ProjectData]");
        std::map<std::string, std::string> asset_by_file;
        if (data.contains("samples") && data["samples"].is_array()) {
            std::size_t index = 0;
            for (const auto& sample : data["samples"]) {
                const auto filename = sample_name(sample);
                core::AssetReference asset;
                asset.id = "asset-" + std::to_string(index++) + "-" + stable_id(filename);
                const auto path = project_data / std::filesystem::path(filename).filename();
                asset.source_path = path.string();
                asset.bytes = std::filesystem::exists(path) ? std::filesystem::file_size(path) : 0;
                asset.required = true;
                asset_by_file[std::filesystem::path(filename).filename().string()] = asset.id;
                output.canonical.assets.push_back(std::move(asset));
            }
        }
        if (data.contains("tracks") && data["tracks"].is_array()) {
            std::size_t track_index = 0;
            for (const auto& source : data["tracks"]) {
                session::Track track;
                track.id = "track-" + std::to_string(track_index++);
                track.name = source.value("name", "Unnamed track");
                const int type = source.contains("program") ? source["program"].value("type", -1) : -1;
                track.kind = type == 0 ? session::TrackKind::drum : type == 8 ? session::TrackKind::return_bus : session::TrackKind::unknown;
                output.tracks.push_back(track);
                session::MixerChannel mixer;
                mixer.id = "mixer-" + track.id;
                mixer.track_id = track.id;
                const double gain = source.value("volume", 1.0);
                mixer.volume_db = gain > 0.0 ? 20.0 * std::log10(gain) : -144.0;
                mixer.pan = source.value("pan", 0.5) * 2.0 - 1.0;
                mixer.muted = source.value("mute", false);
                output.mixer.push_back(std::move(mixer));
                if (type != 0 || !source.contains("program")) continue;
                session::InstrumentProgram program;
                program.id = "program-" + stable_id(source["program"].value("name", track.name));
                program.name = source["program"].value("name", track.name);
                const auto& drum = source["program"].value("drum", nlohmann::json::object());
                if (drum.contains("instruments") && drum["instruments"].is_array()) {
                    int pad_index = 0;
                    for (const auto& instrument : drum["instruments"]) {
                        if (!instrument.contains("layersv") || !instrument["layersv"].is_array()) { ++pad_index; continue; }
                        const auto layer = std::find_if(instrument["layersv"].begin(), instrument["layersv"].end(), [](const nlohmann::json& item) { return !item.value("sampleFile", "").empty(); });
                        if (layer == instrument["layersv"].end()) { ++pad_index; continue; }
                        const auto filename = std::filesystem::path(layer->value("sampleFile", "")).filename().string();
                        session::Pad pad;
                        pad.id = program.id + "-pad-" + std::to_string(pad_index);
                        pad.index = pad_index;
                        pad.midi_note = 36 + pad_index;
                        pad.program_id = program.id;
                        if (const auto found = asset_by_file.find(filename); found != asset_by_file.end()) pad.sample_asset_id = found->second;
                        pad.level = layer->contains("volume") ? (*layer)["volume"].value("gainCoefficient", 1.0) : 1.0;
                        pad.pan = layer->value("pan", 0.5) * 2.0 - 1.0;
                        output.pads.push_back(pad);
                        program.pad_ids.push_back(pad.id);
                        if (layer->contains("sliceInfo")) {
                            const auto& info = (*layer)["sliceInfo"];
                            const auto start = info.value("Start", 0LL);
                            const auto end = info.value("End", 0LL);
                            if (end > start) {
                                session::SampleSlice slice{pad.id + "-slice", pad.sample_asset_id, static_cast<std::uint64_t>(start), static_cast<std::uint64_t>(end), pad_index, layer->value("sampleName", filename)};
                                program.slice_ids.push_back(slice.id);
                                output.slices.push_back(std::move(slice));
                            }
                        }
                        ++pad_index;
                    }
                }
                output.programs.push_back(std::move(program));
            }
        }
        std::map<int, std::string> sequence_ids;
        if (data.contains("sequences") && data["sequences"].is_array()) {
            for (const auto& entry : data["sequences"]) {
                if (!entry.contains("value")) continue;
                const int key = entry.value("key", static_cast<int>(sequence_ids.size()));
                const auto& value = entry["value"];
                session::Sequence sequence;
                sequence.id = "sequence-" + std::to_string(key);
                sequence.name = value.value("name", sequence.id);
                sequence.length_ticks = value.value("lengthPulses", 0LL);
                for (const auto& track : output.tracks) sequence.track_ids.push_back(track.id);
                sequence_ids[key] = sequence.id;
                output.sequences.push_back(std::move(sequence));
            }
        }
        if (data.contains("songs") && data["songs"].is_array()) {
            std::size_t song_index = 0;
            for (const auto& source : data["songs"]) {
                if (!source.contains("items") || !source["items"].is_array() || source["items"].empty()) { ++song_index; continue; }
                session::Song song{"song-" + std::to_string(song_index), source.value("name", "Unnamed song"), {}};
                for (const auto& item : source["items"]) {
                    const int sequence_index = item.value("item.sequenceIndex", -1);
                    if (const auto found = sequence_ids.find(sequence_index); found != sequence_ids.end()) song.steps.push_back({found->second, item.value("item.repeats", 1)});
                }
                output.songs.push_back(std::move(song));
                ++song_index;
            }
        }
        imported.unmapped_field_groups = {"sequence event variants", "MPC plug-in state blobs", "automation and Q-Link assignments", "device synthesis parameters", "clip matrix state"};
        imported.hardware_branch = session::create_branch("hardware-import-" + stable_id(project_file.stem().string()), session::BranchOrigin::hardware, {0, 0, 0}, {});
        imported.diagnostics = session::validate(output);
        imported.diagnostics.push_back({core::DiagnosticSeverity::warning, "xpj_partial_canonical_translation", "Supported project structure was imported; device-specific and unvalidated fields remain explicitly unmapped.", "Do not write back or claim lossless DAW reconstruction until every required field group is qualified."});
        imported.valid = std::none_of(imported.diagnostics.begin(), imported.diagnostics.end(), [](const core::Diagnostic& item) { return item.severity == core::DiagnosticSeverity::error; });
    } catch (const std::exception& error) {
        imported.diagnostics.push_back({core::DiagnosticSeverity::error, "xpj_import_failed", error.what(), "Preserve the source and add a version-specific parser fixture."});
    }
    return imported;
}

std::string serialize_import_json(const XpjCanonicalImport& imported) {
    nlohmann::json root;
    const auto& value = imported.session;
    root["schema"] = value.canonical.schema_version;
    root["session_id"] = value.canonical.session_id;
    root["source_id"] = value.canonical.source_id;
    root["branch"] = {{"id", imported.hardware_branch.id}, {"origin", "hardware"}, {"immutable_source_snapshot", imported.hardware_branch.immutable_source_snapshot}};
    root["tempo_bpm"] = value.tempo_bpm;
    for (const auto& asset : value.canonical.assets) root["assets"].push_back({{"id", asset.id}, {"source_path", asset.source_path}, {"bytes", asset.bytes}, {"required", asset.required}});
    for (const auto& track : value.tracks) root["tracks"].push_back({{"id", track.id}, {"name", track.name}, {"kind", session::to_string(track.kind)}});
    for (const auto& pad : value.pads) root["pads"].push_back({{"id", pad.id}, {"index", pad.index}, {"midi_note", pad.midi_note}, {"program_id", pad.program_id}, {"asset_id", pad.sample_asset_id}, {"level", pad.level}, {"pan", pad.pan}});
    for (const auto& slice : value.slices) root["slices"].push_back({{"id", slice.id}, {"asset_id", slice.asset_id}, {"start_frame", slice.start_frame}, {"end_frame", slice.end_frame}, {"target_pad_index", slice.target_pad_index}});
    for (const auto& sequence : value.sequences) root["sequences"].push_back({{"id", sequence.id}, {"name", sequence.name}, {"length_ticks", sequence.length_ticks}});
    for (const auto& song : value.songs) { nlohmann::json output = {{"id", song.id}, {"name", song.name}}; for (const auto& step : song.steps) output["steps"].push_back({{"sequence_id", step.sequence_id}, {"repetitions", step.repetitions}}); root["songs"].push_back(std::move(output)); }
    root["unmapped_field_groups"] = imported.unmapped_field_groups;
    root["valid"] = imported.valid;
    root["write_back_enabled"] = false;
    return root.dump(2) + "\n";
}

} // namespace ubridge::mpc
