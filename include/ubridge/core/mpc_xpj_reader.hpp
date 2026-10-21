#pragma once

#include "ubridge/core/bridge_core.hpp"
#include "ubridge/core/session_tools.hpp"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace ubridge::mpc {

struct XpjParserLimits {
    std::uintmax_t maximum_compressed_bytes = 64ULL * 1024ULL * 1024ULL;
    std::size_t maximum_decompressed_bytes = 128ULL * 1024ULL * 1024ULL;
    std::size_t maximum_json_depth = 64;
    std::size_t maximum_json_nodes = 1'000'000;
    std::size_t maximum_string_bytes = 64ULL * 1024ULL * 1024ULL;
    std::size_t maximum_container_elements = 250'000;
    std::size_t maximum_opaque_retention_bytes = 16ULL * 1024ULL * 1024ULL;
};

struct XpjInspection {
    bool readable = false;
    bool gzip_container = false;
    bool json_payload = false;
    int schema_version = 0;
    double master_tempo = 0.0;
    std::size_t sample_count = 0;
    std::size_t track_count = 0;
    std::size_t sequence_count = 0;
    std::size_t song_slot_count = 0;
    std::size_t available_asset_count = 0;
    std::size_t missing_asset_count = 0;
    std::size_t decompressed_bytes = 0;
    std::size_t json_nodes = 0;
    std::size_t maximum_depth = 0;
    std::vector<std::string> preamble;
    std::vector<std::string> sample_names;
    std::vector<core::Diagnostic> diagnostics;
};

struct XpjCanonicalImport {
    session::FullSession session;
    session::SessionBranch hardware_branch;
    std::vector<std::string> unmapped_field_groups;
    std::string opaque_source_json;
    bool opaque_source_retained = false;
    std::vector<core::Diagnostic> diagnostics;
    std::size_t mapped_note_events = 0;
    std::size_t unmapped_automation_events = 0;
    bool valid = false;
};

// Read-only: opens the XPJ and its sibling ProjectData directory without
// creating, changing, or deleting anything in the source folder.
[[nodiscard]] XpjInspection inspect_xpj(const std::filesystem::path& project_file, XpjParserLimits limits = {});
[[nodiscard]] XpjCanonicalImport import_xpj(const std::filesystem::path& project_file, XpjParserLimits limits = {});
[[nodiscard]] std::string serialize_import_json(const XpjCanonicalImport& imported);

} // namespace ubridge::mpc
