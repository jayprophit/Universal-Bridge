#include "ubridge/reporting_tools.hpp"

#include <algorithm>
#include <set>
#include <sstream>
#include <utility>

namespace ubridge::reporting {
namespace {

std::string escape_json(const std::string& value) {
    std::ostringstream output;
    for (const char character : value) {
        switch (character) {
            case '\\': output << "\\\\"; break;
            case '"': output << "\\\""; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default: output << character; break;
        }
    }
    return output.str();
}

const char* boolean(bool value) {
    return value ? "true" : "false";
}

bool has_error(const std::vector<core::Diagnostic>& diagnostics) {
    return std::any_of(diagnostics.begin(), diagnostics.end(), [](const core::Diagnostic& diagnostic) {
        return diagnostic.severity == core::DiagnosticSeverity::error;
    });
}

void append_diagnostics(std::ostringstream& output, const std::vector<core::Diagnostic>& diagnostics, const std::string& indent) {
    output << "[";
    for (std::size_t index = 0; index < diagnostics.size(); ++index) {
        const auto& diagnostic = diagnostics[index];
        output << "\n" << indent << "  {\"severity\": \"" << core::to_string(diagnostic.severity)
               << "\", \"code\": \"" << escape_json(diagnostic.code)
               << "\", \"message\": \"" << escape_json(diagnostic.message)
               << "\", \"recommendation\": \"" << escape_json(diagnostic.recommendation) << "\"}";
        if (index + 1 < diagnostics.size()) {
            output << ",";
        }
    }
    if (!diagnostics.empty()) {
        output << "\n" << indent;
    }
    output << "]";
}

} // namespace

ProfileValidationResult validate_profile(const ProfileDocument& profile, const core::PlatformCapability& platform) {
    ProfileValidationResult result;
    const std::set<std::string> kinds = {"device", "daw", "platform", "controller", "connection"};
    if (profile.id.empty()) {
        result.diagnostics.push_back({core::DiagnosticSeverity::error, "profile_id_missing", "Profile has no stable identifier.", "Use a reverse-domain or vendor-qualified identifier."});
    }
    if (!kinds.contains(profile.kind)) {
        result.diagnostics.push_back({core::DiagnosticSeverity::error, "profile_kind_invalid", "Profile kind is not recognized.", "Use device, daw, platform, controller, or connection."});
    }
    if (profile.schema_version.empty() || profile.version.empty()) {
        result.diagnostics.push_back({core::DiagnosticSeverity::error, "profile_version_missing", "Profile schema and version are required.", "Provide a compatible schema version and a profile revision."});
    }
    if (profile.issuer.empty()) {
        result.diagnostics.push_back({core::DiagnosticSeverity::warning, "profile_issuer_missing", "Profile issuer is unknown.", "Record the author or organization before sharing the profile."});
    }

    std::set<std::string> capabilities;
    for (const auto& claim : profile.claims) {
        if (claim.capability.empty()) {
            result.diagnostics.push_back({core::DiagnosticSeverity::error, "profile_claim_empty", "Profile has an unnamed capability claim.", "Name every capability semantically and unambiguously."});
            continue;
        }
        if (!capabilities.insert(claim.capability).second) {
            result.diagnostics.push_back({core::DiagnosticSeverity::error, "profile_claim_duplicate", "Profile claims capability '" + claim.capability + "' more than once.", "Merge duplicate claims into one authoritative declaration."});
        }
        if (claim.enabled && claim.requires_physical_qualification && !profile.signed_profile) {
            result.diagnostics.push_back({core::DiagnosticSeverity::error, "unsigned_active_physical_claim", "An unsigned profile tries to enable physical capability '" + claim.capability + "'.", "Require a signed, reviewed profile before enabling physical device, audio, MIDI, or write operations."});
        }
        if (claim.enabled && claim.requires_physical_qualification && !platform.runtime_qualified) {
            result.diagnostics.push_back({core::DiagnosticSeverity::warning, "platform_claim_gated", "Physical capability '" + claim.capability + "' is declared but the selected platform is not qualified.", "Keep the capability unavailable until the platform backend and device route pass qualification."});
        }
    }
    result.valid = !has_error(result.diagnostics);
    return result;
}

CompatibilityReport build_compatibility_report(
    std::string report_id,
    const core::IntegrationPlan& integration,
    const std::vector<core::Diagnostic>& source_diagnostics) {
    CompatibilityReport report;
    report.report_id = std::move(report_id);
    report.route_name = integration.route_name;
    report.diagnostics = source_diagnostics;

    const auto add = [&report](const std::string& feature, bool enabled, const std::string& enabled_text, const std::string& disabled_text, int score) {
        report.items.push_back({feature, enabled ? "available" : "gated", enabled ? enabled_text : disabled_text});
        if (enabled) {
            report.score_percent += score;
        }
    };
    add("safe_preflight", integration.safe_preflight, "Read-only project intake is available.", "No qualified project/storage intake route is available.", 20);
    add("asset_exchange", integration.asset_exchange, "Audio asset exchange is available.", "Audio asset exchange is unavailable.", 15);
    add("midi_exchange", integration.midi_exchange, "MIDI exchange is available.", "MIDI exchange is unavailable.", 15);
    add("direct_daw_creation", integration.direct_daw_creation, "Direct DAW creation route is available.", "Direct DAW creation remains gated; use exchange package.", 15);
    add("hardware_control", integration.hardware_control, "Live hardware control is available.", "Hardware control remains gated.", 10);
    add("audio_capture", integration.audio_capture, "Live audio capture is available.", "Audio capture remains gated.", 10);
    add("bidirectional_sync", integration.bidirectional_sync, "Bidirectional sync is available.", "Bidirectional sync remains gated.", 10);
    add("mobile_companion", integration.mobile_companion, "Companion session pathway is available.", "No companion pathway is declared.", 5);
    add("mobile_bridge", integration.mobile_bridge, "The mobile/tablet runtime is qualified as a direct bridge host.", "Direct mobile/tablet host operation remains gated.", 0);

    for (const auto& limitation : integration.limitations) {
        report.diagnostics.push_back({core::DiagnosticSeverity::info, "capability_limitation", limitation, "Review the capability matrix and use the documented safe fallback."});
    }
    return report;
}

std::string serialize_workflow_json(const modules::FinishWorkflow& workflow) {
    std::ostringstream output;
    output << "{\n  \"schema_version\": \"0.4.0\",\n"
           << "  \"workflow_id\": \"" << escape_json(workflow.id) << "\",\n"
           << "  \"route_name\": \"" << escape_json(workflow.integration.route_name) << "\",\n"
           << "  \"capabilities\": {\n"
           << "    \"safe_preflight\": " << boolean(workflow.integration.safe_preflight) << ",\n"
           << "    \"asset_exchange\": " << boolean(workflow.integration.asset_exchange) << ",\n"
           << "    \"midi_exchange\": " << boolean(workflow.integration.midi_exchange) << ",\n"
           << "    \"direct_daw_creation\": " << boolean(workflow.integration.direct_daw_creation) << ",\n"
           << "    \"hardware_control\": " << boolean(workflow.integration.hardware_control) << ",\n"
           << "    \"audio_capture\": " << boolean(workflow.integration.audio_capture) << ",\n"
           << "    \"bidirectional_sync\": " << boolean(workflow.integration.bidirectional_sync) << ",\n"
           << "    \"mobile_companion\": " << boolean(workflow.integration.mobile_companion) << ",\n"
           << "    \"mobile_bridge\": " << boolean(workflow.integration.mobile_bridge) << "\n"
           << "  },\n  \"capability_decisions\": [";
    for (std::size_t index = 0; index < workflow.integration.decisions.size(); ++index) {
        const auto& decision = workflow.integration.decisions[index];
        output << "\n    {\"capability\": \"" << escape_json(decision.capability)
               << "\", \"enabled\": " << boolean(decision.enabled)
               << ", \"evidence\": \"" << core::to_string(decision.evidence)
               << "\", \"rationale\": \"" << escape_json(decision.rationale) << "\"}";
        if (index + 1 < workflow.integration.decisions.size()) {
            output << ",";
        }
    }
    output << "\n  ],\n  \"steps\": [";
    for (std::size_t index = 0; index < workflow.steps.size(); ++index) {
        const auto& step = workflow.steps[index];
        output << "\n    {\"id\": \"" << escape_json(step.id) << "\", \"title\": \"" << escape_json(step.title)
               << "\", \"requires_user_approval\": " << boolean(step.requires_user_approval)
               << ", \"enabled\": " << boolean(step.enabled)
               << ", \"rationale\": \"" << escape_json(step.rationale) << "\"}";
        if (index + 1 < workflow.steps.size()) {
            output << ",";
        }
    }
    output << "\n  ],\n  \"limitations\": [";
    for (std::size_t index = 0; index < workflow.integration.limitations.size(); ++index) {
        output << "\n    \"" << escape_json(workflow.integration.limitations[index]) << "\"";
        if (index + 1 < workflow.integration.limitations.size()) {
            output << ",";
        }
    }
    output << "\n  ]\n}\n";
    return output.str();
}

std::string serialize_archive_plan_json(const session::PortableArchivePlan& archive) {
    std::ostringstream output;
    output << "{\n  \"schema_version\": \"" << escape_json(archive.schema_version) << "\",\n"
           << "  \"archive_id\": \"" << escape_json(archive.archive_id) << "\",\n"
           << "  \"session_id\": \"" << escape_json(archive.session_id) << "\",\n"
           << "  \"ready_to_package\": " << boolean(archive.ready_to_package) << ",\n"
           << "  \"entries\": [";
    for (std::size_t index = 0; index < archive.entries.size(); ++index) {
        const auto& entry = archive.entries[index];
        output << "\n    {\"asset_id\": \"" << escape_json(entry.asset_id)
               << "\", \"source_path\": \"" << escape_json(entry.source_path)
               << "\", \"archive_path\": \"" << escape_json(entry.archive_path)
               << "\", \"fingerprint\": \"" << escape_json(entry.fingerprint)
               << "\", \"bytes\": " << entry.bytes
               << ", \"required\": " << boolean(entry.required) << "}";
        if (index + 1 < archive.entries.size()) {
            output << ",";
        }
    }
    output << "\n  ],\n  \"diagnostics\": ";
    append_diagnostics(output, archive.diagnostics, "  ");
    output << "\n}\n";
    return output.str();
}

std::string serialize_compatibility_report_json(const CompatibilityReport& report) {
    std::ostringstream output;
    output << "{\n  \"schema_version\": \"0.4.0\",\n"
           << "  \"report_id\": \"" << escape_json(report.report_id) << "\",\n"
           << "  \"route_name\": \"" << escape_json(report.route_name) << "\",\n"
           << "  \"score_percent\": " << report.score_percent << ",\n"
           << "  \"items\": [";
    for (std::size_t index = 0; index < report.items.size(); ++index) {
        const auto& item = report.items[index];
        output << "\n    {\"feature\": \"" << escape_json(item.feature)
               << "\", \"status\": \"" << escape_json(item.status)
               << "\", \"explanation\": \"" << escape_json(item.explanation) << "\"}";
        if (index + 1 < report.items.size()) {
            output << ",";
        }
    }
    output << "\n  ],\n  \"diagnostics\": ";
    append_diagnostics(output, report.diagnostics, "  ");
    output << "\n}\n";
    return output.str();
}

} // namespace ubridge::reporting
