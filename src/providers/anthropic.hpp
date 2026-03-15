#pragma once

/// Anthropic provider — talks to the Anthropic Messages API.

#include <string>
#include <vector>
#include <optional>
#include "traits.hpp"
#include "nlohmann/json.hpp"

namespace zeroclaw {
namespace providers {

/// Content block types for Anthropic API
enum class ContentBlockType { Text, ToolUse, ToolResult };

/// Anthropic provider implementation
class AnthropicProvider : public Provider {
public:
    explicit AnthropicProvider(const std::optional<std::string>& credential = std::nullopt);

    static AnthropicProvider with_base_url(const std::optional<std::string>& credential,
                                            const std::optional<std::string>& base_url);

    std::string name() const { return "anthropic"; }

    ChatResponse chat(const ChatRequest& request,
                       const std::string& model,
                       double temperature) override;

    ChatResponse chat_with_tools(const std::vector<ChatMessage>& messages,
                                   const std::vector<ToolSpec>& tools,
                                   const std::string& model,
                                   double temperature) override;

    std::string chat_with_system(const std::optional<std::string>& system_prompt,
                                 const std::string& message,
                                 const std::string& model,
                                 double temperature) override;

    void warmup() override;

private:
    // Anthropic uses a different message format from OpenAI
    static nlohmann::json to_api_messages(const std::vector<ChatMessage>& messages);
    static nlohmann::json to_api_system_prompt(const std::string& system, bool should_cache);
    static nlohmann::json to_api_tools(const std::vector<ToolSpec>& tools);
    static std::vector<ToolCall> parse_content_blocks(const nlohmann::json& content);

    /// Check if system prompt is large enough to warrant caching
    static bool should_cache_system(const std::string& text);
    /// Apply cache markers to conversation messages
    static void apply_cache_to_last_message(nlohmann::json& messages);

    std::string api_key_;
    std::string base_url_ = "https://api.anthropic.com";
    static constexpr const char* API_VERSION = "2023-06-01";
};

} // namespace providers
} // namespace zeroclaw
