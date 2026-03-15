#include "traits.hpp"
#include "../tools/traits.hpp"
#include <sstream>

namespace zeroclaw {
namespace providers {

// ── Provider default implementations ─────────────────────────────

ToolsPayload Provider::convert_tools(const std::vector<ToolSpec>& tools) const {
    return ToolsPayload::prompt_guided(build_tool_instructions_text(tools));
}

std::string Provider::chat_with_history(
    const std::vector<ChatMessage>& messages,
    const std::string& model,
    double temperature) {
    // Default: extract system prompt and last user message
    std::optional<std::string> system_prompt;
    std::string last_user;

    for (const auto& msg : messages) {
        if (msg.role == "system") {
            system_prompt = msg.content;
        }
    }
    for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
        if (it->role == "user") {
            last_user = it->content;
            break;
        }
    }

    return chat_with_system(system_prompt, last_user, model, temperature);
}

ChatResponse Provider::chat(
    const ChatRequest& request,
    const std::string& model,
    double temperature) {
    // If tools are provided but provider doesn't support native tools,
    // inject tool instructions into system prompt as fallback.
    if (request.tools && !request.tools->empty() && !supports_native_tools()) {
        auto payload = convert_tools(*request.tools);
        if (payload.type != ToolsPayload::Type::PromptGuided) {
            // Error: non-prompt-guided payload from non-native provider
            ChatResponse resp;
            resp.text = "Error: provider returned non-prompt-guided tools payload while supports_native_tools() is false";
            return resp;
        }

        auto modified_messages = *request.messages;

        // Inject tool instructions into existing system message or prepend one
        bool found_system = false;
        for (auto& msg : modified_messages) {
            if (msg.role == "system") {
                if (!msg.content.empty()) {
                    msg.content += "\n\n";
                }
                msg.content += payload.instructions;
                found_system = true;
                break;
            }
        }
        if (!found_system) {
            modified_messages.insert(modified_messages.begin(),
                                     ChatMessage::system(payload.instructions));
        }

        auto text = chat_with_history(modified_messages, model, temperature);
        ChatResponse resp;
        resp.text = text;
        return resp;
    }

    auto text = chat_with_history(
        request.messages ? *request.messages : std::vector<ChatMessage>{},
        model, temperature);
    ChatResponse resp;
    resp.text = text;
    return resp;
}

ChatResponse Provider::chat_with_tools(
    const std::vector<ChatMessage>& messages,
    const std::vector<ToolSpec>& /*tools*/,
    const std::string& model,
    double temperature) {
    auto text = chat_with_history(messages, model, temperature);
    ChatResponse resp;
    resp.text = text;
    return resp;
}

// ── Helper functions ─────────────────────────────────────────────

std::string build_tool_instructions_text(const std::vector<ToolSpec>& tools) {
    std::ostringstream oss;

    oss << "## Tool Use Protocol\n\n";
    oss << "To use a tool, wrap a JSON object in <tool_call></tool_call> tags:\n\n";
    oss << "<tool_call>\n";
    oss << R"({"name": "tool_name", "arguments": {"param": "value"}})";
    oss << "\n</tool_call>\n\n";
    oss << "You may use multiple tool calls in a single response. ";
    oss << "After tool execution, results appear in <tool_result> tags. ";
    oss << "Continue reasoning with the results until you can give a final answer.\n\n";
    oss << "### Available Tools\n\n";

    for (const auto& tool : tools) {
        oss << "**" << tool.name << "**: " << tool.description << "\n";
        oss << "Parameters: `" << tool.parameters.dump() << "`\n\n";
    }

    return oss.str();
}

} // namespace providers
} // namespace zeroclaw
