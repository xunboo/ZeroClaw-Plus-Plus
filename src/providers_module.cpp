#include "providers_module.hpp"
#include <algorithm>

namespace zeroclaw {
namespace providers {

std::vector<ProviderInfo> list_providers() {
    return {
        {"openai",       {"gpt", "chatgpt"}},
        {"anthropic",    {"claude"}},
        {"gemini",       {"google"}},
        {"ollama",       {"local"}},
        {"openrouter",   {"or"}},
        {"bedrock",      {"aws"}},
        {"copilot",      {"github-copilot"}},
        {"openai_codex", {"codex"}},
        {"glm",          {"chatglm", "zhipu", "zai"}},
        {"telnyx",       {}},
    };
}

std::unique_ptr<Provider> create_provider(
    const std::string& name,
    const std::optional<std::string>& api_key,
    const std::optional<std::string>& base_url) {

    // Normalize to lowercase
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Resolve aliases
    auto providers = list_providers();
    std::string resolved = lower;
    for (const auto& info : providers) {
        if (info.name == lower) break;
        for (const auto& alias : info.aliases) {
            std::string lower_alias = alias;
            std::transform(lower_alias.begin(), lower_alias.end(), lower_alias.begin(), ::tolower);
            if (lower_alias == lower) {
                resolved = info.name;
                break;
            }
        }
    }

    if (resolved == "openai") {
        return std::make_unique<OpenAiProvider>(
            OpenAiProvider::with_base_url(base_url, api_key));
    }
    if (resolved == "anthropic") {
        return std::make_unique<AnthropicProvider>(
            AnthropicProvider::with_base_url(api_key, base_url));
    }
    if (resolved == "gemini") {
        return std::make_unique<GeminiProvider>(
            GeminiProvider::with_base_url(api_key, base_url));
    }
    if (resolved == "ollama") {
        return std::make_unique<OllamaProvider>(base_url);
    }
    if (resolved == "bedrock") {
        return std::make_unique<BedrockProvider>();
    }
    if (resolved == "openrouter") {
        return std::make_unique<OpenRouterProvider>(api_key);
    }
    if (resolved == "copilot") {
        return std::make_unique<CopilotProvider>(api_key);
    }
    if (resolved == "openai_codex") {
        return std::make_unique<OpenAiCodexProvider>(api_key);
    }
    if (resolved == "glm") {
        std::string bu = base_url.value_or("https://open.bigmodel.cn/api/coding/paas/v4");
        return std::make_unique<GlmProvider>(api_key, bu);
    }
    if (resolved == "telnyx") {
        return std::make_unique<TelnyxProvider>(api_key);
    }

    // Fallback: OpenAI-compatible
    return std::make_unique<CompatibleProvider>(
        resolved,
        base_url.value_or("https://api.openai.com/v1"),
        api_key,
        AuthStyle::Bearer);
}

std::unique_ptr<Provider> create_resilient_provider(
    const std::string& name,
    const std::optional<std::string>& api_key,
    const std::optional<std::string>& base_url,
    int max_retries) {
    auto inner = create_provider(name, api_key, base_url);
    return std::make_unique<ReliableProvider>(std::move(inner), max_retries);
}

} // namespace providers
} // namespace zeroclaw
