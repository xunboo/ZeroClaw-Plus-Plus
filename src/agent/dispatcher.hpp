#pragma once

/// Tool dispatcher — handles parsing LLM responses and formatting tool results.
/// Two implementations: XmlToolDispatcher (prompt-guided) and NativeToolDispatcher (API-native).

#include <string>
#include <vector>
#include <optional>
#include <sstream>
#include "../providers/traits.hpp"
#include "../tools/traits.hpp"
#include "nlohmann/json.hpp"

namespace zeroclaw {
namespace agent {

using providers::ChatMessage;
using providers::ChatResponse;
using providers::ConversationMessage;
using providers::ToolResultMessage;

/// Parsed tool call from LLM response
struct ParsedToolCall {
    std::string name;
    nlohmann::json arguments;
    std::optional<std::string> tool_call_id;
};

/// Result of executing a tool
struct ToolExecutionResult {
    std::string name;
    std::string output;
    bool success = false;
    std::optional<std::string> tool_call_id;
};

/// Abstract dispatcher interface
class ToolDispatcher {
public:
    virtual ~ToolDispatcher() = default;

    /// Parse an LLM response into text and tool calls
    virtual std::pair<std::string, std::vector<ParsedToolCall>>
    parse_response(const ChatResponse& response) const = 0;

    /// Format tool execution results into a conversation message
    virtual ConversationMessage
    format_results(const std::vector<ToolExecutionResult>& results) const = 0;

    /// Generate prompt instructions for tools
    virtual std::string prompt_instructions(const std::vector<std::unique_ptr<Tool>>& tools) const = 0;

    /// Convert conversation history to provider messages
    virtual std::vector<ChatMessage>
    to_provider_messages(const std::vector<ConversationMessage>& history) const = 0;

    /// Whether to send tool specs via the native API
    virtual bool should_send_tool_specs() const = 0;
};

/// XML-based tool dispatcher (prompt-guided tool calling)
class XmlToolDispatcher : public ToolDispatcher {
public:
    std::pair<std::string, std::vector<ParsedToolCall>>
    parse_response(const ChatResponse& response) const override;

    ConversationMessage
    format_results(const std::vector<ToolExecutionResult>& results) const override;

    std::string prompt_instructions(const std::vector<std::unique_ptr<Tool>>& tools) const override;

    std::vector<ChatMessage>
    to_provider_messages(const std::vector<ConversationMessage>& history) const override;

    bool should_send_tool_specs() const override { return false; }

    /// Parse XML tool calls from response text
    static std::pair<std::string, std::vector<ParsedToolCall>>
    parse_xml_tool_calls(const std::string& response);
};

/// Native API tool dispatcher (provider-native function calling)
class NativeToolDispatcher : public ToolDispatcher {
public:
    std::pair<std::string, std::vector<ParsedToolCall>>
    parse_response(const ChatResponse& response) const override;

    ConversationMessage
    format_results(const std::vector<ToolExecutionResult>& results) const override;

    std::string prompt_instructions(const std::vector<std::unique_ptr<Tool>>& tools) const override;

    std::vector<ChatMessage>
    to_provider_messages(const std::vector<ConversationMessage>& history) const override;

    bool should_send_tool_specs() const override { return true; }
};

} // namespace agent
} // namespace zeroclaw
