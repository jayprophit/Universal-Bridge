#pragma once

#include "ubridge/bridge_modules.hpp"
#include "ubridge/core/performance_tools.hpp"
#include "ubridge/core/session_tools.hpp"

#include <string>
#include <vector>

namespace ubridge::reporting {

struct ProfileClaim {
    std::string capability;
    bool enabled = false;
    bool requires_physical_qualification = true;
};

struct ProfileDocument {
    std::string id;
    std::string kind; // device, DAW, platform, controller, connection
    std::string schema_version;
    std::string version;
    std::string issuer;
    bool signed_profile = false;
    std::vector<ProfileClaim> claims;
};

struct ProfileValidationResult {
    bool valid = false;
    std::vector<core::Diagnostic> diagnostics;
};

struct CompatibilityItem {
    std::string feature;
    std::string status;
    std::string explanation;
};

struct CompatibilityReport {
    std::string report_id;
    std::string route_name;
    int score_percent = 0;
    std::vector<CompatibilityItem> items;
    std::vector<core::Diagnostic> diagnostics;
};

[[nodiscard]] ProfileValidationResult validate_profile(
    const ProfileDocument& profile,
    const core::PlatformCapability& platform);
[[nodiscard]] CompatibilityReport build_compatibility_report(
    std::string report_id,
    const core::IntegrationPlan& integration,
    const std::vector<core::Diagnostic>& source_diagnostics);
[[nodiscard]] std::string serialize_workflow_json(const modules::FinishWorkflow& workflow);
[[nodiscard]] std::string serialize_archive_plan_json(const session::PortableArchivePlan& archive);
[[nodiscard]] std::string serialize_compatibility_report_json(const CompatibilityReport& report);

} // namespace ubridge::reporting
