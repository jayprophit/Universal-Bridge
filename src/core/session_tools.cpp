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
    report_duplicate_ids(session.slices, "slice", diagnostics);
    report_duplicate_ids(session.programs, "program", diagnostics);
    report_duplicate_ids(session.sequences, "sequence", diagnostics);
    report_duplicate_ids(session.songs, "song", diagnostics);
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
    std::set<std::string> sequence_ids;
    for (const auto& sequence : session.sequences) {
        sequence_ids.insert(sequence.id);
        if (sequence.length_ticks <= 0) diagnostics.push_back({core::DiagnosticSeverity::warning, "nonpositive_sequence_length", "Sequence '" + sequence.id + "' has no positive duration.", "Preserve it as metadata but do not schedule it until its length is known."});
    }
    for (const auto& song : session.songs) {
        for (const auto& step : song.steps) {
            if (!sequence_ids.contains(step.sequence_id)) diagnostics.push_back({core::DiagnosticSeverity::error, "song_missing_sequence", "Song '" + song.id + "' refers to missing sequence '" + step.sequence_id + "'.", "Import or relink the sequence before reconstructing the song."});
            if (step.repetitions <= 0) diagnostics.push_back({core::DiagnosticSeverity::error, "invalid_song_repetitions", "Song '" + song.id + "' has a nonpositive repetition count.", "Set a positive count before DAW arrangement creation."});
        }
    }
    for (const auto& slice : session.slices) {
        if (slice.end_frame <= slice.start_frame) diagnostics.push_back({core::DiagnosticSeverity::error, "invalid_sample_slice", "Slice '" + slice.id + "' has an invalid frame range.", "Keep the source audio unchanged and repair the non-destructive slice markers."});
        if (slice.target_pad_index < 0) diagnostics.push_back({core::DiagnosticSeverity::warning, "slice_without_pad", "Slice '" + slice.id + "' is not assigned to a pad.", "Retain it as an unassigned chop until the user maps it."});
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

std::string to_string(BranchOrigin origin) {
    switch (origin) {
        case BranchOrigin::hardware: return "hardware";
        case BranchOrigin::daw: return "daw";
        case BranchOrigin::bridge: return "bridge";
    }
    return "bridge";
}

std::string to_string(MergeChoice choice) {
    switch (choice) {
        case MergeChoice::keep_hardware: return "keep_hardware";
        case MergeChoice::keep_daw: return "keep_daw";
        case MergeChoice::merge_value: return "merge_value";
        case MergeChoice::extract_section: return "extract_section";
    }
    return "merge_value";
}

SessionBranch create_branch(
    std::string id,
    BranchOrigin origin,
    core::RevisionVector base_revision,
    std::vector<core::Change> changes) {
    SessionBranch branch;
    branch.id = std::move(id);
    branch.origin = origin;
    branch.base_revision = base_revision;
    branch.head_revision = base_revision;
    branch.changes = std::move(changes);
    ++branch.head_revision.session;
    if (origin == BranchOrigin::hardware) ++branch.head_revision.hardware;
    if (origin == BranchOrigin::daw) ++branch.head_revision.daw;
    return branch;
}

SessionMergePlan plan_merge(
    std::string base_session_id,
    SessionBranch hardware,
    SessionBranch daw) {
    SessionMergePlan plan;
    plan.base_session_id = std::move(base_session_id);
    plan.hardware = std::move(hardware);
    plan.daw = std::move(daw);
    plan.conflicts = core::detect_conflicts(plan.hardware.changes, plan.daw.changes);
    const auto conflicts_with = [&plan](const core::Change& change) {
        return std::any_of(plan.conflicts.begin(), plan.conflicts.end(), [&change](const core::Conflict& conflict) {
            return conflict.hardware_change.entity_id == change.entity_id && conflict.hardware_change.field == change.field;
        });
    };
    for (const auto& change : plan.hardware.changes) if (!conflicts_with(change)) plan.automatic_changes.push_back(change);
    for (const auto& change : plan.daw.changes) {
        if (conflicts_with(change)) continue;
        const bool duplicate = std::any_of(plan.automatic_changes.begin(), plan.automatic_changes.end(), [&change](const core::Change& existing) {
            return existing.entity_id == change.entity_id && existing.field == change.field && existing.after == change.after;
        });
        if (!duplicate) plan.automatic_changes.push_back(change);
    }
    plan.ready_to_apply = plan.conflicts.empty();
    return plan;
}

bool resolve_conflict(SessionMergePlan& plan, MergeResolution resolution) {
    const auto conflict = std::find_if(plan.conflicts.begin(), plan.conflicts.end(), [&resolution](const core::Conflict& value) {
        return value.hardware_change.entity_id == resolution.entity_id && value.hardware_change.field == resolution.field;
    });
    if (conflict == plan.conflicts.end()) return false;
    if (resolution.choice == MergeChoice::keep_hardware) resolution.resolved_value = conflict->hardware_change.after;
    if (resolution.choice == MergeChoice::keep_daw) resolution.resolved_value = conflict->daw_change.after;
    if (resolution.resolved_value.empty()) return false;
    plan.resolutions.push_back(std::move(resolution));
    plan.ready_to_apply = plan.resolutions.size() == plan.conflicts.size();
    return true;
}

bool extract_section(SessionMergePlan& plan, SectionExtraction extraction) {
    if (extraction.destination_branch_id.empty() || extraction.entity_ids.empty()) return false;
    const bool valid_source = extraction.source_branch_id == plan.hardware.id || extraction.source_branch_id == plan.daw.id;
    if (!valid_source) return false;
    plan.extractions.push_back(std::move(extraction));
    return true;
}

std::string to_string(TransferDirection direction) {
    return direction == TransferDirection::hardware_to_daw ? "hardware_to_daw" : "daw_to_hardware";
}

PadAssignmentIntent plan_pad_assignment(
    std::string id,
    TransferDirection direction,
    std::string branch_id,
    std::string program_id,
    int pad_index,
    core::AssetReference asset,
    bool backup_verified,
    bool target_format_qualified) {
    PadAssignmentIntent intent;
    intent.id = std::move(id);
    intent.direction = direction;
    intent.branch_id = std::move(branch_id);
    intent.program_id = std::move(program_id);
    intent.pad_index = pad_index;
    intent.asset = std::move(asset);
    intent.backup_verified = backup_verified;
    intent.target_format_qualified = target_format_qualified;
    intent.ready_to_apply = !intent.id.empty() && !intent.branch_id.empty() && !intent.program_id.empty() &&
                            intent.pad_index >= 0 && !intent.asset.fingerprint.empty() &&
                            backup_verified && target_format_qualified;
    return intent;
}

} // namespace ubridge::session
