#include "ubridge/core/mpc_xpj_reader.hpp"

#include <nlohmann/json.hpp>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace ubridge::mpc {
namespace {

struct GzipReadResult {
    std::string payload;
    std::string error;
};

struct JsonMetrics {
    std::size_t nodes = 0;
    std::size_t string_bytes = 0;
    std::size_t maximum_depth = 0;
};

GzipReadResult read_gzip(const std::filesystem::path& path, const XpjParserLimits& limits) {
    GzipReadResult output;
    std::error_code size_error;
    const auto compressed_bytes = std::filesystem::file_size(path, size_error);
    if (size_error) {
        output.error = "xpj_file_size_unavailable";
        return output;
    }
    if (compressed_bytes > limits.maximum_compressed_bytes) {
        output.error = "xpj_compressed_limit_exceeded";
        return output;
    }
    gzFile file = gzopen(path.string().c_str(), "rb");
    if (!file) {
        output.error = "xpj_gzip_open_failed";
        return output;
    }
    std::array<char, 64 * 1024> buffer{};
    for (;;) {
        const int count = gzread(file, buffer.data(), static_cast<unsigned int>(buffer.size()));
        if (count < 0) {
            output.payload.clear();
            output.error = "xpj_gzip_stream_invalid";
            break;
        }
        if (count == 0) break;
        const auto bytes = static_cast<std::size_t>(count);
        if (bytes >
            limits.maximum_decompressed_bytes - (std::min)(output.payload.size(), limits.maximum_decompressed_bytes)) {
            output.payload.clear();
            output.error = "xpj_decompressed_limit_exceeded";
            break;
        }
        output.payload.append(buffer.data(), bytes);
    }
    gzclose(file);
    return output;
}

void measure_json(const nlohmann::json& value, const XpjParserLimits& limits, JsonMetrics& metrics, std::size_t depth) {
    ++metrics.nodes;
    metrics.maximum_depth = (std::max)(metrics.maximum_depth, depth);
    if (metrics.nodes > limits.maximum_json_nodes) throw std::runtime_error("xpj_json_node_limit_exceeded");
    if (depth > limits.maximum_json_depth) throw std::runtime_error("xpj_json_depth_limit_exceeded");
    if ((value.is_array() || value.is_object()) && value.size() > limits.maximum_container_elements) {
        throw std::runtime_error("xpj_json_container_limit_exceeded");
    }
    if (value.is_string()) {
        const auto bytes = value.get_ref<const std::string&>().size();
        if (bytes > limits.maximum_string_bytes - (std::min)(metrics.string_bytes, limits.maximum_string_bytes)) {
            throw std::runtime_error("xpj_json_string_limit_exceeded");
        }
        metrics.string_bytes += bytes;
        return;
    }
    if (value.is_object()) {
        for (auto iterator = value.begin(); iterator != value.end(); ++iterator) {
            if (iterator.key().size() >
                limits.maximum_string_bytes - (std::min)(metrics.string_bytes, limits.maximum_string_bytes)) {
                throw std::runtime_error("xpj_json_string_limit_exceeded");
            }
            metrics.string_bytes += iterator.key().size();
            measure_json(iterator.value(), limits, metrics, depth + 1);
        }
    } else if (value.is_array()) {
        for (const auto& item : value)
            measure_json(item, limits, metrics, depth + 1);
    }
}

nlohmann::json parse_json_payload(std::string_view payload, const XpjParserLimits& limits, JsonMetrics& metrics) {
    const auto start = payload.find('{');
    if (start == std::string_view::npos) throw std::runtime_error("xpj_json_object_missing");
    JsonMetrics streaming_metrics;
    const auto account_string = [&](std::size_t bytes) {
        if (bytes >
            limits.maximum_string_bytes - (std::min)(streaming_metrics.string_bytes, limits.maximum_string_bytes)) {
            throw std::runtime_error("xpj_json_string_limit_exceeded");
        }
        streaming_metrics.string_bytes += bytes;
    };
    const auto callback = [&](int depth, nlohmann::json::parse_event_t event, nlohmann::json& parsed) {
        const auto bounded_depth = depth < 0 ? 0U : static_cast<std::size_t>(depth) + 1U;
        streaming_metrics.maximum_depth = (std::max)(streaming_metrics.maximum_depth, bounded_depth);
        if (bounded_depth > limits.maximum_json_depth) {
            throw std::runtime_error("xpj_json_depth_limit_exceeded");
        }
        if (event == nlohmann::json::parse_event_t::object_start ||
            event == nlohmann::json::parse_event_t::array_start || event == nlohmann::json::parse_event_t::value) {
            ++streaming_metrics.nodes;
            if (streaming_metrics.nodes > limits.maximum_json_nodes) {
                throw std::runtime_error("xpj_json_node_limit_exceeded");
            }
        }
        if ((event == nlohmann::json::parse_event_t::key || event == nlohmann::json::parse_event_t::value) &&
            parsed.is_string()) {
            account_string(parsed.get_ref<const std::string&>().size());
        }
        return true;
    };
    auto root = nlohmann::json::parse(payload.begin() + static_cast<std::ptrdiff_t>(start), payload.end(), callback);
    metrics = {};
    measure_json(root, limits, metrics, 1);
    return root;
}

std::string sample_name(const nlohmann::json& value) {
    for (const auto* key : {"path", "fileName", "filename", "name"}) {
        if (value.contains(key) && value[key].is_string()) return value[key].get<std::string>();
    }
    return {};
}

std::string stable_id(std::string value) {
    for (auto& c : value)
        c = std::isalnum(static_cast<unsigned char>(c)) ? static_cast<char>(std::tolower(static_cast<unsigned char>(c)))
                                                        : '-';
    value.erase(std::unique(value.begin(), value.end(), [](char a, char b) { return a == '-' && b == '-'; }),
                value.end());
    while (!value.empty() && value.front() == '-')
        value.erase(value.begin());
    while (!value.empty() && value.back() == '-')
        value.pop_back();
    return value.empty() ? "unnamed" : value;
}

nlohmann::json parse_root(const std::filesystem::path& path, const XpjParserLimits& limits, JsonMetrics& metrics) {
    const auto read = read_gzip(path, limits);
    if (!read.error.empty()) throw std::runtime_error(read.error);
    return parse_json_payload(read.payload, limits, metrics);
}

} // namespace

XpjInspection inspect_xpj(const std::filesystem::path& project_file, XpjParserLimits limits) {
    XpjInspection result;
    std::ifstream header(project_file, std::ios::binary);
    unsigned char signature[2]{};
    if (!header.read(reinterpret_cast<char*>(signature), 2)) {
        result.diagnostics.push_back({core::DiagnosticSeverity::error, "xpj_unreadable", "The XPJ could not be read.",
                                      "Select a readable working copy; do not modify the device project."});
        return result;
    }
    result.readable = true;
    result.gzip_container = signature[0] == 0x1f && signature[1] == 0x8b;
    if (!result.gzip_container) {
        result.diagnostics.push_back({core::DiagnosticSeverity::error, "xpj_not_gzip",
                                      "The selected file does not use the observed XPJ gzip container.",
                                      "Keep it unchanged and use a separately qualified format adapter."});
        return result;
    }
    const auto read = read_gzip(project_file, limits);
    if (!read.error.empty()) {
        result.diagnostics.push_back({core::DiagnosticSeverity::error, read.error,
                                      "The XPJ payload was rejected by a bounded container or decompression check.",
                                      "Preserve the source and inspect it with a separately reviewed limit change."});
        return result;
    }
    const auto& payload = read.payload;
    result.decompressed_bytes = payload.size();
    const auto json_start = payload.find('{');
    if (json_start == std::string::npos) {
        result.diagnostics.push_back({core::DiagnosticSeverity::error, "xpj_json_object_missing",
                                      "The XPJ payload has no JSON object.",
                                      "Preserve the source and inspect it as an unsupported version."});
        return result;
    }
    std::istringstream prefix(payload.substr(0, json_start));
    for (std::string line; std::getline(prefix, line);)
        if (!line.empty()) result.preamble.push_back(line);
    try {
        JsonMetrics metrics;
        const auto root = parse_json_payload(payload, limits, metrics);
        result.json_nodes = metrics.nodes;
        result.maximum_depth = metrics.maximum_depth;
        if (!root.contains("data") || !root["data"].is_object())
            throw nlohmann::json::type_error::create(302, "missing data object", &root);
        const auto& data = root["data"];
        result.json_payload = true;
        result.schema_version = data.value("version", 0);
        result.master_tempo = data.value("masterTempo", 0.0);
        const auto count = [&data](const char* key) {
            return data.contains(key) && data[key].is_array() ? data[key].size() : 0U;
        };
        result.sample_count = count("samples");
        result.track_count = count("tracks");
        result.sequence_count = count("sequences");
        result.song_slot_count = count("songs");
        if (data.contains("samples") && data["samples"].is_array()) {
            for (const auto& sample : data["samples"])
                result.sample_names.push_back(sample_name(sample));
        }
        auto stem = project_file.stem().string();
        const auto project_data = project_file.parent_path() / (stem + "_[ProjectData]");
        for (const auto& name : result.sample_names) {
            if (!name.empty() && std::filesystem::exists(project_data / std::filesystem::path(name).filename()))
                ++result.available_asset_count;
            else
                ++result.missing_asset_count;
        }
        result.diagnostics.push_back(
            {core::DiagnosticSeverity::info, "xpj_read_only_observed",
             "The XPJ container and JSON metadata were parsed without changing the source.",
             "Translate fields into a new canonical branch; keep unknown fields preserved as unsupported metadata."});
        if (result.missing_asset_count)
            result.diagnostics.push_back({core::DiagnosticSeverity::warning, "xpj_assets_unresolved",
                                          "Some sample references were not resolved in the sibling ProjectData folder.",
                                          "Search approved sample roots read-only before offering a relink plan."});
    } catch (const std::exception& error) {
        const std::string detail = error.what();
        const auto code = detail.starts_with("xpj_") ? detail : "xpj_json_invalid";
        result.diagnostics.push_back({core::DiagnosticSeverity::error, code,
                                      std::string("The XPJ JSON payload was not accepted: ") + detail,
                                      "Preserve the source and add a version-specific parser fixture."});
    }
    return result;
}

XpjCanonicalImport import_xpj(const std::filesystem::path& project_file, XpjParserLimits limits) {
    XpjCanonicalImport imported;
    try {
        JsonMetrics metrics;
        const auto root = parse_root(project_file, limits, metrics);
        if (!root.contains("data") || !root["data"].is_object())
            throw std::runtime_error("missing project data object");
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
                track.kind = type == 0   ? session::TrackKind::drum
                             : type == 8 ? session::TrackKind::return_bus
                                         : session::TrackKind::unknown;
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
                        if (!instrument.contains("layersv") || !instrument["layersv"].is_array()) {
                            ++pad_index;
                            continue;
                        }
                        const auto layer = std::find_if(
                            instrument["layersv"].begin(), instrument["layersv"].end(),
                            [](const nlohmann::json& item) { return !item.value("sampleFile", "").empty(); });
                        if (layer == instrument["layersv"].end()) {
                            ++pad_index;
                            continue;
                        }
                        const auto filename = std::filesystem::path(layer->value("sampleFile", "")).filename().string();
                        session::Pad pad;
                        pad.id = program.id + "-pad-" + std::to_string(pad_index);
                        pad.index = pad_index;
                        pad.midi_note = 36 + pad_index;
                        pad.program_id = program.id;
                        if (const auto found = asset_by_file.find(filename); found != asset_by_file.end())
                            pad.sample_asset_id = found->second;
                        pad.level = layer->contains("volume") ? (*layer)["volume"].value("gainCoefficient", 1.0) : 1.0;
                        pad.pan = layer->value("pan", 0.5) * 2.0 - 1.0;
                        output.pads.push_back(pad);
                        program.pad_ids.push_back(pad.id);
                        if (layer->contains("sliceInfo")) {
                            const auto& info = (*layer)["sliceInfo"];
                            const auto start = info.value("Start", 0LL);
                            const auto end = info.value("End", 0LL);
                            if (end > start) {
                                session::SampleSlice slice{pad.id + "-slice",
                                                           pad.sample_asset_id,
                                                           static_cast<std::uint64_t>(start),
                                                           static_cast<std::uint64_t>(end),
                                                           pad_index,
                                                           layer->value("sampleName", filename)};
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
            for (const auto& sequence_entry : data["sequences"]) {
                if (!sequence_entry.contains("value")) continue;
                const int key = sequence_entry.value("key", static_cast<int>(sequence_ids.size()));
                const auto& value = sequence_entry["value"];
                session::Sequence sequence;
                sequence.id = "sequence-" + std::to_string(key);
                sequence.name = value.value("name", sequence.id);
                sequence.length_ticks = value.value("lengthPulses", 0LL);
                for (const auto& track : output.tracks)
                    sequence.track_ids.push_back(track.id);
                if (value.contains("trackClipMaps") && value["trackClipMaps"].is_array()) {
                    std::size_t event_index = 0;
                    for (const auto& clip_map : value["trackClipMaps"]) {
                        if (!clip_map.is_array()) continue;
                        for (const auto& clip_entry : clip_map) {
                            if (!clip_entry.contains("value")) continue;
                            const auto track_name = clip_entry.value("key", std::string{});
                            const auto track = std::find_if(
                                output.tracks.begin(), output.tracks.end(),
                                [&track_name](const session::Track& item) { return item.name == track_name; });
                            if (track == output.tracks.end()) continue;
                            const auto& clip_value = clip_entry["value"];
                            session::Clip clip;
                            clip.id = sequence.id + "-" + track->id + "-clip";
                            clip.track_id = track->id;
                            clip.start_tick = clip_value.value("startPulses", 0LL);
                            clip.length_ticks = clip_value.value("endPulses", 0LL) - clip.start_tick;
                            clip.sequence_id = sequence.id;
                            output.clips.push_back(clip);
                            if (!clip_value.contains("eventList") || !clip_value["eventList"].contains("events"))
                                continue;
                            for (const auto& event : clip_value["eventList"]["events"]) {
                                const int type = event.value("type", -1);
                                if (type == 3 && event.contains("note")) {
                                    const auto& note = event["note"];
                                    core::MusicalEvent translated;
                                    translated.id = sequence.id + "-event-" + std::to_string(event_index++);
                                    translated.track_id = track->id;
                                    translated.tick = event.value("time", 0LL);
                                    translated.duration_ticks = note.value("length", 0LL);
                                    translated.channel = event.value("channel", 0);
                                    translated.note = note.value("note", -1);
                                    translated.velocity = std::clamp(
                                        static_cast<int>(std::lround(note.value("velocity", 0.0) * 127.0)), 0, 127);
                                    output.canonical.midi_events.push_back(std::move(translated));
                                    ++imported.mapped_note_events;
                                } else if (type == 1 && event.contains("automation")) {
                                    ++imported.unmapped_automation_events;
                                }
                            }
                        }
                    }
                }
                sequence_ids[key] = sequence.id;
                output.sequences.push_back(std::move(sequence));
            }
        }
        if (data.contains("songs") && data["songs"].is_array()) {
            std::size_t song_index = 0;
            for (const auto& source : data["songs"]) {
                if (!source.contains("items") || !source["items"].is_array() || source["items"].empty()) {
                    ++song_index;
                    continue;
                }
                session::Song song{"song-" + std::to_string(song_index), source.value("name", "Unnamed song"), {}};
                for (const auto& item : source["items"]) {
                    const int sequence_index = item.value("item.sequenceIndex", -1);
                    if (const auto found = sequence_ids.find(sequence_index); found != sequence_ids.end())
                        song.steps.push_back({found->second, item.value("item.repeats", 1)});
                }
                output.songs.push_back(std::move(song));
                ++song_index;
            }
        }
        imported.unmapped_field_groups = {"note probability ratchet articulation and modifiers",
                                          "MPC plug-in state blobs", "automation and Q-Link parameter identities",
                                          "device synthesis parameters", "clip matrix state"};
        const auto opaque = root.dump();
        if (opaque.size() <= limits.maximum_opaque_retention_bytes) {
            imported.opaque_source_json = opaque;
            imported.opaque_source_retained = true;
        } else {
            imported.diagnostics.push_back(
                {core::DiagnosticSeverity::warning, "xpj_opaque_retention_limit_exceeded",
                 "The validated source JSON exceeds the bounded opaque-retention limit.",
                 "Keep the original read-only XPJ with the session and do not claim lossless reconstruction."});
        }
        imported.hardware_branch = session::create_branch("hardware-import-" + stable_id(project_file.stem().string()),
                                                          session::BranchOrigin::hardware, {0, 0, 0}, {});
        const auto validation = session::validate(output);
        imported.diagnostics.insert(imported.diagnostics.end(), validation.begin(), validation.end());
        imported.diagnostics.push_back(
            {core::DiagnosticSeverity::warning, "xpj_partial_canonical_translation",
             "Supported project structure was imported; device-specific and unvalidated fields remain explicitly "
             "unmapped.",
             "Do not write back or claim lossless DAW reconstruction until every required field group is qualified."});
        imported.valid =
            std::none_of(imported.diagnostics.begin(), imported.diagnostics.end(),
                         [](const core::Diagnostic& item) { return item.severity == core::DiagnosticSeverity::error; });
    } catch (const std::exception& error) {
        imported.diagnostics.push_back({core::DiagnosticSeverity::error, "xpj_import_failed", error.what(),
                                        "Preserve the source and add a version-specific parser fixture."});
    }
    return imported;
}

std::string serialize_import_json(const XpjCanonicalImport& imported) {
    nlohmann::json root;
    const auto& value = imported.session;
    root["schema"] = value.canonical.schema_version;
    root["session_id"] = value.canonical.session_id;
    root["source_id"] = value.canonical.source_id;
    root["branch"] = {{"id", imported.hardware_branch.id},
                      {"origin", "hardware"},
                      {"immutable_source_snapshot", imported.hardware_branch.immutable_source_snapshot}};
    root["tempo_bpm"] = value.tempo_bpm;
    root["event_translation"] = {{"mapped_note_events", imported.mapped_note_events},
                                 {"unmapped_automation_events", imported.unmapped_automation_events}};
    for (const auto& asset : value.canonical.assets)
        root["assets"].push_back({{"id", asset.id},
                                  {"source_path", asset.source_path},
                                  {"bytes", asset.bytes},
                                  {"required", asset.required}});
    for (const auto& track : value.tracks)
        root["tracks"].push_back({{"id", track.id}, {"name", track.name}, {"kind", session::to_string(track.kind)}});
    for (const auto& pad : value.pads)
        root["pads"].push_back({{"id", pad.id},
                                {"index", pad.index},
                                {"midi_note", pad.midi_note},
                                {"program_id", pad.program_id},
                                {"asset_id", pad.sample_asset_id},
                                {"level", pad.level},
                                {"pan", pad.pan}});
    for (const auto& slice : value.slices)
        root["slices"].push_back({{"id", slice.id},
                                  {"asset_id", slice.asset_id},
                                  {"start_frame", slice.start_frame},
                                  {"end_frame", slice.end_frame},
                                  {"target_pad_index", slice.target_pad_index}});
    for (const auto& sequence : value.sequences)
        root["sequences"].push_back(
            {{"id", sequence.id}, {"name", sequence.name}, {"length_ticks", sequence.length_ticks}});
    for (const auto& clip : value.clips)
        root["clips"].push_back({{"id", clip.id},
                                 {"track_id", clip.track_id},
                                 {"start_tick", clip.start_tick},
                                 {"length_ticks", clip.length_ticks},
                                 {"sequence_id", clip.sequence_id}});
    for (const auto& event : value.canonical.midi_events)
        root["midi_events"].push_back({{"id", event.id},
                                       {"track_id", event.track_id},
                                       {"tick", event.tick},
                                       {"duration_ticks", event.duration_ticks},
                                       {"channel", event.channel},
                                       {"note", event.note},
                                       {"velocity", event.velocity}});
    for (const auto& song : value.songs) {
        nlohmann::json output = {{"id", song.id}, {"name", song.name}};
        for (const auto& step : song.steps)
            output["steps"].push_back({{"sequence_id", step.sequence_id}, {"repetitions", step.repetitions}});
        root["songs"].push_back(std::move(output));
    }
    root["unmapped_field_groups"] = imported.unmapped_field_groups;
    root["opaque_source_retained"] = imported.opaque_source_retained;
    if (imported.opaque_source_retained)
        root["source_opaque_payload"] = nlohmann::json::parse(imported.opaque_source_json);
    root["valid"] = imported.valid;
    root["write_back_enabled"] = false;
    return root.dump(2) + "\n";
}

} // namespace ubridge::mpc
