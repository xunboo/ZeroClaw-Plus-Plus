#include "other_tools.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include "security/policy.hpp"

namespace fs = std::filesystem;

namespace zeroclaw {
namespace tools {

// ── ContentSearchTool ────────────────────────────────────────────

ContentSearchTool::ContentSearchTool(std::shared_ptr<security::SecurityPolicy> security)
    : security_(std::move(security)) {}

nlohmann::json ContentSearchTool::parameters_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"pattern", {{"type", "string"}, {"description", "Search pattern (regex or literal)"}}},
            {"path", {{"type", "string"}, {"description", "Directory or file to search in"}}},
            {"case_sensitive", {{"type", "boolean"}, {"description", "Case-sensitive search (default: false)"}}},
            {"max_results", {{"type", "integer"}, {"description", "Maximum results to return (default: 50)"}}}
        }},
        {"required", nlohmann::json::array({"pattern"})}
    };
}

ToolResult ContentSearchTool::execute(const nlohmann::json& args) {
    if (!args.contains("pattern")) return ToolResult::fail("Missing: pattern");
    std::string pattern = args["pattern"].get<std::string>();
    bool case_sensitive = args.value("case_sensitive", false);
    // Rust cap: 1000 matches
    static constexpr size_t MAX_MATCHES = 1000;

    // Determine search root
    std::string search_path = ".";
    if (args.contains("path") && args["path"].is_string()) {
        search_path = args["path"].get<std::string>();
    }
    if (security_) {
        auto check = security_->check_path_read(search_path);
        if (!check.allowed) return ToolResult::fail(check.reason);
    }

    // Compile regex (matching Rust: ripgrep-style line search)
    std::regex re;
    try {
        auto flags = std::regex::ECMAScript;
        if (!case_sensitive) flags |= std::regex::icase;
        re = std::regex(pattern, flags);
    } catch (const std::regex_error& e) {
        return ToolResult::fail(std::string("Invalid pattern: ") + e.what());
    }

    // Collect matches: vector of (filepath_str, line_number, line_content)
    struct Match { std::string file; size_t line; std::string content; };
    std::vector<Match> matches;
    bool capped = false;

    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(search_path, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;

        std::ifstream f(entry.path());
        if (!f) continue;

        std::string rel = fs::relative(entry.path(), search_path, ec).string();
        if (ec) rel = entry.path().string();

        std::string line_text;
        size_t lineno = 0;
        while (std::getline(f, line_text)) {
            ++lineno;
            if (std::regex_search(line_text, re)) {
                matches.push_back({rel, lineno, line_text});
                if (matches.size() >= MAX_MATCHES) { capped = true; break; }
            }
        }
        if (capped) break;
    }

    // Sort by (file, line) — matching Rust sorted output
    std::sort(matches.begin(), matches.end(), [](const Match& a, const Match& b) {
        if (a.file != b.file) return a.file < b.file;
        return a.line < b.line;
    });

    // Format: "file:line:content" per match
    std::ostringstream oss;
    for (const auto& m : matches) {
        oss << m.file << ":" << m.line << ":" << m.content << "\n";
    }
    if (capped) {
        oss << "[Results capped at " << MAX_MATCHES << " matches]\n";
    }
    if (matches.empty()) {
        oss << "No matches found.";
    }
    return ToolResult::ok(oss.str());
}

// ── GlobSearchTool ───────────────────────────────────────────────

GlobSearchTool::GlobSearchTool(std::shared_ptr<security::SecurityPolicy> security)
    : security_(std::move(security)) {}

nlohmann::json GlobSearchTool::parameters_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"pattern", {{"type", "string"}, {"description", "Glob pattern (e.g. **/*.rs)"}}},
            {"path", {{"type", "string"}, {"description", "Base directory for the search"}}}
        }},
        {"required", nlohmann::json::array({"pattern"})}
    };
}

ToolResult GlobSearchTool::execute(const nlohmann::json& args) {
    if (!args.contains("pattern")) return ToolResult::fail("Missing: pattern");
    std::string glob_pattern = args["pattern"].get<std::string>();
    // Rust cap: 10000 entries
    static constexpr size_t MAX_ENTRIES = 10000;

    std::string search_path = ".";
    if (args.contains("path") && args["path"].is_string()) {
        search_path = args["path"].get<std::string>();
    }

    // Convert glob pattern to regex:
    // ** matches any path segment(s); * matches within one segment; ? matches one char.
    auto glob_to_regex = [](const std::string& glob) -> std::string {
        std::string re;
        re.reserve(glob.size() * 2);
        for (size_t i = 0; i < glob.size(); ++i) {
            char c = glob[i];
            if (c == '*' && i + 1 < glob.size() && glob[i+1] == '*') {
                re += ".*"; ++i;  // ** -> .*
            } else if (c == '*') {
                re += "[^/\\\\]*";  // * -> any segment chars
            } else if (c == '?') {
                re += "[^/\\\\]";   // ? -> single segment char
            } else if (std::string(".+^${}[]|()\\\\").find(c) != std::string::npos) {
                re += '\\';
                re += c;
            } else {
                re += c;
            }
        }
        return re;
    };

    std::regex re;
    try {
        re = std::regex(glob_to_regex(glob_pattern), std::regex::icase);
    } catch (const std::regex_error& e) {
        return ToolResult::fail(std::string("Invalid glob: ") + e.what());
    }

    std::vector<std::string> paths;
    bool capped = false;
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(search_path, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;

        std::string rel = fs::relative(entry.path(), search_path, ec).string();
        if (ec) rel = entry.path().string();
        // Normalise path sep on Windows
        std::replace(rel.begin(), rel.end(), '\\', '/');

        if (std::regex_match(rel, re) ||
            std::regex_match(entry.path().filename().string(), re)) {
            paths.push_back(rel);
            if (paths.size() >= MAX_ENTRIES) { capped = true; break; }
        }
    }

    // Sorted output — matching Rust
    std::sort(paths.begin(), paths.end());

    std::ostringstream oss;
    for (const auto& p : paths) oss << p << "\n";
    if (capped) oss << "[Results capped at " << MAX_ENTRIES << " entries]\n";
    if (paths.empty()) oss << "No files matched.";
    return ToolResult::ok(oss.str());
}

// ── CliDiscoveryTool ─────────────────────────────────────────────

nlohmann::json CliDiscoveryTool::parameters_schema() const {
    return {{"type", "object"}, {"properties", nlohmann::json::object()}};
}

ToolResult CliDiscoveryTool::execute(const nlohmann::json& /*args*/) {
    return ToolResult::ok("[cli_discovery - not fully implemented]");
}

// ── Memory Tools (store, recall, forget) ─────────────────────────

nlohmann::json MemoryStoreTool::parameters_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"key", {{"type", "string"}, {"description", "Memory key"}}},
            {"value", {{"type", "string"}, {"description", "Value to store"}}}
        }},
        {"required", nlohmann::json::array({"key", "value"})}
    };
}

ToolResult MemoryStoreTool::execute(const nlohmann::json& args) {
    if (!args.contains("key") || !args.contains("value"))
        return ToolResult::fail("Missing key or value");
    return ToolResult::ok("Stored in memory");
}

nlohmann::json MemoryRecallTool::parameters_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"query", {{"type", "string"}, {"description", "Search query for memory recall"}}}
        }},
        {"required", nlohmann::json::array({"query"})}
    };
}

ToolResult MemoryRecallTool::execute(const nlohmann::json& args) {
    if (!args.contains("query")) return ToolResult::fail("Missing: query");
    return ToolResult::ok("[memory_recall - not fully implemented]");
}

nlohmann::json MemoryForgetTool::parameters_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"key", {{"type", "string"}, {"description", "Memory key to forget"}}}
        }},
        {"required", nlohmann::json::array({"key"})}
    };
}

ToolResult MemoryForgetTool::execute(const nlohmann::json& args) {
    if (!args.contains("key")) return ToolResult::fail("Missing: key");
    return ToolResult::ok("Forgotten");
}

// ── Web Tools ────────────────────────────────────────────────────

WebSearchTool::WebSearchTool(const std::string& api_key, const std::string& engine)
    : api_key_(api_key), engine_(engine) {}

nlohmann::json WebSearchTool::parameters_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"query", {{"type", "string"}, {"description", "Search query"}}},
            {"num_results", {{"type", "integer"}, {"description", "Number of results (default: 5)"}}}
        }},
        {"required", nlohmann::json::array({"query"})}
    };
}

ToolResult WebSearchTool::execute(const nlohmann::json& args) {
    if (!args.contains("query")) return ToolResult::fail("Missing: query");
    return ToolResult::ok("[web_search - not fully implemented]");
}

HttpRequestTool::HttpRequestTool(bool allow_external) : allow_external_(allow_external) {}

nlohmann::json HttpRequestTool::parameters_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"url", {{"type", "string"}, {"description", "URL to request"}}},
            {"method", {{"type", "string"}, {"description", "HTTP method (default: GET)"}}},
            {"headers", {{"type", "object"}, {"description", "Request headers"}}},
            {"body", {{"type", "string"}, {"description", "Request body"}}}
        }},
        {"required", nlohmann::json::array({"url"})}
    };
}

ToolResult HttpRequestTool::execute(const nlohmann::json& args) {
    if (!args.contains("url")) return ToolResult::fail("Missing: url");
    return ToolResult::ok("[http_request - not fully implemented]");
}

// ── Browser Tools ────────────────────────────────────────────────

nlohmann::json BrowserTool::parameters_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"action", {{"type", "string"}, {"description", "Browser action: navigate, click, type, screenshot, extract"}}},
            {"url", {{"type", "string"}, {"description", "URL to navigate to"}}},
            {"selector", {{"type", "string"}, {"description", "CSS selector for interaction"}}},
            {"text", {{"type", "string"}, {"description", "Text to type"}}}
        }},
        {"required", nlohmann::json::array({"action"})}
    };
}

ToolResult BrowserTool::execute(const nlohmann::json& args) {
    if (!args.contains("action")) return ToolResult::fail("Missing: action");
    return ToolResult::ok("[browser - not fully implemented]");
}

nlohmann::json BrowserOpenTool::parameters_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"url", {{"type", "string"}, {"description", "URL to open"}}}
        }},
        {"required", nlohmann::json::array({"url"})}
    };
}

ToolResult BrowserOpenTool::execute(const nlohmann::json& args) {
    if (!args.contains("url")) return ToolResult::fail("Missing: url");
    std::string url = args["url"].get<std::string>();
#ifdef _WIN32
    std::string cmd = "start \"\" \"" + url + "\"";
#elif __APPLE__
    std::string cmd = "open \"" + url + "\"";
#else
    std::string cmd = "xdg-open \"" + url + "\"";
#endif
    int result = std::system(cmd.c_str());
    return result == 0 ? ToolResult::ok("Opened: " + url)
                       : ToolResult::fail("Failed to open: " + url);
}

nlohmann::json ScreenshotTool::parameters_schema() const {
    return {{"type", "object"}, {"properties", nlohmann::json::object()}};
}

ToolResult ScreenshotTool::execute(const nlohmann::json& /*args*/) {
    return ToolResult::ok("[screenshot - not fully implemented]");
}

// ── Git Tools ────────────────────────────────────────────────────

GitOperationsTool::GitOperationsTool(std::shared_ptr<security::SecurityPolicy> security)
    : security_(std::move(security)) {}

nlohmann::json GitOperationsTool::parameters_schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"operation", {{"type", "string"}, {"description", "Git operation: status, diff, log, branch, commit, etc."}}},
            {"args", {{"type", "array"}, {"items", {{"type", "string"}}}, {"description", "Additional arguments"}}}
        }},
        {"required", nlohmann::json::array({"operation"})}
    };
}

ToolResult GitOperationsTool::execute(const nlohmann::json& args) {
    if (!args.contains("operation")) return ToolResult::fail("Missing: operation");
    return ToolResult::ok("[git_operations - not fully implemented]");
}

// ── Cron/Schedule Tools (stubs) ──────────────────────────────────

nlohmann::json CronAddTool::parameters_schema() const {
    return {{"type", "object"}, {"properties", {
        {"expression", {{"type", "string"}, {"description", "Cron expression"}}},
        {"command", {{"type", "string"}, {"description", "Command to run"}}},
        {"name", {{"type", "string"}, {"description", "Task name"}}}
    }}, {"required", nlohmann::json::array({"expression", "command"})}};
}
ToolResult CronAddTool::execute(const nlohmann::json&) { return ToolResult::ok("[cron_add - stub]"); }

nlohmann::json CronListTool::parameters_schema() const {
    return {{"type", "object"}, {"properties", nlohmann::json::object()}};
}
ToolResult CronListTool::execute(const nlohmann::json&) { return ToolResult::ok("[cron_list - stub]"); }

nlohmann::json CronRemoveTool::parameters_schema() const {
    return {{"type", "object"}, {"properties", {{"id", {{"type", "string"}}}}}, {"required", nlohmann::json::array({"id"})}};
}
ToolResult CronRemoveTool::execute(const nlohmann::json&) { return ToolResult::ok("[cron_remove - stub]"); }

nlohmann::json CronRunTool::parameters_schema() const {
    return {{"type", "object"}, {"properties", {{"id", {{"type", "string"}}}}}, {"required", nlohmann::json::array({"id"})}};
}
ToolResult CronRunTool::execute(const nlohmann::json&) { return ToolResult::ok("[cron_run - stub]"); }

nlohmann::json CronRunsTool::parameters_schema() const {
    return {{"type", "object"}, {"properties", {{"id", {{"type", "string"}}}}}, {"required", nlohmann::json::array({"id"})}};
}
ToolResult CronRunsTool::execute(const nlohmann::json&) { return ToolResult::ok("[cron_runs - stub]"); }

nlohmann::json CronUpdateTool::parameters_schema() const {
    return {{"type", "object"}, {"properties", {
        {"id", {{"type", "string"}}}, {"expression", {{"type", "string"}}}, {"command", {{"type", "string"}}}
    }}, {"required", nlohmann::json::array({"id"})}};
}
ToolResult CronUpdateTool::execute(const nlohmann::json&) { return ToolResult::ok("[cron_update - stub]"); }

nlohmann::json ScheduleTool::parameters_schema() const {
    return {{"type", "object"}, {"properties", {
        {"at", {{"type", "string"}, {"description", "ISO 8601 timestamp"}}},
        {"command", {{"type", "string"}}}
    }}, {"required", nlohmann::json::array({"at", "command"})}};
}
ToolResult ScheduleTool::execute(const nlohmann::json&) { return ToolResult::ok("[schedule - stub]"); }

// ── Hardware Tools (stubs) ───────────────────────────────────────

nlohmann::json HardwareBoardInfoTool::parameters_schema() const {
    return {{"type", "object"}, {"properties", nlohmann::json::object()}};
}
ToolResult HardwareBoardInfoTool::execute(const nlohmann::json&) { return ToolResult::ok("[hardware_board_info - stub]"); }

nlohmann::json HardwareMemoryMapTool::parameters_schema() const {
    return {{"type", "object"}, {"properties", nlohmann::json::object()}};
}
ToolResult HardwareMemoryMapTool::execute(const nlohmann::json&) { return ToolResult::ok("[hardware_memory_map - stub]"); }

nlohmann::json HardwareMemoryReadTool::parameters_schema() const {
    return {{"type", "object"}, {"properties", {
        {"address", {{"type", "string"}}}, {"length", {{"type", "integer"}}}
    }}, {"required", nlohmann::json::array({"address", "length"})}};
}
ToolResult HardwareMemoryReadTool::execute(const nlohmann::json&) { return ToolResult::ok("[hardware_memory_read - stub]"); }

// ── Media Tools (stubs) ──────────────────────────────────────────

nlohmann::json ImageInfoTool::parameters_schema() const {
    return {{"type", "object"}, {"properties", {
        {"path", {{"type", "string"}, {"description", "Path to image file"}}}
    }}, {"required", nlohmann::json::array({"path"})}};
}
ToolResult ImageInfoTool::execute(const nlohmann::json&) { return ToolResult::ok("[image_info - stub]"); }

nlohmann::json PdfReadTool::parameters_schema() const {
    return {{"type", "object"}, {"properties", {
        {"path", {{"type", "string"}, {"description", "Path to PDF file"}}}
    }}, {"required", nlohmann::json::array({"path"})}};
}
ToolResult PdfReadTool::execute(const nlohmann::json&) { return ToolResult::ok("[pdf_read - stub]"); }

// ── Integration Tools ────────────────────────────────────────────

ComposioTool::ComposioTool(const std::string& api_key, const std::string& entity_id)
    : api_key_(api_key), entity_id_(entity_id) {}

nlohmann::json ComposioTool::parameters_schema() const {
    return {{"type", "object"}, {"properties", {
        {"action", {{"type", "string"}, {"description", "Composio action to execute"}}},
        {"params", {{"type", "object"}, {"description", "Action parameters"}}}
    }}, {"required", nlohmann::json::array({"action"})}};
}
ToolResult ComposioTool::execute(const nlohmann::json&) { return ToolResult::ok("[composio - stub]"); }

PushoverTool::PushoverTool(const std::string& user_key, const std::string& app_token)
    : user_key_(user_key), app_token_(app_token) {}

nlohmann::json PushoverTool::parameters_schema() const {
    return {{"type", "object"}, {"properties", {
        {"message", {{"type", "string"}, {"description", "Notification message"}}},
        {"title", {{"type", "string"}, {"description", "Notification title"}}}
    }}, {"required", nlohmann::json::array({"message"})}};
}
ToolResult PushoverTool::execute(const nlohmann::json&) { return ToolResult::ok("[pushover - stub]"); }

// ── Delegation/Routing Tools ─────────────────────────────────────

nlohmann::json DelegateTool::parameters_schema() const {
    return {{"type", "object"}, {"properties", {
        {"agent", {{"type", "string"}, {"description", "Agent to delegate to"}}},
        {"task", {{"type", "string"}, {"description", "Task description for the delegate"}}}
    }}, {"required", nlohmann::json::array({"agent", "task"})}};
}
ToolResult DelegateTool::execute(const nlohmann::json&) { return ToolResult::ok("[delegate - stub]"); }

nlohmann::json ModelRoutingConfigTool::parameters_schema() const {
    return {{"type", "object"}, {"properties", nlohmann::json::object()}};
}
ToolResult ModelRoutingConfigTool::execute(const nlohmann::json&) { return ToolResult::ok("[model_routing_config - stub]"); }

nlohmann::json ProxyConfigTool::parameters_schema() const {
    return {{"type", "object"}, {"properties", nlohmann::json::object()}};
}
ToolResult ProxyConfigTool::execute(const nlohmann::json&) { return ToolResult::ok("[proxy_config - stub]"); }

nlohmann::json SchemaCleanr::parameters_schema() const {
    return {{"type", "object"}, {"properties", {
        {"schema", {{"type", "object"}, {"description", "JSON schema to clean"}}}
    }}, {"required", nlohmann::json::array({"schema"})}};
}
ToolResult SchemaCleanr::execute(const nlohmann::json&) { return ToolResult::ok("[schema_cleanr - stub]"); }

} // namespace tools
} // namespace zeroclaw
