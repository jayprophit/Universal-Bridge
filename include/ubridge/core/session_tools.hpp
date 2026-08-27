#pragma once

#include "ubridge/core/bridge_core.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ubridge::session {

enum class TrackKind {
    drum,
    instrument,
    audio,
    return_bus,
    master,
    unknown,
};

struct Pad {
    std::string id;
    int index = -1;
    int midi_note = -1;
    std::string program_id;
    std::string sample_asset_id;
    double level = 1.0;
    double pan = 0.0;
};

struct Track {
    std::string id;
    std::string name;
    TrackKind kind = TrackKind::unknown;
    int midi_channel = 0;
    std::vector<std::string> clip_ids;
    std::vector<std::string> pad_ids;
};

struct Clip {
    std::string id;
    std::string track_id;
    std::int64_t start_tick = 0;
    std::int64_t length_ticks = 0;
    std::string sequence_id;
};

struct ArrangementRegion {
    std::string id;
    std::string name;
    std::string sequence_id;
    std::int64_t start_tick = 0;
    std::int64_t length_ticks = 0;
};

struct MixerChannel {
    std::string id;
    std::string track_id;
    double volume_db = 0.0;
    double pan = 0.0;
    bool muted = false;
    bool solo = false;
    std::string output_bus_id;
};

struct FxIntent {
    std::string id;
    std::string channel_id;
    std::string semantic_type;
    bool enabled = true;
    bool wet_dry_supported = false;
    double wet = 1.0;
    std::vector<core::CanonicalParameter> parameters;
};

struct AutomationPoint {
    std::int64_t tick = 0;
    double value = 0.0;
};

struct AutomationCurve {
    std::string id;
    std::string target_parameter_id;
    std::vector<AutomationPoint> points;
    bool source_editable = true;
};

struct RoutingEdge {
    std::string from_id;
    std::string to_id;
    std::string signal_kind;
    double gain_db = 0.0;
};

struct FullSession {
    core::CanonicalSession canonical;
    double tempo_bpm = 120.0;
    int time_signature_numerator = 4;
    int time_signature_denominator = 4;
    std::vector<Track> tracks;
    std::vector<Pad> pads;
    std::vector<Clip> clips;
    std::vector<ArrangementRegion> arrangement;
    std::vector<MixerChannel> mixer;
    std::vector<FxIntent> effects;
    std::vector<AutomationCurve> automation;
    std::vector<RoutingEdge> routing;
};

struct DuplicateAssetGroup {
    std::string fingerprint;
    std::vector<std::string> asset_ids;
    std::uint64_t reclaimable_bytes = 0;
};

struct AssetHealthReport {
    std::vector<std::string> missing_required_asset_ids;
    std::vector<DuplicateAssetGroup> duplicate_groups;
    std::vector<std::string> unreferenced_asset_ids;
    std::uint64_t total_bytes = 0;
    std::uint64_t duplicate_bytes = 0;
    std::vector<core::Diagnostic> diagnostics;
};

struct ArchiveEntry {
    std::string asset_id;
    std::string source_path;
    std::string archive_path;
    std::string fingerprint;
    std::uint64_t bytes = 0;
    bool required = true;
};

struct PortableArchivePlan {
    std::string archive_id;
    std::string schema_version;
    std::string session_id;
    std::vector<ArchiveEntry> entries;
    std::vector<core::Diagnostic> diagnostics;
    bool ready_to_package = false;
};

[[nodiscard]] std::string to_string(TrackKind kind);
[[nodiscard]] std::vector<core::Diagnostic> validate(const FullSession& session);
[[nodiscard]] AssetHealthReport analyze_assets(const FullSession& session);
[[nodiscard]] PortableArchivePlan plan_archive(const FullSession& session, std::string archive_id);

} // namespace ubridge::session
