#include "ubridge/bridge_modules.hpp"
#include "ubridge/core/performance_tools.hpp"
#include "ubridge/core/session_tools.hpp"
#include "ubridge/platform/local_service.hpp"
#include "ubridge/reporting_tools.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "Virtual lab failure: " << message << '\n';
        std::exit(1);
    }
}

ubridge::core::ConnectionCapability storage_connection() {
    ubridge::core::ConnectionCapability connection;
    connection.transport_id = "project-folder";
    connection.storage = true;
    return connection;
}

void profile_registry_test() {
    const auto devices = ubridge::modules::builtin_device_profiles();
    const auto daws = ubridge::modules::builtin_daw_profiles();
    const auto platforms = ubridge::modules::builtin_platform_profiles();
    expect(devices.size() >= 7, "device catalog must contain MPC, SP, Elektron, Maschine, Circuit, and legacy scaffolds");
    expect(daws.size() >= 9, "DAW catalog must cover documented desktop and mobile host targets");
    expect(platforms.size() == 7, "platform catalog must hard-code all seven requested operating-system targets");
    expect(ubridge::modules::find_device_profile("akai.mpc-sample").has_value(), "MPC Sample profile must resolve");
    expect(ubridge::modules::find_daw_profile("cubase").has_value(), "Cubase profile must resolve");
    expect(ubridge::modules::find_daw_profile("reason").has_value(), "Reason profile must resolve");
    expect(ubridge::modules::find_platform_profile("ipados").has_value(), "iPadOS profile must resolve");
}

void negotiation_and_workflow_test() {
    const auto device = *ubridge::modules::find_device_profile("akai.mpc-sample");
    const auto platform = *ubridge::modules::find_platform_profile("windows");
    const auto daw = *ubridge::modules::find_daw_profile("cubase");
    const auto connection = storage_connection();

    const auto integration = ubridge::core::negotiate(device.capability, platform.capability, daw.capability, connection);
    expect(integration.safe_preflight, "Windows storage route should permit read-only preflight");
    expect(integration.asset_exchange, "Cubase profile should accept audio exchange package");
    expect(integration.midi_exchange, "Cubase profile should accept MIDI exchange package");
    expect(!integration.direct_daw_creation, "unqualified direct DAW creation must remain disabled");
    expect(!integration.hardware_control, "storage-only route must not enable hardware control");

    const auto workflow = ubridge::modules::plan_finish_in_daw(device, platform, daw, connection);
    expect(workflow.steps.size() == 10, "Finish-in-DAW workflow must contain the complete safe lifecycle");
    expect(workflow.steps.at(1).enabled, "preflight should be enabled for a storage intake route");
    expect(!workflow.steps.at(4).enabled, "proprietary parser must remain disabled before qualification");
    expect(workflow.steps.at(5).enabled, "asset exchange should be enabled for the reference workflow");
}

void mobile_and_audio_safety_test() {
    const auto device = *ubridge::modules::find_device_profile("akai.mpc-sample");
    const auto android = *ubridge::modules::find_platform_profile("android");
    const auto daw = *ubridge::modules::find_daw_profile("mobile-generic");

    ubridge::core::ConnectionCapability usb;
    usb.transport_id = "usb-c";
    usb.usb_midi = true;
    usb.usb_audio = true;

    const auto integration = ubridge::core::negotiate(device.capability, android.capability, daw.capability, usb);
    expect(!integration.hardware_control, "unqualified Android route must not activate hardware control");
    expect(integration.mobile_companion, "Android profile must retain the companion pathway");

    const auto capture = ubridge::modules::plan_audio_capture(device, android, usb);
    expect(!capture.allowed, "audio capture must stay disabled before mobile backend qualification");
    expect(!capture.limitations.empty(), "disabled capture must report a limitation");

    const auto midi = ubridge::modules::plan_midi_route(device, android, usb);
    expect(!midi.allowed, "live MIDI must stay disabled before virtual MIDI backend qualification");
}

void conflict_and_transaction_test() {
    using namespace ubridge::core;
    const Change hardware {"pad-A01", "mixer.volume", "0.5", "0.7", SyncPolicy::bidirectional};
    const Change daw {"pad-A01", "mixer.volume", "0.5", "0.9", SyncPolicy::bidirectional};
    const auto conflicts = detect_conflicts({hardware}, {daw});
    expect(conflicts.size() == 1, "divergent edits to the same field must create a conflict");
    expect(conflicts.front().resolution_hint == "require_user_review", "bidirectional collision must not silently choose a winner");

    const Change hardware_authoritative {"pad-A02", "mixer.pan", "0.0", "-0.5", SyncPolicy::hardware_authoritative};
    const auto resolved = detect_conflicts({hardware_authoritative}, {daw});
    expect(resolved.empty(), "unrelated fields must not conflict");

    TransactionJournal journal;
    const auto transaction = journal.begin("transaction-001", "Fixture bridge workflow", {1, 2, 3}, {"backup/fixture"});
    expect(transaction.phase == TransactionPhase::planned, "new transaction must start planned");
    expect(journal.transition("transaction-001", TransactionPhase::awaiting_approval), "planned transaction must await approval");
    expect(journal.transition("transaction-001", TransactionPhase::running), "approved transaction must run");
    expect(journal.transition("transaction-001", TransactionPhase::verified), "running transaction must verify");
    expect(journal.transition("transaction-001", TransactionPhase::committed), "verified transaction must commit");
    expect(!journal.transition("transaction-001", TransactionPhase::running), "committed transaction must be immutable");
}

void local_service_test() {
    const auto device = *ubridge::modules::find_device_profile("akai.mpc-sample");
    const auto platform = *ubridge::modules::find_platform_profile("windows");
    const auto daw = *ubridge::modules::find_daw_profile("reason");
    const auto workflow = ubridge::modules::plan_finish_in_daw(device, platform, daw, storage_connection());

    ubridge::platform::LocalBridgeService service(platform.capability);
    const auto status = service.start();
    expect(status.state == ubridge::platform::ServiceState::ready_without_hardware, "Windows reference service must start in safe no-hardware mode");
    expect(!service.supports_hardware_backend(), "hardware backend must remain unavailable until native backend implementation and qualification");
    const auto transaction = service.begin_transaction(workflow.integration, "service-fixture", {"backup/fixture"});
    expect(transaction.has_value(), "safe preflight plan must create an approval-gated transaction");
    expect(transaction->phase == ubridge::core::TransactionPhase::awaiting_approval, "service transaction must not execute before approval");
    expect(service.stop().state == ubridge::platform::ServiceState::stopped, "service must stop cleanly");
}

ubridge::session::FullSession complete_session_fixture(bool include_missing_asset) {
    using namespace ubridge;
    session::FullSession session;
    session.canonical.session_id = "fixture-session";
    session.canonical.source_id = "fixture-mpc";
    session.canonical.schema_version = "0.4.0";
    session.canonical.revision = {3, 2, 1};
    session.canonical.assets = {
        {"asset-kick", "samples/kick.wav", "sha256:kicksnare", 1000, true},
        {"asset-snare", "samples/snare.wav", "sha256:kicksnare", 1000, true},
        {"asset-unused", "samples/unused.wav", "sha256:unused", 400, false}
    };
    if (include_missing_asset) {
        session.canonical.assets.push_back({"asset-missing", "", "", 500, true});
    }
    session.tracks = {{"track-drums", "Drums", session::TrackKind::drum, 9, {"clip-intro"}, {"pad-kick", "pad-snare"}}};
    session.pads = {
        {"pad-kick", 0, 36, "program-a", "asset-kick", 1.0, 0.0},
        {"pad-snare", 1, 38, "program-a", "asset-snare", 1.0, 0.0}
    };
    session.clips = {{"clip-intro", "track-drums", 0, 3840, "sequence-intro"}};
    session.arrangement = {{"region-intro", "Intro", "sequence-intro", 0, 3840}};
    session.mixer = {
        {"channel-drums", "track-drums", -3.0, 0.0, false, false, "channel-master"},
        {"channel-master", "track-master", 0.0, 0.0, false, false, ""}
    };
    session.routing = {{"channel-drums", "channel-master", "audio", 0.0}};
    session.automation = {{"curve-volume", "mixer.volume", {{0, 0.2}, {960, 0.8}}, true}};
    return session;
}

void session_asset_and_archive_test() {
    const auto session_with_missing = complete_session_fixture(true);
    const auto session_diagnostics = ubridge::session::validate(session_with_missing);
    expect(session_diagnostics.empty(), "complete session fixture should have structurally valid tracks, pads, clips, automation, and routing");

    const auto health = ubridge::session::analyze_assets(session_with_missing);
    expect(health.missing_required_asset_ids.size() == 1, "missing required asset must be detected");
    expect(health.duplicate_groups.size() == 1, "same-fingerprint assets must be grouped as duplicates");
    expect(health.duplicate_bytes == 1000, "duplicate analysis must calculate reclaimable bytes conservatively");
    expect(health.unreferenced_asset_ids.size() >= 2, "unused and missing fixture assets should be advisory unreferenced items");

    const auto incomplete_archive = ubridge::session::plan_archive(session_with_missing, "fixture-archive");
    expect(!incomplete_archive.ready_to_package, "archive with missing required asset must remain gated");

    const auto complete = complete_session_fixture(false);
    const auto archive = ubridge::session::plan_archive(complete, "fixture-archive");
    expect(archive.ready_to_package, "archive with complete required assets must be package-ready");
    expect(archive.entries.size() == 3, "archive manifest must retain all assets");
}

void performance_and_routing_test() {
    using namespace ubridge;
    const auto timing = performance::measure_timing("fixture-loopback", 48000.0, 0, 576, 480);
    expect(timing.valid, "timing calculation must accept valid marker positions");
    expect(timing.correction_samples == 96.0, "timing calculation must record sample correction");

    const auto drift = performance::analyze_drift(48000.0, {{0, 0, 0.0}, {480000, 480120, 10.0}}, 100.0);
    expect(drift.correction_recommended, "large fixture drift must request reviewable correction");

    const performance::StemValidationPolicy policy;
    const auto valid_stem = performance::validate_stem({"stem-kick", 0, 2, 48000, 48008, 2400, -6.0, -18.0, true}, policy);
    expect(valid_stem.acceptable, "valid completed stem within policy should pass");
    const auto silent_stem = performance::validate_stem({"stem-silent", 0, 0, 48000, 48000, 100, -100.0, -120.0, true}, policy);
    expect(!silent_stem.acceptable, "silent stem must not pass validation");

    const std::vector<performance::MidiEndpoint> endpoints = {{"device-out", false, true, false}, {"daw-in", true, false, true}};
    const auto valid_routes = performance::validate_midi_routes(endpoints, {{"route-main", "device-out", "daw-in", -1, false}});
    expect(valid_routes.valid, "valid MIDI output-to-input route must pass");
    const auto loop_routes = performance::validate_midi_routes({{"loop", true, true, true}}, {{"route-loop", "loop", "loop", -1, false}});
    expect(!loop_routes.valid, "feedback loop route must be rejected");

    const std::vector<core::MusicalEvent> events = {{"event-1", "track-drums", 0, 120, 9, 36, 100, std::nullopt, std::nullopt}};
    expect(performance::validate_midi_events(events).empty(), "valid MIDI performance event must pass validation");
    expect(performance::scale_normalized(0.5, {"filter.cutoff", 20.0, 20000.0, performance::ScaleCurve::logarithmic}) > 600.0, "logarithmic scaling must map normalized controller values non-linearly");

    const auto automation = performance::plan_automation_translation({"curve", "mixer.volume", {{0, 0.0}}, true}, "", false);
    expect(automation.render_fallback_required, "unsupported automation target must require an explicit fallback");
    const auto mixer = performance::plan_mixer_rebuild(complete_session_fixture(false));
    expect(mixer.structurally_complete, "complete mixer graph should be structurally ready for a qualified host adapter");
}

void reporting_and_profile_test() {
    using namespace ubridge;
    const auto platform = *modules::find_platform_profile("windows");
    const reporting::ProfileDocument unsigned_physical {
        "community.unsafe", "controller", "0.4.0", "0.1.0", "fixture", false, {{"hardware_writeback", true, true}}
    };
    expect(!reporting::validate_profile(unsigned_physical, platform.capability).valid, "unsigned physical control claim must fail profile validation");
    const reporting::ProfileDocument safe_profile {
        "community.safe", "controller", "0.4.0", "0.1.0", "fixture", true, {{"pad_note_map", true, false}}
    };
    expect(reporting::validate_profile(safe_profile, platform.capability).valid, "signed non-physical profile declaration must validate");

    const auto device = *modules::find_device_profile("akai.mpc-sample");
    const auto daw = *modules::find_daw_profile("cubase");
    const auto workflow = modules::plan_finish_in_daw(device, platform, daw, storage_connection());
    const auto report = reporting::build_compatibility_report("report-fixture", workflow.integration, {});
    expect(report.score_percent == 50, "reference exchange route should score only its actually available capabilities");
    const auto workflow_json = reporting::serialize_workflow_json(workflow);
    expect(workflow_json.find("\"workflow_id\": \"finish-in-cubase\"") != std::string::npos, "workflow serializer must preserve workflow identity");
    const auto archive_json = reporting::serialize_archive_plan_json(session::plan_archive(complete_session_fixture(false), "fixture-archive"));
    expect(archive_json.find("\"ready_to_package\": true") != std::string::npos, "archive serializer must preserve readiness state");
    const auto report_json = reporting::serialize_compatibility_report_json(report);
    expect(report_json.find("\"score_percent\": 50") != std::string::npos, "compatibility serializer must preserve score");
}

void virtual_device_test() {
    const auto device = *ubridge::modules::find_device_profile("akai.mpc-sample");
    ubridge::modules::VirtualDevice virtual_mpc("fixture-mpc-sample", device.capability);
    virtual_mpc.enqueue({"midi_note", "note=36,velocity=100", 960});
    expect(virtual_mpc.next_event().has_value(), "connected virtual device must emit queued event");
    virtual_mpc.enqueue({"transport", "stop", 1920});
    virtual_mpc.disconnect();
    expect(virtual_mpc.disconnected(), "disconnect state must be observable");
    expect(!virtual_mpc.next_event().has_value(), "disconnected virtual device must not emit events");
    virtual_mpc.reconnect();
    expect(virtual_mpc.next_event().has_value(), "queued events must remain available after reconnect");
}

} // namespace

int main() {
    profile_registry_test();
    negotiation_and_workflow_test();
    mobile_and_audio_safety_test();
    conflict_and_transaction_test();
    local_service_test();
    session_asset_and_archive_test();
    performance_and_routing_test();
    reporting_and_profile_test();
    virtual_device_test();
    std::cout << "Universal Bridge virtual hardware laboratory passed.\n";
    return 0;
}
