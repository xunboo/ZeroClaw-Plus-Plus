#include "anthropic.hpp"
#include "../http/http_client.hpp"
#include "../tools/traits.hpp"

namespace zeroclaw {
namespace providers {

AnthropicProvider::AnthropicProvider(const std::optional<std::string>& credential)
    : api_key_(credential.value_or("")) {}

AnthropicProvider AnthropicProvider::with_base_url(
    const std::optional<std::string>& credential,
    const std::optional<std::string>& base_url) {
    AnthropicProvider p(credential);
    if (base_url.has_value() && !base_url->empty()) {
        p.base_url_ = *base_url;
    }
    return p;
}

bool AnthropicProvider::should_cache_system(const std::string& text) {
    return text.size() > 3072;  // ~1024 tokens
}

nlohmann::json AnthropicProvider::to_api_system_prompt(const std::string& system, bool cache) {
    if (cache) {
        return nlohmann::json::array({
            {{"type", "text"}, {"text", system},
             {"cache_control", {{"type", "ephemeral"}}}}
        });
    }
    return system;
}

nlohmann::json AnthropicProvider::to_api_messages(const std::vector<ChatMessage>& messages) {
    nlohmann::json result = nlohmann::json::array();
    for (const auto& msg : messages) {
        if (msg.role == "system") continue;  // System prompt handled separately

        nlohmann::json m;
        m["role"] = msg.role;

        if (msg.role == "assistant" && !msg.tool_calls.empty()) {
            nlohmann::json content = nlohmann::json::array();
            if (!msg.content.empty()) {
                content.push_back({{"type", "text"}, {"text", msg.content}});
            }
            for (const auto& tc : msg.tool_calls) {
                nlohmann::json args;
                try { args = nlohmann::json::parse(tc.arguments); }
                catch (...) { args = tc.arguments; }
                content.push_back({
                    {"type", "tool_use"}, {"id", tc.id},
                    {"name", tc.name}, {"input", args}
                });
            }
            m["content"] = content;
        } else if (!msg.tool_call_id.empty()) {
            m["role"] = "user";
            m["content"] = nlohmann::json::array({
                {{"type", "tool_result"}, {"tool_use_id", msg.tool_call_id},
                 {"content", msg.content}}
            });
        } else {
            m["content"] = msg.content;
        }

        result.push_back(m);
    }
    return result;
}

nlohmann::json AnthropicProvider::to_api_tools(const std::vector<ToolSpec>& tools) {
    nlohmann::json result = nlohmann::json::array();
    for (const auto& t : tools) {
        nlohmann::json spec;
        spec["name"] = t.name;
        spec["description"] = t.description;
        spec["input_schema"] = t.parameters;
        result.push_back(spec);
    }
    return result;
}

std::vector<ToolCall> AnthropicProvider::parse_content_blocks(const nlohmann::json& content) {
    std::vector<ToolCall> calls;
    if (!content.is_array()) return calls;

    for (const auto& block : content) {
        if (block.value("type", "") == "tool_use") {
            ToolCall tc;
            tc.id = block.value("id", "");
            tc.name = block.value("name", "");
            tc.arguments = block.value("input", nlohmann::json::object()).dump();
            calls.push_back(tc);
        }
    }
    return calls;
}

void AnthropicProvider::apply_cache_to_last_message(nlohmann::json& messages) {
    if (messages.empty() || !messages.is_array()) return;
    auto& last = messages.back();
    if (last.contains("content") && last["content"].is_array() && !last["content"].empty()) {
        last["content"].back()["cache_control"] = {{"type", "ephemeral"}};
    }
}

ChatResponse AnthropicProvider::chat(const ChatRequest& request,
                                       const std::string& model,
                                       double temperature) {
    std::string system_text;
    std::vector<ChatMessage> non_system;
    if (request.messages) {
        for (const auto& msg : *request.messages) {
            if (msg.role == "system") {
                system_text = msg.content;
            } else {
                non_system.push_back(msg);
            }
        }
    }

    auto api_messages = to_api_messages(non_system);
    bool cache_system = should_cache_system(system_text);

    nlohmann::json body = {
        {"model", model},
        {"max_tokens", 4096},
        {"temperature", temperature},
        {"messages", api_messages}
    };

    if (!system_text.empty()) {
        body["system"] = to_api_system_prompt(system_text, cache_system);
    }

    if (request.tools && !request.tools->empty()) {
        body["tools"] = to_api_tools(*request.tools);
    }

    // HTTP POST to Anthropic Messages API
    http::HttpClient client;
    client.with_api_key("x-api-key", api_key_);
    client.with_header("anthropic-version", "2023-06-01");
    if (cache_system) {
        client.with_header("anthropic-beta", "prompt-caching-2024-07-31");
    }

    std::string url = base_url_ + "/v1/messages";
    auto resp = client.post_json(url, body);

    ChatResponse response;
    if (!resp.ok()) {
        response.text = "[Anthropic error] " + resp.error_message();
        return response;
    }

    // Parse the response
    auto json_resp = resp.json();

    // Extract text content and tool calls from content blocks
    if (json_resp.contains("content") && json_resp["content"].is_array()) {
        std::string text_parts;
        for (const auto& block : json_resp["content"]) {
            auto block_type = block.value("type", "");
            if (block_type == "text") {
                if (!text_parts.empty()) text_parts += "\n";
                text_parts += block.value("text", "");
            }
        }
        response.text = text_parts;
        response.tool_calls = parse_content_blocks(json_resp["content"]);
    }

    // Parse usage
    if (json_resp.contains("usage")) {
        auto& usage = json_resp["usage"];
        TokenUsage tu;
        tu.input_tokens = usage.value("input_tokens", 0);
        tu.output_tokens = usage.value("output_tokens", 0);
        response.usage = tu;
    }

    return response;
}

ChatResponse AnthropicProvider::chat_with_tools(const std::vector<ChatMessage>& messages,
                                                  const std::vector<ToolSpec>& tools,
                                                  const std::string& model,
                                                  double temperature) {
    ChatRequest request;
    request.messages = &messages;
    request.tools = &tools;
    return chat(request, model, temperature);
}

void AnthropicProvider::warmup() {}

std::string AnthropicProvider::chat_with_system(
    const std::optional<std::string>& system_prompt,
    const std::string& message,
    const std::string& model,
    double temperature) {
    std::vector<ChatMessage> msgs;
    if (system_prompt.has_value() && !system_prompt->empty()) {
        msgs.push_back(ChatMessage::system(*system_prompt));
    }
    msgs.push_back(ChatMessage::user(message));
    ChatRequest req;
    req.messages = &msgs;
    return chat(req, model, temperature).text_or_empty();
}

} // namespace providers
} // namespace zeroclaw
