#include "tools_module.hpp"

namespace zeroclaw {
namespace tools {

std::vector<std::unique_ptr<Tool>> default_tools(
    std::shared_ptr<security::SecurityPolicy> security) {
    std::vector<std::unique_ptr<Tool>> tools;

    // Core file tools
    tools.push_back(std::make_unique<ShellTool>(security));
    tools.push_back(std::make_unique<FileReadTool>(security));
    tools.push_back(std::make_unique<FileWriteTool>(security));
    tools.push_back(std::make_unique<FileEditTool>(security));

    // Search tools
    tools.push_back(std::make_unique<ContentSearchTool>(security));
    tools.push_back(std::make_unique<GlobSearchTool>(security));

    // Git
    tools.push_back(std::make_unique<GitOperationsTool>(security));

    // Web
    tools.push_back(std::make_unique<WebSearchTool>());
    tools.push_back(std::make_unique<HttpRequestTool>());
    tools.push_back(std::make_unique<BrowserOpenTool>());

    // Memory
    tools.push_back(std::make_unique<MemoryStoreTool>());
    tools.push_back(std::make_unique<MemoryRecallTool>());
    tools.push_back(std::make_unique<MemoryForgetTool>());

    // Delegation
    tools.push_back(std::make_unique<DelegateTool>());

    // Cron
    tools.push_back(std::make_unique<CronAddTool>());
    tools.push_back(std::make_unique<CronListTool>());
    tools.push_back(std::make_unique<CronRemoveTool>());
    tools.push_back(std::make_unique<CronRunTool>());
    tools.push_back(std::make_unique<CronRunsTool>());
    tools.push_back(std::make_unique<CronUpdateTool>());
    tools.push_back(std::make_unique<ScheduleTool>());

    // Media
    tools.push_back(std::make_unique<ImageInfoTool>());
    tools.push_back(std::make_unique<PdfReadTool>());

    // Hardware
    tools.push_back(std::make_unique<HardwareBoardInfoTool>());
    tools.push_back(std::make_unique<HardwareMemoryMapTool>());
    tools.push_back(std::make_unique<HardwareMemoryReadTool>());

    // Config tools
    tools.push_back(std::make_unique<ModelRoutingConfigTool>());
    tools.push_back(std::make_unique<ProxyConfigTool>());

    return tools;
}

std::vector<std::unique_ptr<Tool>> all_tools(
    std::shared_ptr<security::SecurityPolicy> security,
    bool enable_browser,
    bool enable_composio,
    const std::string& composio_key,
    const std::string& composio_entity) {

    auto tools = default_tools(security);

    if (enable_browser) {
        tools.push_back(std::make_unique<BrowserTool>());
        tools.push_back(std::make_unique<ScreenshotTool>());
    }

    if (enable_composio && !composio_key.empty()) {
        tools.push_back(std::make_unique<ComposioTool>(composio_key, composio_entity));
    }

    tools.push_back(std::make_unique<CliDiscoveryTool>());

    return tools;
}

} // namespace tools
} // namespace zeroclaw
