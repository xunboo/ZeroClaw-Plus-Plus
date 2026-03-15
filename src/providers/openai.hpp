#pragma once

/// OpenAI provider — talks to the OpenAI chat completions API.

#include <string>
#include <vector>
#include <optional>
#include "traits.hpp"
#include "nlohmann/json.hpp"

namespace zeroclaw {
namespace providers {

/// OpenAI-specific request/response types for the /v1/chat/completions API.

struct OpenAiMessage {
    std::string role;
    std::string content;
    std::optional<std::string> name;
    std::optional<std::vector<nlohmann::json>> tool_calls;
    std::optional<std::string> tool_call_id;
};

struct OpenAiUsage {
    int prompt_tokens = 0;
    int completion_tokens = 0;
    int total_tokens = 0;
};

/// OpenAI provider implementation
class OpenAiProvider : public Provider {
public:
    /// Create with optional API key credential
    explicit OpenAiProvider(const std::optional<std::string>& credential = std::nullopt);

    /// Create with optional custom base URL (default: https://api.openai.com/v1)
    static OpenAiProvider with_base_url(const std::optional<std::string>& base_url,
                                         const std::optional<std::string>& credential);

    std::string name() const { return "openai"; }

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
    /// Convert ChatMessage to OpenAI API format
    static nlohmann::json to_api_message(const ChatMessage& msg);
    /// Convert native tool calls from response
    static std::vector<ToolCall> parse_tool_calls(const nlohmann::json& choices);
    /// Convert ToolSpec to OpenAI function format
    static nlohmann::json to_native_tool_spec(const ToolSpec& tool);

    std::string api_key_;
    std::string base_url_ = "https://api.openai.com/v1";
};

} // namespace providers
} // namespace zeroclaw
