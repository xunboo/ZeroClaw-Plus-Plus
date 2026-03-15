#pragma once

/// All remaining tool implementations — each follows the Tool interface pattern.
/// Tools are grouped by functionality: search, memory, web, hardware, scheduling, etc.

#include <string>
#include <vector>
#include <memory>
#include "traits.hpp"
#include "nlohmann/json.hpp"

namespace zeroclaw {
namespace security { class SecurityPolicy; }
namespace tools {

// ── Search Tools ─────────────────────────────────────────────────

/// Content search tool — search file contents with pattern matching
class ContentSearchTool : public Tool {
public:
    explicit ContentSearchTool(std::shared_ptr<security::SecurityPolicy> security);
    std::string name() const override { return "content_search"; }
    std::string description() const override {
        return "Search for a pattern in file contents within the workspace using ripgrep-style matching.";
    }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
private:
    std::shared_ptr<security::SecurityPolicy> security_;
};

/// Glob search tool — find files matching a glob pattern
class GlobSearchTool : public Tool {
public:
    explicit GlobSearchTool(std::shared_ptr<security::SecurityPolicy> security);
    std::string name() const override { return "glob_search"; }
    std::string description() const override {
        return "Find files matching a glob pattern within the workspace directory.";
    }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
private:
    std::shared_ptr<security::SecurityPolicy> security_;
};

/// CLI discovery tool — list and describe available CLI commands
class CliDiscoveryTool : public Tool {
public:
    std::string name() const override { return "cli_discovery"; }
    std::string description() const override {
        return "Discover available CLI commands and their usage.";
    }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

// ── Memory Tools ─────────────────────────────────────────────────

/// Memory store tool — save information to long-term memory
class MemoryStoreTool : public Tool {
public:
    std::string name() const override { return "memory_store"; }
    std::string description() const override {
        return "Store a key-value pair in long-term memory for later recall.";
    }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

/// Memory recall tool — retrieve information from long-term memory
class MemoryRecallTool : public Tool {
public:
    std::string name() const override { return "memory_recall"; }
    std::string description() const override {
        return "Recall information from long-term memory by searching for a query.";
    }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

/// Memory forget tool — remove information from long-term memory
class MemoryForgetTool : public Tool {
public:
    std::string name() const override { return "memory_forget"; }
    std::string description() const override {
        return "Remove a specific entry from long-term memory.";
    }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

// ── Web Tools ────────────────────────────────────────────────────

/// Web search tool — search the web using configured search provider
class WebSearchTool : public Tool {
public:
    explicit WebSearchTool(const std::string& api_key = "",
                            const std::string& engine = "google");
    std::string name() const override { return "web_search"; }
    std::string description() const override {
        return "Search the web for information. Returns summarized search results.";
    }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
private:
    std::string api_key_;
    std::string engine_;
};

/// HTTP request tool — make HTTP requests
class HttpRequestTool : public Tool {
public:
    explicit HttpRequestTool(bool allow_external = false);
    std::string name() const override { return "http_request"; }
    std::string description() const override {
        return "Make an HTTP request to a URL and return the response.";
    }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
private:
    bool allow_external_;
};

// ── Browser Tools ────────────────────────────────────────────────

/// Browser tool — headless browser interaction for web pages
class BrowserTool : public Tool {
public:
    std::string name() const override { return "browser"; }
    std::string description() const override {
        return "Interact with a headless browser to navigate, click, type, and extract "
               "content from web pages.";
    }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

/// Browser open tool — open a URL in the default browser
class BrowserOpenTool : public Tool {
public:
    std::string name() const override { return "browser_open"; }
    std::string description() const override {
        return "Open a URL in the user's default browser.";
    }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

/// Screenshot tool — capture screenshots
class ScreenshotTool : public Tool {
public:
    std::string name() const override { return "screenshot"; }
    std::string description() const override {
        return "Capture a screenshot of the current screen or a specific window.";
    }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

// ── Git Tools ────────────────────────────────────────────────────

/// Git operations tool — common git operations
class GitOperationsTool : public Tool {
public:
    explicit GitOperationsTool(std::shared_ptr<security::SecurityPolicy> security);
    std::string name() const override { return "git_operations"; }
    std::string description() const override {
        return "Perform git operations: status, diff, log, branch, commit, etc.";
    }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
private:
    std::shared_ptr<security::SecurityPolicy> security_;
};

// ── Scheduling/Cron Tools ────────────────────────────────────────

/// Cron add tool — schedule a recurring task
class CronAddTool : public Tool {
public:
    std::string name() const override { return "cron_add"; }
    std::string description() const override { return "Schedule a recurring task with a cron expression."; }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

/// Cron list tool — list scheduled tasks
class CronListTool : public Tool {
public:
    std::string name() const override { return "cron_list"; }
    std::string description() const override { return "List all scheduled recurring tasks."; }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

/// Cron remove tool — remove a scheduled task
class CronRemoveTool : public Tool {
public:
    std::string name() const override { return "cron_remove"; }
    std::string description() const override { return "Remove a scheduled recurring task by ID."; }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

/// Cron run tool — manually trigger a scheduled task
class CronRunTool : public Tool {
public:
    std::string name() const override { return "cron_run"; }
    std::string description() const override { return "Manually trigger a scheduled task by ID."; }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

/// Cron runs tool — list recent runs of scheduled tasks
class CronRunsTool : public Tool {
public:
    std::string name() const override { return "cron_runs"; }
    std::string description() const override { return "List recent execution history of scheduled tasks."; }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

/// Cron update tool — update a scheduled task
class CronUpdateTool : public Tool {
public:
    std::string name() const override { return "cron_update"; }
    std::string description() const override { return "Update an existing scheduled task."; }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

/// Schedule tool — one-time future task scheduling
class ScheduleTool : public Tool {
public:
    std::string name() const override { return "schedule"; }
    std::string description() const override { return "Schedule a one-time task for future execution."; }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

// ── Hardware Tools ───────────────────────────────────────────────

/// Hardware board info tool — query hardware board information
class HardwareBoardInfoTool : public Tool {
public:
    std::string name() const override { return "hardware_board_info"; }
    std::string description() const override { return "Query hardware board information (SoC, peripherals, etc)."; }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

/// Hardware memory map tool — query hardware memory map
class HardwareMemoryMapTool : public Tool {
public:
    std::string name() const override { return "hardware_memory_map"; }
    std::string description() const override { return "Query and display hardware memory map layout."; }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

/// Hardware memory read tool — read hardware memory regions
class HardwareMemoryReadTool : public Tool {
public:
    std::string name() const override { return "hardware_memory_read"; }
    std::string description() const override { return "Read data from a hardware memory region."; }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

// ── Media Tools ──────────────────────────────────────────────────

/// Image info tool — get metadata from image files
class ImageInfoTool : public Tool {
public:
    std::string name() const override { return "image_info"; }
    std::string description() const override { return "Get metadata and dimensions from an image file."; }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

/// PDF read tool — extract text from PDF files
class PdfReadTool : public Tool {
public:
    std::string name() const override { return "pdf_read"; }
    std::string description() const override { return "Extract text content from a PDF file."; }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

// ── Integration Tools ────────────────────────────────────────────

/// Composio tool — interact with Composio integrations
class ComposioTool : public Tool {
public:
    ComposioTool(const std::string& api_key, const std::string& entity_id);
    std::string name() const override { return "composio"; }
    std::string description() const override { return "Execute actions via Composio integrations."; }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
private:
    std::string api_key_;
    std::string entity_id_;
};

/// Pushover notification tool
class PushoverTool : public Tool {
public:
    PushoverTool(const std::string& user_key, const std::string& app_token);
    std::string name() const override { return "pushover"; }
    std::string description() const override { return "Send push notifications via Pushover."; }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
private:
    std::string user_key_;
    std::string app_token_;
};

// ── Delegation/Routing Tools ─────────────────────────────────────

/// Delegate tool — delegate a sub-task to another agent
class DelegateTool : public Tool {
public:
    std::string name() const override { return "delegate"; }
    std::string description() const override {
        return "Delegate a sub-task to a specialized agent.";
    }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

/// Model routing config tool — configure model routing
class ModelRoutingConfigTool : public Tool {
public:
    std::string name() const override { return "model_routing_config"; }
    std::string description() const override {
        return "View or modify the model routing configuration.";
    }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

/// Proxy config tool — configure proxy settings
class ProxyConfigTool : public Tool {
public:
    std::string name() const override { return "proxy_config"; }
    std::string description() const override { return "View or modify proxy settings."; }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

/// Schema cleanr tool — clean/optimize JSON schemas
class SchemaCleanr : public Tool {
public:
    std::string name() const override { return "schema_cleanr"; }
    std::string description() const override { return "Clean and optimize JSON schemas for tools."; }
    nlohmann::json parameters_schema() const override;
    ToolResult execute(const nlohmann::json& args) override;
};

} // namespace tools
} // namespace zeroclaw
