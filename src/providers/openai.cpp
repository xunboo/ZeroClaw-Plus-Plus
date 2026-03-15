#include "openai.hpp"
#include "../http/http_client.hpp"
#include "../tools/traits.hpp"

namespace zeroclaw {
namespace providers {

OpenAiProvider::OpenAiProvider(const std::optional<std::string>& credential)
    : api_key_(credential.value_or("")) {}

OpenAiProvider OpenAiProvider::with_base_url(const std::optional<std::string>& base_url,
                                               const std::optional<std::string>& credential) {
    OpenAiProvider p(credential);
    if (base_url.has_value() && !base_url->empty()) {
        p.base_url_ = *base_url;
    }
    return p;
}

nlohmann::json OpenAiProvider::to_api_message(const ChatMessage& msg) {
    nlohmann::json j;
    j["role"] = msg.role;
    j["content"] = msg.content;
    j["content"] = msg.content;
    if (!msg.tool_calls.empty()) {
        nlohmann::json calls = nlohmann::json::array();
        for (const auto& tc : msg.tool_calls) {
            calls.push_back({
                {"id", tc.id},
                {"type", "function"},
                {"function", {{"name", tc.name}, {"arguments", tc.arguments}}}
            });
        }
        j["tool_calls"] = calls;
    }
    if (!msg.tool_call_id.empty()) j["tool_call_id"] = msg.tool_call_id;
    return j;
}

std::vector<ToolCall> OpenAiProvider::parse_tool_calls(const nlohmann::json& choices) {
    std::vector<ToolCall> calls;
    if (!choices.is_array() || choices.empty()) return calls;

    const auto& message = choices[0].value("message", nlohmann::json::object());
    if (!message.contains("tool_calls")) return calls;

    for (const auto& tc : message["tool_calls"]) {
        ToolCall call;
        call.id = tc.value("id", "");
        if (tc.contains("function")) {
            call.name = tc["function"].value("name", "");
            auto args = tc["function"].value("arguments", "");
            call.arguments = args;
        }
        calls.push_back(call);
    }
    return calls;
}

nlohmann::json OpenAiProvider::to_native_tool_spec(const ToolSpec& t) {
    return {
        {"type", "function"},
        {"function", {
            {"name", t.name},
            {"description", t.description},
            {"parameters", t.parameters}
        }}
    };
}

ChatResponse OpenAiProvider::chat(const ChatRequest& request,
                                    const std::string& model,
                                    double temperature) {
    // Build request body
    nlohmann::json messages = nlohmann::json::array();
    if (request.messages) {
        for (const auto& msg : *request.messages) {
            messages.push_back(to_api_message(msg));
        }
    }

    nlohmann::json body = {
        {"model", model},
        {"temperature", temperature},
        {"messages", messages}
    };

    if (request.tools && !request.tools->empty()) {
        nlohmann::json tools_json = nlohmann::json::array();
        for (const auto& t : *request.tools) {
            tools_json.push_back(to_native_tool_spec(t));
        }
        body["tools"] = tools_json;
    }

    // HTTP POST to chat completions endpoint
    http::HttpClient client;
    client.with_bearer_token(api_key_);

    std::string url = base_url_ + "/chat/completions";
    auto resp = client.post_json(url, body);

    ChatResponse response;
    if (!resp.ok()) {
        response.text = "[OpenAI error] " + resp.error_message();
        return response;
    }

    // Parse the response
    auto json_resp = resp.json();
    if (json_resp.contains("choices") && !json_resp["choices"].empty()) {
        auto& first_choice = json_resp["choices"][0];
        if (first_choice.contains("message")) {
            auto& msg = first_choice["message"];
            response.text = msg.value("content", "");

            // Parse tool calls if present
            if (msg.contains("tool_calls")) {
                response.tool_calls = parse_tool_calls(json_resp["choices"]);
            }
        }


    }

    // Parse usage
    if (json_resp.contains("usage")) {
        auto& usage = json_resp["usage"];
        TokenUsage tu;
        tu.input_tokens = usage.value("prompt_tokens", 0);
        tu.output_tokens = usage.value("completion_tokens", 0);
        response.usage = tu;
    }



    return response;
}

ChatResponse OpenAiProvider::chat_with_tools(const std::vector<ChatMessage>& messages,
                                               const std::vector<ToolSpec>& tools,
                                               const std::string& model,
                                               double temperature) {
    ChatRequest request;
    request.messages = &messages;
    request.tools = &tools;
    return chat(request, model, temperature);
}

void OpenAiProvider::warmup() {
    if (api_key_.empty()) return;

    http::HttpClient client;
    client.with_bearer_token(api_key_);

    std::string url = base_url_ + "/models";
    client.get(url);
}

std::string OpenAiProvider::chat_with_system(
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
