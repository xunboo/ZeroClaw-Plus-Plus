#pragma once

/// Migration module — configuration file migration between versions.

#include <string>
#include <optional>
#include "nlohmann/json.hpp"

namespace zeroclaw {
namespace migration {

/// Migration version info
struct MigrationInfo {
    int from_version;
    int to_version;
    std::string description;
};

/// Check if config needs migration
std::optional<MigrationInfo> needs_migration(const nlohmann::json& config);

/// Apply migration to config
nlohmann::json apply_migration(const nlohmann::json& config, const MigrationInfo& migration);

/// Current config version
constexpr int CURRENT_CONFIG_VERSION = 2;

} // namespace migration
} // namespace zeroclaw
