#pragma once

#include "ubridge/core/bridge_core.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ubridge::core {

struct SessionStoreLimits {
    std::uintmax_t maximum_snapshot_bytes = 32ULL * 1024ULL * 1024ULL;
    std::size_t maximum_assets = 100'000;
    std::size_t maximum_midi_events = 2'000'000;
    std::size_t maximum_parameters = 250'000;
    std::size_t maximum_metadata_entries = 10'000;
};

enum class SessionLoadSource {
    none,
    current,
    pending,
    previous,
};

struct SessionLoadResult {
    std::optional<CanonicalSession> session;
    SessionLoadSource source = SessionLoadSource::none;
    bool recovered = false;
    std::vector<Diagnostic> diagnostics;
};

class DurableSessionStore {
  public:
    explicit DurableSessionStore(std::filesystem::path root, SessionStoreLimits limits = {});

    [[nodiscard]] bool save(const CanonicalSession& session, std::vector<Diagnostic>& diagnostics) const;
    [[nodiscard]] SessionLoadResult load(std::string_view session_id) const;
    [[nodiscard]] std::filesystem::path snapshot_path(std::string_view session_id) const;
    [[nodiscard]] const std::filesystem::path& root() const noexcept;

  private:
    std::filesystem::path root_;
    SessionStoreLimits limits_;
};

[[nodiscard]] std::string to_string(SessionLoadSource source);

} // namespace ubridge::core
