#pragma once
/// SOP loading, parsing, and validation.
/// Mirrors Rust src/sop/mod.rs

#include "types.hpp"
#include <string>
#include <vector>
#include <filesystem>

namespace zeroclaw {
namespace sop {

/// Return the default SOPs directory: <workspace>/sops
std::filesystem::path resolve_sops_dir(const std::filesystem::path& workspace_dir,
                                        const std::string& config_dir = "");

/// Load all SOPs from the configured directory
std::vector<Sop> load_sops(const std::filesystem::path& workspace_dir,
                             const std::string& config_dir,
                             SopExecutionMode default_mode);

/// Load SOPs from a specific directory
std::vector<Sop> load_sops_from_directory(const std::filesystem::path& sops_dir,
                                            SopExecutionMode default_mode);

/// Parse SOP.md markdown into steps
std::vector<SopStep> parse_steps(const std::string& md);

/// Validate a SOP and return warnings
std::vector<std::string> validate_sop(const Sop& sop);

} // namespace sop
} // namespace zeroclaw
