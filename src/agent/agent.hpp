#pragma once

/// Agent — the core agentic loop orchestrator.
///
/// Contains the Agent struct (with builder pattern) and the main turn/run logic
/// that orchestrates LLM calls, tool execution, memory loading, and history management.

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <functional>
#include <filesystem>
#include <iostream>
#include <algorithm>

#include "../providers/traits.hpp"
#include "../tools/traits.hpp"
#include "classifier.hpp"
#include "dispatcher.hpp"
#include "memory_loader.hpp"
#include "prompt.hpp"

namespace zeroclaw {
namespace agent {

using providers::ChatMessage;
using providers::ChatRequest;
using providers::ChatResponse;
using providers::ConversationMessage;
using providers::Provider;

/// Agent configuration
struct AgentConfig {
    size_t max_tool_iterations = 10;
    size_t max_history_messages = 50;
    bool parallel_tools = false;
    std::string tool_dispatcher = "auto";  // "native", "xml", or "auto"
};

/// Agent — the core orchestrator for multi-turn LLM interactions with tool calling.
class Agent {
public:
    /// Builder pattern for constructing agents
    class Builder {
    public:
        Builder& provider(std::unique_ptr<Provider> p) { provider_ = std::move(p); return *this; }
        Builder& tools(std::vector<std::unique_ptr<Tool>> t) { tools_ = std::move(t); return *this; }
        Builder& memory(Memory* m) { memory_ = m; return *this; }
        Builder& tool_dispatcher(std::unique_ptr<ToolDispatcher> d) { dispatcher_ = std::move(d); return *this; }
        Builder& memory_loader(std::unique_ptr<MemoryLoader> l) { loader_ = std::move(l); return *this; }
        Builder& prompt_builder(SystemPromptBuilder pb) { prompt_builder_ = std::move(pb); return *this; }
        Builder& config(const AgentConfig& c) { config_ = c; return *this; }
        Builder& model_name(const std::string& m) { model_name_ = m; return *this; }
        Builder& temperature(double t) { temperature_ = t; return *this; }
        Builder& workspace_dir(const std::filesystem::path& p) { workspace_dir_ = p; return *this; }
        Builder& classification_config(const QueryClassificationConfig& c) { classification_config_ = c; return *this; }
        Builder& available_hints(const std::vector<std::string>& h) { available_hints_ = h; return *this; }
        Builder& auto_save(bool a) { auto_save_ = a; return *this; }

        /// Build the agent. Returns nullptr on error (missing required fields).
        std::unique_ptr<Agent> build();

    private:
        std::unique_ptr<Provider> provider_;
        std::vector<std::unique_ptr<Tool>> tools_;
        Memory* memory_ = nullptr;
        std::unique_ptr<ToolDispatcher> dispatcher_;
        std::unique_ptr<MemoryLoader> loader_;
        std::optional<SystemPromptBuilder> prompt_builder_;
        AgentConfig config_;
        std::string model_name_ = "anthropic/claude-sonnet-4-20250514";
        double temperature_ = 0.7;
        std::filesystem::path workspace_dir_ = ".";
        QueryClassificationConfig classification_config_;
        std::vector<std::string> available_hints_;
        bool auto_save_ = false;
    };

    /// Create a new builder
    static Builder builder() { return Builder{}; }

    /// Get conversation history
    const std::vector<ConversationMessage>& history() const { return history_; }

    /// Clear conversation history
    void clear_history() { history_.clear(); }

    /// Execute a single conversational turn with tool calling loop.
    /// Returns the final LLM text response.
    std::string turn(const std::string& user_message);

    /// Run a single message and return the response
    std::string run_single(const std::string& message) { return turn(message); }

private:
    Agent() = default;

    /// Trim history to prevent unbounded growth
    void trim_history();

    /// Build the system prompt from sections
    std::string build_system_prompt();

    /// Execute a single tool call
    ToolExecutionResult execute_tool_call(const ParsedToolCall& call);

    /// Execute multiple tool calls
    std::vector<ToolExecutionResult> execute_tools(const std::vector<ParsedToolCall>& calls);

    /// Classify user message to select model route
    std::string classify_model(const std::string& user_message);

    std::unique_ptr<Provider> provider_;
    std::vector<std::unique_ptr<Tool>> tools_;
    std::vector<ToolSpec> tool_specs_;
    Memory* memory_ = nullptr;
    std::unique_ptr<ToolDispatcher> dispatcher_;
    std::unique_ptr<MemoryLoader> loader_;
    SystemPromptBuilder prompt_builder_;
    AgentConfig config_;
    std::string model_name_;
    double temperature_ = 0.7;
    std::filesystem::path workspace_dir_;
    QueryClassificationConfig classification_config_;
    std::vector<std::string> available_hints_;
    bool auto_save_ = false;
    std::vector<ConversationMessage> history_;
};

} // namespace agent
} // namespace zeroclaw
