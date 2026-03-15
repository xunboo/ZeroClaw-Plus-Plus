#pragma once

/// Provider traits — core types for LLM provider interactions.
///
/// This module defines the unified interface for communicating with various LLM
/// providers (Anthropic, OpenAI, Gemini, Ollama, etc.). All conversation types,
/// tool calling abstractions, and streaming primitives live here.

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <memory>
#include <functional>
#include <variant>
#include "nlohmann/json.hpp"

namespace zeroclaw {

// Forward declaration (defined in tools/traits.hpp)
struct ToolSpec;

namespace providers {

// ── Core message types ───────────────────────────────────────────

/// A tool call requested by the LLM.
struct ToolCall {
    std::string id;
    std::string name;
    std::string arguments;

    nlohmann::json to_json() const {
        return {{"id", id}, {"name", name}, {"arguments", arguments}};
    }
    static ToolCall from_json(const nlohmann::json& j) {
        return {j.value("id", ""), j.value("name", ""), j.value("arguments", "")};
    }
};

/// A single message in a conversation.
struct ChatMessage {
    std::string role;
    std::string content;
    std::vector<ToolCall> tool_calls;
    std::string tool_call_id;

    // Convenience constructors
    static ChatMessage system(const std::string& content) { return {"system", content, {}, ""}; }
    static ChatMessage user(const std::string& content) { return {"user", content, {}, ""}; }
    static ChatMessage assistant(const std::string& content) { return {"assistant", content, {}, ""}; }
    static ChatMessage tool(const std::string& content) { return {"tool", content, {}, ""}; }

    // JSON serialization
    nlohmann::json to_json() const {
        nlohmann::json j = {{"role", role}, {"content", content}};
        if (!tool_calls.empty()) {
            nlohmann::json calls = nlohmann::json::array();
            for (const auto& tc : tool_calls) {
                calls.push_back(tc.to_json());
            }
            j["tool_calls"] = calls;
        }
        if (!tool_call_id.empty()) {
            j["tool_call_id"] = tool_call_id;
        }
        return j;
    }

    static ChatMessage from_json(const nlohmann::json& j) {
        ChatMessage msg = {j.value("role", ""), j.value("content", ""), {}, ""};
        if (j.contains("tool_calls") && j["tool_calls"].is_array()) {
            for (const auto& call_json : j["tool_calls"]) {
                msg.tool_calls.push_back(ToolCall::from_json(call_json));
            }
        }
        msg.tool_call_id = j.value("tool_call_id", "");
        return msg;
    }
};

/// Raw token counts from a single LLM API response.
struct TokenUsage {
    std::optional<uint64_t> input_tokens;
    std::optional<uint64_t> output_tokens;
};

/// An LLM response that may contain text, tool calls, or both.
struct ChatResponse {
    /// Text content of the response (may be empty if only tool calls).
    std::optional<std::string> text;
    /// Tool calls requested by the LLM.
    std::vector<ToolCall> tool_calls;
    /// Token usage reported by the provider, if available.
    std::optional<TokenUsage> usage;
    /// Raw reasoning/thinking content from thinking models (e.g. DeepSeek-R1,
    /// Kimi K2.5, GLM-4.7). Preserved as an opaque pass-through.
    std::optional<std::string> reasoning_content;

    /// True when the LLM wants to invoke at least one tool.
    bool has_tool_calls() const { return !tool_calls.empty(); }

    /// Convenience: return text content or empty string.
    const std::string& text_or_empty() const {
        static const std::string empty;
        return text.has_value() ? text.value() : empty;
    }
};

/// Request payload for provider chat calls.
struct ChatRequest {
    const std::vector<ChatMessage>* messages = nullptr;
    const std::vector<ToolSpec>* tools = nullptr;
};

/// A tool result to feed back to the LLM.
struct ToolResultMessage {
    std::string tool_call_id;
    std::string content;

    nlohmann::json to_json() const {
        return {{"tool_call_id", tool_call_id}, {"content", content}};
    }
};

// ── Conversation history types ───────────────────────────────────

/// A message in a multi-turn conversation, including tool interactions.
struct ConversationMessage {
    enum class Type { Chat, AssistantToolCalls, ToolResults };
    Type type;

    // Chat variant
    std::optional<ChatMessage> chat;

    // AssistantToolCalls variant
    std::optional<std::string> assistant_text;
    std::vector<ToolCall> assistant_tool_calls;
    std::optional<std::string> reasoning_content;

    // ToolResults variant
    std::vector<ToolResultMessage> tool_results;

    static ConversationMessage make_chat(const ChatMessage& msg) {
        ConversationMessage cm;
        cm.type = Type::Chat;
        cm.chat = msg;
        return cm;
    }
    static ConversationMessage make_tool_calls(
        const std::optional<std::string>& text,
        const std::vector<ToolCall>& calls,
        const std::optional<std::string>& reasoning = std::nullopt) {
        ConversationMessage cm;
        cm.type = Type::AssistantToolCalls;
        cm.assistant_text = text;
        cm.assistant_tool_calls = calls;
        cm.reasoning_content = reasoning;
        return cm;
    }
    static ConversationMessage make_tool_results(const std::vector<ToolResultMessage>& results) {
        ConversationMessage cm;
        cm.type = Type::ToolResults;
        cm.tool_results = results;
        return cm;
    }
};

// ── Streaming types ──────────────────────────────────────────────

/// A chunk of content from a streaming response.
struct StreamChunk {
    /// Text delta for this chunk.
    std::string delta;
    /// Whether this is the final chunk.
    bool is_final = false;
    /// Approximate token count for this chunk (estimated).
    size_t token_count = 0;

    /// Create a new non-final chunk.
    static StreamChunk make_delta(const std::string& text) {
        return {text, false, 0};
    }

    /// Create a final chunk.
    static StreamChunk final_chunk() {
        return {"", true, 0};
    }

    /// Create an error chunk.
    static StreamChunk error(const std::string& message) {
        return {message, true, 0};
    }

    /// Estimate tokens (rough approximation: ~4 chars per token).
    StreamChunk& with_token_estimate() {
        token_count = (delta.size() + 3) / 4;
        return *this;
    }
};

/// Options for streaming chat requests.
struct StreamOptions {
    /// Whether to enable streaming (default: true).
    bool enabled = false;
    /// Whether to include token counts in chunks.
    bool count_tokens = false;

    static StreamOptions make(bool enabled) {
        return {enabled, false};
    }
    StreamOptions& with_token_count() {
        count_tokens = true;
        return *this;
    }
};

/// Errors that can occur during streaming.
struct StreamError {
    enum class Type { Http, Json, InvalidSse, Provider, Io };
    Type type;
    std::string message;

    static StreamError http(const std::string& msg) { return {Type::Http, msg}; }
    static StreamError json(const std::string& msg) { return {Type::Json, msg}; }
    static StreamError invalid_sse(const std::string& msg) { return {Type::InvalidSse, msg}; }
    static StreamError provider(const std::string& msg) { return {Type::Provider, msg}; }
    static StreamError io(const std::string& msg) { return {Type::Io, msg}; }
};

/// Structured error returned when a requested capability is not supported.
struct ProviderCapabilityError {
    std::string provider;
    std::string capability;
    std::string message;

    std::string to_string() const {
        return "provider_capability_error provider=" + provider +
               " capability=" + capability + " message=" + message;
    }
};

// ── Provider capabilities ────────────────────────────────────────

/// Provider capabilities declaration.
struct ProviderCapabilities {
    /// Whether the provider supports native tool calling via API primitives.
    bool native_tool_calling = false;
    /// Whether the provider supports vision / image inputs.
    bool vision = false;

    bool operator==(const ProviderCapabilities& other) const {
        return native_tool_calling == other.native_tool_calling && vision == other.vision;
    }
    bool operator!=(const ProviderCapabilities& other) const { return !(*this == other); }
};

/// Provider-specific tool payload formats.
struct ToolsPayload {
    enum class Type { Gemini, Anthropic, OpenAI, PromptGuided };
    Type type;
    /// For Gemini: function_declarations; for Anthropic/OpenAI: tools array
    std::vector<nlohmann::json> values;
    /// For PromptGuided: text instructions
    std::string instructions;

    static ToolsPayload gemini(const std::vector<nlohmann::json>& declarations) {
        return {Type::Gemini, declarations, ""};
    }
    static ToolsPayload anthropic(const std::vector<nlohmann::json>& tools) {
        return {Type::Anthropic, tools, ""};
    }
    static ToolsPayload openai(const std::vector<nlohmann::json>& tools) {
        return {Type::OpenAI, tools, ""};
    }
    static ToolsPayload prompt_guided(const std::string& text) {
        return {Type::PromptGuided, {}, text};
    }
};

// ── Provider trait ───────────────────────────────────────────────

/// Abstract provider interface for LLM interactions.
class Provider {
public:
    virtual ~Provider() = default;

    /// Provider identifier
    virtual std::string name() const = 0;

    /// Query provider capabilities.
    virtual ProviderCapabilities capabilities() const {
        return ProviderCapabilities{};
    }

    /// Convert tool specifications to provider-native format.
    virtual ToolsPayload convert_tools(const std::vector<ToolSpec>& tools) const;

    /// One-shot chat with optional system prompt.
    virtual std::string chat_with_system(
        const std::optional<std::string>& system_prompt,
        const std::string& message,
        const std::string& model,
        double temperature) = 0;

    /// Simple one-shot chat (single user message, no explicit system prompt).
    virtual std::string simple_chat(
        const std::string& message,
        const std::string& model,
        double temperature) {
        return chat_with_system(std::nullopt, message, model, temperature);
    }

    /// Multi-turn conversation.
    virtual std::string chat_with_history(
        const std::vector<ChatMessage>& messages,
        const std::string& model,
        double temperature);

    /// Structured chat API for agent loop callers.
    virtual ChatResponse chat(
        const ChatRequest& request,
        const std::string& model,
        double temperature);

    /// Whether provider supports native tool calls over API.
    virtual bool supports_native_tools() const {
        return capabilities().native_tool_calling;
    }

    /// Whether provider supports multimodal vision input.
    virtual bool supports_vision() const {
        return capabilities().vision;
    }

    /// Warm up the HTTP connection pool.
    virtual void warmup() {}

    /// Chat with tool definitions for native function calling support.
    virtual ChatResponse chat_with_tools(
        const std::vector<ChatMessage>& messages,
        const std::vector<ToolSpec>& tools,
        const std::string& model,
        double temperature);

    /// Whether provider supports streaming responses.
    virtual bool supports_streaming() const { return false; }
};

// ── Helper functions ─────────────────────────────────────────────

/// Build tool instructions text for prompt-guided tool calling.
std::string build_tool_instructions_text(const std::vector<ToolSpec>& tools);

} // namespace providers
} // namespace zeroclaw
