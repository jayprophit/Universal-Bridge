#include "ubridge/bridge_modules.hpp"
#include "ubridge/core/performance_tools.hpp"
#include "ubridge/core/mpc_xpj_reader.hpp"
#include "ubridge/core/session_tools.hpp"
#include "ubridge/core/state_sync.hpp"
#include "ubridge/core/sync_guard.hpp"
#include "ubridge/platform/hardware_backends.hpp"
#include "ubridge/platform/local_service.hpp"
#include "ubridge/reporting_tools.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <zlib.h>

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
    expect(devices.size() >= 12, "device catalog must contain instruments, interfaces, controllers, and mixing-desk profiles");
    expect(daws.size() >= 9, "DAW catalog must cover documented desktop and mobile host targets");
    expect(platforms.size() == 7, "platform catalog must hard-code all seven requested operating-system targets");
    expect(ubridge::modules::find_device_profile("akai.mpc-sample").has_value(), "MPC Sample profile must resolve");
    expect(ubridge::modules::find_device_profile("akai.mpc-one-plus").has_value(), "MPC One+ official companion route profile must resolve independently");
    expect(ubridge::modules::find_device_profile("mixing-desk.digital-generic").has_value(), "generic digital mixing desk profile must resolve");
    expect(ubridge::modules::find_device_profile("soundcraft.spirit-digital-328").has_value(), "Soundcraft Spirit Digital 328 profile must resolve");
    expect(ubridge::modules::find_daw_profile("cubase").has_value(), "Cubase profile must resolve");
    expect(ubridge::modules::find_daw_profile("mpc-beats").has_value(), "MPC Beats official-assisted host profile must resolve");
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
    expect(!integration.mobile_bridge, "a declared companion route must not be mistaken for a qualified mobile bridge host");

    const auto capture = ubridge::modules::plan_audio_capture(device, android, usb);
    expect(!capture.allowed, "audio capture must stay disabled before mobile backend qualification");
    expect(!capture.limitations.empty(), "disabled capture must report a limitation");

    const auto midi = ubridge::modules::plan_midi_route(device, android, usb);
    expect(!midi.allowed, "live MIDI must stay disabled before virtual MIDI backend qualification");
}

void xpj_reader_test() {
    const auto root = std::filesystem::temp_directory_path() / "ubridge-xpj-reader-test";
    std::filesystem::create_directories(root / "Fixture_[ProjectData]");
    std::ofstream(root / "Fixture_[ProjectData]" / "Kick.wav", std::ios::binary).put('\0');
    const std::string payload = "ACVS\n1.3.0.12\nSerialisableProjectData\njson\nLinux\n" R"({"data":{"version":28,"masterTempo":96.5,"samples":[{"path":"Kick.wav"}],"tracks":[{"name":"Drum","volume":1.0,"pan":0.5,"mute":false,"program":{"type":-1}}],"sequences":[{"key":0,"value":{"name":"Sequence 01","lengthPulses":3840,"trackClipMaps":[[{"key":"Drum","value":{"startPulses":0,"endPulses":3840,"eventList":{"events":[{"type":3,"time":120,"channel":0,"note":{"note":36,"velocity":0.5,"length":240}},{"type":1,"time":0,"automation":{"parameter":131,"value":0.0}}]}}}]]}}],"songs":[{"name":"Song","items":[{"item.sequenceIndex":0,"item.repeats":2}]}]}})";
    const auto xpj = root / "Fixture.xpj";
    gzFile file = gzopen(xpj.string().c_str(), "wb");
    expect(file != nullptr, "test XPJ gzip fixture must open");
    expect(gzwrite(file, payload.data(), static_cast<unsigned int>(payload.size())) == static_cast<int>(payload.size()), "test XPJ payload must write");
    gzclose(file);
    const auto report = ubridge::mpc::inspect_xpj(xpj);
    expect(report.json_payload && report.schema_version == 28, "XPJ reader must decode gzip preamble and JSON schema");
    expect(report.master_tempo == 96.5 && report.sample_count == 1 && report.track_count == 1, "XPJ reader must report core project counts");
    expect(report.sequence_count == 1 && report.song_slot_count == 1 && report.available_asset_count == 1, "XPJ reader must resolve sibling project assets");
    const auto imported = ubridge::mpc::import_xpj(xpj);
    expect(imported.valid && imported.hardware_branch.immutable_source_snapshot, "XPJ import must create a valid immutable hardware branch");
    expect(imported.session.canonical.assets.size() == 1 && imported.session.tracks.size() == 1, "XPJ import must translate evidenced assets and tracks");
    expect(imported.mapped_note_events == 1 && imported.unmapped_automation_events == 1, "XPJ import must map note events and count unqualified automation separately");
    expect(imported.session.canonical.midi_events.front().note == 36 && imported.session.canonical.midi_events.front().velocity == 64, "XPJ note translation must preserve pitch and normalized velocity");
    expect(imported.session.sequences.size() == 1 && imported.session.songs.front().steps.front().repetitions == 2, "XPJ import must preserve sequence and song repetition structure");
    expect(!imported.unmapped_field_groups.empty() && !ubridge::mpc::serialize_import_json(imported).empty(), "XPJ import must report unmapped data and serialize its canonical result");
    std::filesystem::remove_all(root);
}

void protocol_evidence_and_host_negotiation_test() {
    using namespace ubridge;
    auto device = *modules::find_device_profile("akai.mpc-sample");
    auto platform_profile = *modules::find_platform_profile("android");
    auto daw = *modules::find_daw_profile("mobile-generic");

    platform_profile.capability.runtime_qualified = true;
    platform_profile.capability.local_file_access = true;
    platform_profile.capability.usb_device_access = true;
    platform_profile.capability.audio_backend = true;
    platform_profile.capability.virtual_midi = true;
    platform_profile.capability.direct_mobile_host_route = true;
    device.capability.parameter_read = true;
    device.capability.parameter_write = true;
    daw.capability.parameter_feedback = true;

    core::ConnectionCapability connection;
    connection.transport_id = "usb-c-fixture";
    connection.protocol_evidence = {
        {core::ProtocolKind::usb_midi, core::EvidenceLevel::observed, true, true, 0, 0, "midi-fixture", "fixture endpoint observation"},
        {core::ProtocolKind::usb_audio, core::EvidenceLevel::observed, true, false, 2, 0, "audio-fixture", "fixture endpoint observation"}
    };

    const auto observed = core::negotiate(device.capability, platform_profile.capability, daw.capability, connection);
    expect(!observed.hardware_control, "observed MIDI endpoint must not activate hardware control before qualification");
    expect(!observed.audio_capture, "observed audio endpoint must not activate capture before qualification");
    expect(!observed.mobile_bridge, "unqualified endpoint observations must not activate direct mobile hosting");
    const auto observed_control = core::find_capability_decision(observed, "hardware_control");
    expect(observed_control.has_value(), "every negotiated feature must expose an auditable decision");
    expect(observed_control->evidence == core::EvidenceLevel::observed, "the decision must retain observed evidence without promoting it");

    connection.protocol_evidence.at(0).level = core::EvidenceLevel::qualified;
    connection.protocol_evidence.at(1).level = core::EvidenceLevel::qualified;
    const auto qualified = core::negotiate(device.capability, platform_profile.capability, daw.capability, connection);
    expect(qualified.hardware_control, "qualified duplex MIDI fixture may activate the synthetic control route");
    expect(qualified.audio_capture, "qualified input fixture may activate the synthetic audio route");
    expect(qualified.bidirectional_sync, "qualified device and host feedback fixture may activate synthetic sync");
    expect(qualified.mobile_bridge, "qualified native mobile fixture must be represented as a direct bridge host");

    platform::DiscoveredDevice discovered;
    discovered.interfaces = {
        {"usb-audio-fixture", "container-fixture", "00", "usbaudio2", "MPC Sample Audio"},
        {"usb-unknown-fixture", "container-fixture", "03", "", ""},
        {"usb-parent-fixture", "container-fixture", "", "usbccgp", ""}
    };
    const auto os_evidence = platform::protocol_evidence_for(discovered);
    expect(core::strongest_protocol_evidence(os_evidence, core::ProtocolKind::usb_audio, core::ProtocolDirection::discovery) == core::EvidenceLevel::observed,
           "Windows class-service evidence must remain an observation");
    expect(core::strongest_protocol_evidence(os_evidence, core::ProtocolKind::usb_audio, core::ProtocolDirection::input) == core::EvidenceLevel::unavailable,
           "class-service discovery must not invent stream direction or input access");
    expect(core::strongest_protocol_evidence(os_evidence, core::ProtocolKind::usb_midi, core::ProtocolDirection::discovery) == core::EvidenceLevel::unavailable,
           "an Audio class service must not be relabeled as MIDI without endpoint evidence");
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

void sync_and_backend_contract_test() {
    using namespace ubridge;
    core::SyncRequest request;
    request.expected = {1, 1, 1};
    request.actual = request.expected;
    expect(!core::authorize_sync(request).may_execute, "sync must require both approval and verified backup");
    request.user_approved = true;
    request.backup_verified = true;
    expect(core::authorize_sync(request).may_execute, "unchanged, approved, backed-up sync plan may execute");
    request.actual.hardware = 2;
    expect(!core::authorize_sync(request).may_execute, "stale hardware revision must invalidate a sync plan");
    static_assert(platform::mpc_sample_vendor_id == 0x09E8,
                  "observed MPC Sample vendor ID must remain explicit");
    static_assert(platform::mpc_sample_product_id == 0x205C,
                  "observed MPC Sample product ID must remain explicit");
    expect(platform::to_string(platform::BackendMaturity::experimental) == "experimental", "backend maturity must be reportable");
    expect(platform::make_system_device_discovery() != nullptr, "platform discovery factory must always return a safe implementation");
    expect(platform::make_system_midi_backend() != nullptr, "platform MIDI factory must always return an implementation");
    expect(platform::make_system_audio_backend() != nullptr, "platform audio factory must always return an implementation");
    expect(platform::to_string(platform::EndpointDirection::input) == "input", "endpoint direction must be reportable");

    const std::vector<platform::MidiEndpoint> portable_midi = {
        {"machine-a:7", "Acme Drum Workstation", "Acme", platform::EndpointDirection::input, "ALSA", "MIDI 1.0", true},
        {"machine-b:2", "Studio Keys", "Example", platform::EndpointDirection::input, "CoreMIDI", "MIDI 2.0", true}
    };
    const auto drum_input = platform::resolve_midi_endpoint(portable_midi, {platform::EndpointDirection::input, {"drum"}, {}, {}, 0});
    expect(drum_input.endpoint_id == "machine-a:7" && !drum_input.ambiguous, "portable MIDI roles must resolve without a saved Windows endpoint ID");

    const std::vector<platform::AudioEndpoint> portable_audio = {
        {"wasapi-id", "Interface ADAT 1-8", platform::EndpointDirection::input, true, 48000, 8, 32},
        {"coreaudio-id", "Built-in Microphone", platform::EndpointDirection::input, true, 48000, 2, 32}
    };
    const auto optical_capture = platform::resolve_audio_endpoint(portable_audio, {platform::EndpointDirection::input, {"adat", "optical"}, {}, {}, 8});
    expect(optical_capture.endpoint_id == "wasapi-id", "portable optical roles must use capabilities and hints instead of an Audient-specific ID");
    const auto ambiguous = platform::resolve_audio_endpoint({
        {"one", "Input", platform::EndpointDirection::input, true, 44100, 2, 32},
        {"two", "Input", platform::EndpointDirection::input, true, 44100, 2, 32}},
        {platform::EndpointDirection::input, {}, {}, {}, 2});
    expect(ambiguous.ambiguous, "equally suitable devices must require user selection instead of silently choosing hardware");
}

void state_mirror_and_acknowledgement_test() {
    using namespace ubridge::core;

    ParameterMirror mirror;
    mirror.parameter_id = "pad-A01.volume";
    mirror.desired_value = "-6.0";
    mirror.hardware = StateObservation{StateOrigin::hardware, "-3.0", {2, 2, 1}, 100, EvidenceLevel::observed};
    mirror.daw = StateObservation{StateOrigin::daw, "-6.0", {2, 1, 2}, 110, EvidenceLevel::observed};
    expect(evaluate_parameter_mirror(mirror) == MirrorState::pending_hardware, "a DAW observation at the desired value must leave the divergent hardware side pending");
    mirror.desired_value = "-9.0";
    expect(evaluate_parameter_mirror(mirror) == MirrorState::conflict, "divergent observations that match neither desired value must remain a conflict");

    auto blocked = plan_parameter_write("write-0", "pad-A01.volume", WriteTarget::hardware, "-6.0", {2, 2, 2}, EvidenceLevel::observed, true, 1000, 5000);
    expect(blocked.phase == WritePhase::blocked, "observed mapping evidence must not activate a physical parameter write");

    auto write = plan_parameter_write("write-1", "pad-A01.volume", WriteTarget::hardware, "-6.0", {2, 2, 2}, EvidenceLevel::qualified, true, 1000, 5000);
    expect(write.phase == WritePhase::planned, "qualified writable mapping may produce a bounded write plan");
    expect(!dispatch_parameter_write(write, {2, 3, 2}, 2000) && write.phase == WritePhase::conflict, "revision drift must block dispatch");

    auto acknowledged = plan_parameter_write("write-2", "pad-A01.volume", WriteTarget::hardware, "-6.0", {2, 2, 2}, EvidenceLevel::qualified, true, 1000, 5000);
    expect(dispatch_parameter_write(acknowledged, {2, 2, 2}, 2000), "current write plan must dispatch without implying acknowledgement");
    expect(acknowledge_parameter_write(acknowledged, {StateOrigin::hardware, "-6.0", {3, 3, 2}, 2500, EvidenceLevel::observed}), "matching target observation must acknowledge the write");
    expect(acknowledged.phase == WritePhase::acknowledged, "acknowledged write must preserve a distinct lifecycle state");

    auto timeout = plan_parameter_write("write-3", "pad-A02.pan", WriteTarget::daw, "0.25", {3, 3, 3}, EvidenceLevel::qualified, true, 1000, 5000);
    expect(dispatch_parameter_write(timeout, {3, 3, 3}, 2000), "second write fixture must dispatch");
    expect(expire_parameter_write(timeout, 6000) && timeout.phase == WritePhase::timed_out, "missing acknowledgement must time out instead of assuming success");

    auto stale_ack = plan_parameter_write("write-4", "pad-A03.tune", WriteTarget::hardware, "2.0", {4, 4, 3}, EvidenceLevel::qualified, true, 1000, 5000);
    expect(dispatch_parameter_write(stale_ack, {4, 4, 3}, 2000), "stale acknowledgement fixture must dispatch");
    expect(!acknowledge_parameter_write(stale_ack, {StateOrigin::hardware, "2.0", {3, 3, 3}, 2500, EvidenceLevel::observed}) && stale_ack.phase == WritePhase::conflict,
           "an observation from an older target revision must not acknowledge a write");
}

void clock_recording_and_hardware_learn_test() {
    using namespace ubridge::core;

    const std::vector<ClockDomainState> invalid_domains = {
        {ClockDomainKind::midi_clock, ClockAuthority::hardware, EvidenceLevel::observed, "mpc-sample", true, 24.0, 50.0, 0.0},
        {ClockDomainKind::audio_sample_clock, ClockAuthority::daw, EvidenceLevel::qualified, "audient", true, 0.0, 1.0, 64.0}
    };
    const auto invalid_clock = validate_clock_domains(invalid_domains);
    expect(invalid_clock.size() == 2, "unqualified lock and missing sample rate must remain separate clock-domain failures");

    const std::vector<ClockDomainState> valid_domains = {
        {ClockDomainKind::transport, ClockAuthority::daw, EvidenceLevel::qualified, "host", true, 0.0, 0.0, 0.0},
        {ClockDomainKind::midi_clock, ClockAuthority::daw, EvidenceLevel::qualified, "host", true, 24.0, 40.0, 0.0},
        {ClockDomainKind::audio_sample_clock, ClockAuthority::external, EvidenceLevel::qualified, "interface", true, 48000.0, 1.0, 64.0}
    };
    expect(validate_clock_domains(valid_domains).empty(), "independently qualified clock domains with explicit authorities must validate");

    const auto unsupported_both = plan_recording(RecordingDestination::both, MonitorPath::direct_hardware, RecordingProcessing::clean, EvidenceLevel::qualified, false, true);
    expect(!unsupported_both.ready, "record-to-both must remain disabled when simultaneous capture is unavailable");
    const auto clean_daw = plan_recording(RecordingDestination::daw, MonitorPath::direct_hardware, RecordingProcessing::clean_with_monitor_effects, EvidenceLevel::qualified, false, true);
    expect(clean_daw.ready, "qualified DAW recording may monitor effects while preserving the clean source");

    const auto learned = assess_hardware_learn("user.mpc-sample", {
        {"midi-in:0", "pad-1", "note:36", LearnedControlKind::pad, 1, 127, 12, EvidenceLevel::observed},
        {"midi-in:0", "play", "system:FA", LearnedControlKind::transport, 0, 1, 2, EvidenceLevel::observed}
    }, false);
    expect(learned.ready_to_save, "complete receive-only learning observations must be saveable as an unqualified profile");
    const auto unsafe_feedback = assess_hardware_learn("user.mpc-sample", learned.observations, true);
    expect(unsafe_feedback.ready_to_save, "requesting a separate feedback test must not invalidate receive-only observations");
    expect(!unsafe_feedback.diagnostics.empty(), "outbound feedback must retain a separate qualification warning");
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

void session_branch_and_merge_test() {
    using namespace ubridge;
    const core::RevisionVector base{4, 2, 2};
    auto hardware = session::create_branch("mpc/revision-3", session::BranchOrigin::hardware, base, {
        {"sequence-1", "name", "Verse", "Verse MPC", core::SyncPolicy::bidirectional},
        {"pad-1", "sample", "kick-a", "kick-b", core::SyncPolicy::bidirectional}
    });
    auto daw = session::create_branch("daw/revision-3", session::BranchOrigin::daw, base, {
        {"sequence-1", "name", "Verse", "Verse DAW", core::SyncPolicy::bidirectional},
        {"mixer-1", "volume", "-3", "-2", core::SyncPolicy::bidirectional}
    });
    auto merge = session::plan_merge("session-1", hardware, daw);
    expect(merge.conflicts.size() == 1, "same field edited differently must remain a branch conflict");
    expect(merge.automatic_changes.size() == 2, "independent branch changes must merge automatically");
    expect(!merge.ready_to_apply, "unresolved conflict must block merge application");
    expect(session::resolve_conflict(merge, {"sequence-1", "name", session::MergeChoice::keep_hardware, {}}), "hardware version must be selectable singularly");
    expect(merge.ready_to_apply, "resolved merge must become ready without overwriting either source branch");
    expect(session::extract_section(merge, {"mpc/revision-3", {"pad-1"}, "abstract/kick-change"}), "selected entities must be extractable into a new branch");
    expect(merge.extractions.size() == 1, "section extraction must remain auditable");
}

void pad_slice_sequence_song_test() {
    using namespace ubridge;
    auto session = complete_session_fixture(false);
    session.slices = {{"slice-1", "asset-kick", 100, 1000, 0, "Kick chop"}};
    session.programs = {{"program-a", "Drum Program", {"pad-kick", "pad-snare"}, {"slice-1"}}};
    session.sequences = {{"sequence-intro", "Intro", 3840, {"track-drums"}}};
    session.songs = {{"song-1", "Song", {{"sequence-intro", 2}}}};
    expect(session::validate(session).empty(), "valid pads, chops, instrument program, sequence, and song must share the canonical session");
    const core::AssetReference dropped{"asset-drop", "daw/drop.wav", "sha256:drop", 1234, true};
    const auto gated = session::plan_pad_assignment("drop-1", session::TransferDirection::daw_to_hardware, "daw/revision-4", "program-a", 3, dropped, false, false);
    expect(!gated.ready_to_apply, "DAW drag/drop must not write an MPC pad before backup and target format qualification");
    const auto ready = session::plan_pad_assignment("drop-2", session::TransferDirection::hardware_to_daw, "mpc/revision-4", "program-a", 3, dropped, true, true);
    expect(ready.ready_to_apply, "qualified backed-up pad assignment may become ready for an adapter transaction");
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
    expect(workflow_json.find("\"evidence\": \"declared\"") != std::string::npos, "workflow serializer must expose capability evidence levels");
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
    expect(ubridge::platform::midi_message_semantic(std::vector<std::uint8_t>{0xfaU}) == "transport_start", "MIDI Start must be decoded as transport_start");
    expect(ubridge::platform::midi_message_semantic(std::vector<std::uint8_t>{0xfbU}) == "transport_continue", "MIDI Continue must be decoded as transport_continue");
    expect(ubridge::platform::midi_message_semantic(std::vector<std::uint8_t>{0xfcU}) == "transport_stop", "MIDI Stop must be decoded as transport_stop");
    expect(ubridge::platform::midi_message_semantic(std::vector<std::uint8_t>{0xf8U}) == "timing_clock", "MIDI Clock must be decoded as timing_clock");
    expect(ubridge::platform::midi_message_semantic(std::vector<std::uint8_t>{0xf2U, 0x00U, 0x00U}) == "song_position_pointer", "MIDI Song Position must be decoded");
    xpj_reader_test();
    profile_registry_test();
    negotiation_and_workflow_test();
    mobile_and_audio_safety_test();
    protocol_evidence_and_host_negotiation_test();
    conflict_and_transaction_test();
    state_mirror_and_acknowledgement_test();
    clock_recording_and_hardware_learn_test();
    sync_and_backend_contract_test();
    local_service_test();
    session_asset_and_archive_test();
    session_branch_and_merge_test();
    pad_slice_sequence_song_test();
    performance_and_routing_test();
    reporting_and_profile_test();
    virtual_device_test();
    std::cout << "Universal Bridge virtual hardware laboratory passed.\n";
    return 0;
}
