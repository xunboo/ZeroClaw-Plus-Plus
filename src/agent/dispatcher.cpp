#include "dispatcher.hpp"
#include <sstream>

namespace zeroclaw {
namespace agent {

// ── XmlToolDispatcher ────────────────────────────────────────────

std::pair<std::string, std::vector<ParsedToolCall>>
XmlToolDispatcher::parse_xml_tool_calls(const std::string& response) {
    std::vector<std::string> text_parts;
    std::vector<ParsedToolCall> calls;

    std::string remaining = response;
    while (true) {
        auto start_pos = remaining.find("<tool_call>");
        if (start_pos == std::string::npos) break;

        auto before = remaining.substr(0, start_pos);
        // Trim
        size_t s = before.find_first_not_of(" \t\n\r");
        size_t e = before.find_last_not_of(" \t\n\r");
        if (s != std::string::npos) {
            text_parts.push_back(before.substr(s, e - s + 1));
        }

        auto end_pos = remaining.find("</tool_call>", start_pos);
        if (end_pos == std::string::npos) break;

        auto inner = remaining.substr(start_pos + 11, end_pos - start_pos - 11);
        // Trim inner
        size_t is = inner.find_first_not_of(" \t\n\r");
        size_t ie = inner.find_last_not_of(" \t\n\r");
        if (is != std::string::npos) {
            inner = inner.substr(is, ie - is + 1);
        }

        try {
            auto parsed = nlohmann::json::parse(inner);
            std::string name;
            if (parsed.contains("name") && parsed["name"].is_string()) {
                name = parsed["name"].get<std::string>();
            }
            if (!name.empty()) {
                nlohmann::json arguments = nlohmann::json::object();
                if (parsed.contains("arguments")) {
                    arguments = parsed["arguments"];
                }
                calls.push_back({name, arguments, std::nullopt});
            }
        } catch (...) {
            // Malformed JSON, skip
        }

        remaining = remaining.substr(end_pos + 12);
    }

    // Add remaining text
    size_t rs = remaining.find_first_not_of(" \t\n\r");
    size_t re = remaining.find_last_not_of(" \t\n\r");
    if (rs != std::string::npos) {
        text_parts.push_back(remaining.substr(rs, re - rs + 1));
    }

    std::string text;
    for (size_t i = 0; i < text_parts.size(); ++i) {
        if (i > 0) text += "\n";
        text += text_parts[i];
    }

    return {text, calls};
}

std::pair<std::string, std::vector<ParsedToolCall>>
XmlToolDispatcher::parse_response(const ChatResponse& response) const {
    return parse_xml_tool_calls(response.text_or_empty());
}

ConversationMessage
XmlToolDispatcher::format_results(const std::vector<ToolExecutionResult>& results) const {
    std::ostringstream oss;
    for (const auto& result : results) {
        std::string status = result.success ? "ok" : "error";
        oss << "<tool_result name=\"" << result.name << "\" status=\"" << status << "\">\n"
            << result.output << "\n</tool_result>\n";
    }
    return ConversationMessage::make_chat(
        ChatMessage::user("[Tool results]\n" + oss.str()));
}

std::string
XmlToolDispatcher::prompt_instructions(const std::vector<std::unique_ptr<Tool>>& tools) const {
    std::ostringstream oss;
    oss << "## Tool Use Protocol\n\n";
    oss << "To use a tool, wrap a JSON object in <tool_call></tool_call> tags:\n\n";
    oss << "```\n<tool_call>\n{\"name\": \"tool_name\", \"arguments\": {\"param\": \"value\"}}\n</tool_call>\n```\n\n";
    oss << "### Available Tools\n\n";

    for (const auto& tool : tools) {
        oss << "- **" << tool->name() << "**: " << tool->description()
            << "\n  Parameters: `" << tool->parameters_schema().dump() << "`\n";
    }

    return oss.str();
}

std::vector<ChatMessage>
XmlToolDispatcher::to_provider_messages(const std::vector<ConversationMessage>& history) const {
    std::vector<ChatMessage> messages;
    for (const auto& msg : history) {
        switch (msg.type) {
            case ConversationMessage::Type::Chat:
                if (msg.chat.has_value()) {
                    messages.push_back(*msg.chat);
                }
                break;
            case ConversationMessage::Type::AssistantToolCalls:
                messages.push_back(
                    ChatMessage::assistant(msg.assistant_text.value_or("")));
                break;
            case ConversationMessage::Type::ToolResults: {
                std::ostringstream oss;
                for (const auto& result : msg.tool_results) {
                    oss << "<tool_result id=\"" << result.tool_call_id << "\">\n"
                        << result.content << "\n</tool_result>\n";
                }
                messages.push_back(ChatMessage::user("[Tool results]\n" + oss.str()));
                break;
            }
        }
    }
    return messages;
}

// ── NativeToolDispatcher ─────────────────────────────────────────

std::pair<std::string, std::vector<ParsedToolCall>>
NativeToolDispatcher::parse_response(const ChatResponse& response) const {
    std::string text = response.text.value_or("");
    std::vector<ParsedToolCall> calls;
    for (const auto& tc : response.tool_calls) {
        nlohmann::json args;
        try {
            args = nlohmann::json::parse(tc.arguments);
        } catch (...) {
            args = nlohmann::json::object();
        }
        calls.push_back({tc.name, args, tc.id});
    }
    return {text, calls};
}

ConversationMessage
NativeToolDispatcher::format_results(const std::vector<ToolExecutionResult>& results) const {
    std::vector<ToolResultMessage> messages;
    for (const auto& result : results) {
        messages.push_back({
            result.tool_call_id.value_or("unknown"),
            result.output
        });
    }
    return ConversationMessage::make_tool_results(messages);
}

std::string
NativeToolDispatcher::prompt_instructions(const std::vector<std::unique_ptr<Tool>>& /*tools*/) const {
    return "";
}

std::vector<ChatMessage>
NativeToolDispatcher::to_provider_messages(const std::vector<ConversationMessage>& history) const {
    std::vector<ChatMessage> messages;
    for (const auto& msg : history) {
        switch (msg.type) {
            case ConversationMessage::Type::Chat:
                if (msg.chat.has_value()) {
                    messages.push_back(*msg.chat);
                }
                break;
            case ConversationMessage::Type::AssistantToolCalls: {
                nlohmann::json payload;
                payload["content"] = msg.assistant_text.value_or("");
                nlohmann::json tc_array = nlohmann::json::array();
                for (const auto& tc : msg.assistant_tool_calls) {
                    tc_array.push_back(tc.to_json());
                }
                payload["tool_calls"] = tc_array;
                if (msg.reasoning_content.has_value()) {
                    payload["reasoning_content"] = *msg.reasoning_content;
                }
                messages.push_back(ChatMessage::assistant(payload.dump()));
                break;
            }
            case ConversationMessage::Type::ToolResults:
                for (const auto& result : msg.tool_results) {
                    nlohmann::json j;
                    j["tool_call_id"] = result.tool_call_id;
                    j["content"] = result.content;
                    messages.push_back(ChatMessage::tool(j.dump()));
                }
                break;
        }
    }
    return messages;
}

} // namespace agent
} // namespace zeroclaw
