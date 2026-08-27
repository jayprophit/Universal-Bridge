#include "ubridge/core/session_tools.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

namespace ubridge::session {
namespace {

template <typename T>
void report_duplicate_ids(
    const std::vector<T>& values,
    const std::string& label,
    std::vector<core::Diagnostic>& diagnostics) {
    std::set<std::string> ids;
    for (const auto& value : values) {
        if (value.id.empty()) {
            diagnostics.push_back({core::DiagnosticSeverity::error, "empty_" + label + "_id", "A " + label + " has no stable identifier.", "Assign a deterministic identifier before synchronization or archive creation."});
        } else if (!ids.insert(value.id).second) {
            diagnostics.push_back({core::DiagnosticSeverity::error, "duplicate_" + label + "_id", "The " + label + " identifier '" + value.id + "' occurs more than once.", "Resolve duplicate identities before synchronization or archive creation."});
        }
    }
}

} // namespace

std::string to_string(TrackKind kind) {
    switch (kind) {
        case TrackKind::drum: return "drum";
        case TrackKind::instrument: return "instrument";
        case TrackKind::audio: return "audio";
        case TrackKind::return_bus: return "return_bus";
        case TrackKind::master: return "master";
        case TrackKind::unknown: return "unknown";
    }
    return "unknown";
}

std::vector<core::Diagnostic> validate(const FullSession& session) {
    std::vector<core::Diagnostic> diagnostics;
    if (session.canonical.session_id.empty()) {
        diagnostics.push_back({core::DiagnosticSeverity::error, "empty_session_id", "The canonical session has no stable identity.", "Assign a stable session ID before exporting, matching, or synchronizing."});
    }
    if (session.canonical.schema_version.empty()) {
        diagnostics.push_back({core::DiagnosticSeverity::error, "empty_schema_version", "The canonical session does not identify its schema version.", "Assign a schema version to enable safe migrations."});
    }
    if (session.tempo_bpm <= 0.0 || session.tempo_bpm > 999.0) {
        diagnostics.push_back({core::DiagnosticSeverity::error, "invalid_tempo", "Tempo must be greater than zero and within the supported planning range.", "Correct the tempo before creating timing or arrangement plans."});
    }
    if (session.time_signature_numerator <= 0 || session.time_signature_denominator <= 0) {
        diagnostics.push_back({core::DiagnosticSeverity::error, "invalid_time_signature", "The session time signature is invalid.", "Provide a positive numerator and denominator."});
    }

    report_duplicate_ids(session.tracks, "track", diagnostics);
    report_duplicate_ids(session.pads, "pad", diagnostics);
    report_duplicate_ids(session.clips, "clip", diagnostics);
    report_duplicate_ids(session.arrangement, "arrangement_region", diagnostics);
    report_duplicate_ids(session.mixer, "mixer_channel", diagnostics);
    report_duplicate_ids(session.effects, "effect", diagnostics);
    report_duplicate_ids(session.automation, "automation_curve", diagnostics);

    std::set<std::string> track_ids;
    for (const auto& track : session.tracks) {
        track_ids.insert(track.id);
    }
    for (const auto& clip : session.clips) {
        if (!track_ids.contains(clip.track_id)) {
            diagnostics.push_back({core::DiagnosticSeverity::error, "orphan_clip", "Clip '" + clip.id + "' refers to missing track '" + clip.track_id + "'.", "Create the missing track or repair the clip reference."});
        }
        if (clip.length_ticks <= 0) {
            diagnostics.push_back({core::DiagnosticSeverity::warning, "nonpositive_clip_length", "Clip '" + clip.id + "' has no positive duration.", "Review whether this represents an intentional marker or a damaged sequence."});
        }
    }

    std::set<int> pad_indices;
    for (const auto& pad : session.pads) {
        if (pad.index < 0) {
            diagnostics.push_back({core::DiagnosticSeverity::warning, "unknown_pad_index", "Pad '" + pad.id + "' has no physical/canonical index.", "Preserve the pad name but map an index only when its device profile documents one."});
        } else if (!pad_indices.insert(pad.index).second) {
            diagnostics.push_back({core::DiagnosticSeverity::warning, "duplicate_pad_index", "More than one pad uses index " + std::to_string(pad.index) + ".", "Resolve the pad-map ambiguity before controller routing."});
        }
    }

    for (const auto& edge : session.routing) {
        if (edge.from_id.empty() || edge.to_id.empty()) {
            diagnostics.push_back({core::DiagnosticSeverity::error, "incomplete_routing_edge", "A routing edge lacks a source or destination.", "Repair the routing graph before mixer reconstruction."});
        }
        if (edge.from_id == edge.to_id) {
            diagnostics.push_back({core::DiagnosticSeverity::warning, "self_routing_edge", "A routing edge routes '" + edge.from_id + "' to itself.", "Verify that this is an intentional feedback path; otherwise remove it."});
        }
    }

    for (const auto& curve : session.automation) {
        if (curve.target_parameter_id.empty()) {
            diagnostics.push_back({core::DiagnosticSeverity::error, "automation_without_target", "Automation curve '" + curve.id + "' has no target parameter.", "Retain as metadata or map it to a canonical parameter before export."});
        }
        if (!std::is_sorted(curve.points.begin(), curve.points.end(), [](const AutomationPoint& left, const AutomationPoint& right) {
                return left.tick < right.tick;
            })) {
            diagnostics.push_back({core::DiagnosticSeverity::warning, "unsorted_automation", "Automation curve '" + curve.id + "' has unsorted points.", "Sort the points by tick before interpolation or export."});
        }
    }
    return diagnostics;
}

AssetHealthReport analyze_assets(const FullSession& session) {
    AssetHealthReport report;
    std::map<std::string, std::vector<const core::AssetReference*>> fingerprints;
    std::set<std::string> referenced;
    std::set<std::string> asset_ids;

    for (const auto& pad : session.pads) {
        if (!pad.sample_asset_id.empty()) {
            referenced.insert(pad.sample_asset_id);
        }
    }
    for (const auto& asset : session.canonical.assets) {
        asset_ids.insert(asset.id);
        report.total_bytes += asset.bytes;
        if (asset.required && (asset.source_path.empty() || asset.fingerprint.empty())) {
            report.missing_required_asset_ids.push_back(asset.id);
        }
        if (!asset.fingerprint.empty()) {
            fingerprints[asset.fingerprint].push_back(&asset);
        }
        if (!referenced.contains(asset.id)) {
            report.unreferenced_asset_ids.push_back(asset.id);
        }
    }

    for (const auto& [fingerprint, group] : fingerprints) {
        if (group.size() < 2) {
            continue;
        }
        DuplicateAssetGroup duplicate;
        duplicate.fingerprint = fingerprint;
        std::uint64_t retained_bytes = 0;
        for (const auto* asset : group) {
            duplicate.asset_ids.push_back(asset->id);
            retained_bytes = std::max(retained_bytes, asset->bytes);
        }
        for (const auto* asset : group) {
            duplicate.reclaimable_bytes += asset->bytes;
        }
        duplicate.reclaimable_bytes -= retained_bytes;
        report.duplicate_bytes += duplicate.reclaimable_bytes;
        report.duplicate_groups.push_back(std::move(duplicate));
    }

    for (const auto& required_id : report.missing_required_asset_ids) {
        report.diagnostics.push_back({core::DiagnosticSeverity::warning, "missing_required_asset", "Required asset '" + required_id + "' has no usable source path or fingerprint.", "Locate the asset, verify its content hash, and approve a relink plan before export."});
    }
    for (const auto& duplicate : report.duplicate_groups) {
        report.diagnostics.push_back({core::DiagnosticSeverity::info, "duplicate_asset_content", "Assets share fingerprint '" + duplicate.fingerprint + "'.", "Review duplicates; do not delete source assets automatically."});
    }
    for (const auto& unused_id : report.unreferenced_asset_ids) {
        report.diagnostics.push_back({core::DiagnosticSeverity::info, "possibly_unused_asset", "Asset '" + unused_id + "' has no pad reference in the canonical session.", "Treat this as an advisory result until the complete project parser verifies all dependencies."});
    }
    return report;
}

PortableArchivePlan plan_archive(const FullSession& session, std::string archive_id) {
    PortableArchivePlan plan;
    plan.archive_id = std::move(archive_id);
    plan.schema_version = session.canonical.schema_version;
    plan.session_id = session.canonical.session_id;

    const auto health = analyze_assets(session);
    for (const auto& asset : session.canonical.assets) {
        ArchiveEntry entry;
        entry.asset_id = asset.id;
        entry.source_path = asset.source_path;
        entry.archive_path = "assets/" + asset.id;
        entry.fingerprint = asset.fingerprint;
        entry.bytes = asset.bytes;
        entry.required = asset.required;
        plan.entries.push_back(std::move(entry));
    }
    plan.diagnostics = health.diagnostics;
    plan.ready_to_package = health.missing_required_asset_ids.empty() && !session.canonical.session_id.empty() && !session.canonical.schema_version.empty();
    if (!plan.ready_to_package) {
        plan.diagnostics.push_back({core::DiagnosticSeverity::warning, "archive_not_ready", "The archive manifest is complete but packaging remains disabled due to missing identity or required assets.", "Resolve the reported session/asset issues before creating a portable archive."});
    } else {
        plan.diagnostics.push_back({core::DiagnosticSeverity::info, "archive_manifest_ready", "The portable archive manifest is ready for a separate, user-approved packaging step.", "Package only into a new destination; do not modify the source project folder."});
    }
    return plan;
}

} // namespace ubridge::session
