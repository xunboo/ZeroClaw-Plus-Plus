#pragma once

/// Tools module — aggregates all tool implementations and provides
/// the tool registry factory for creating the default tool set.

#include "tools/traits.hpp"
#include "tools/shell.hpp"
#include "tools/file_read.hpp"
#include "tools/file_write.hpp"
#include "tools/other_tools.hpp"

#include <vector>
#include <memory>
#include <string>

namespace zeroclaw {
namespace security { class SecurityPolicy; }
namespace tools {

/// Create the default tool registry with core tools
std::vector<std::unique_ptr<Tool>> default_tools(
    std::shared_ptr<security::SecurityPolicy> security);

/// Create the full tool registry including memory and integration tools
std::vector<std::unique_ptr<Tool>> all_tools(
    std::shared_ptr<security::SecurityPolicy> security,
    bool enable_browser = false,
    bool enable_composio = false,
    const std::string& composio_key = "",
    const std::string& composio_entity = "");

} // namespace tools
} // namespace zeroclaw
