#include "agent.hpp"
#include "config/config.hpp"
#include <chrono>
#include "../tools_module.hpp"
#include "../providers_module.hpp"
#include "../security/policy.hpp"

namespace zeroclaw {
namespace agent {

// ── Agent::Builder ───────────────────────────────────────────────

std::unique_ptr<Agent> Agent::Builder::build() {
    if (!provider_) return nullptr;
    if (tools_.empty()) return nullptr;
    if (!dispatcher_) return nullptr;

    auto agent = std::unique_ptr<Agent>(new Agent());

    // Compute tool specs before moving tools
    for (const auto& tool : tools_) {
        agent->tool_specs_.push_back(tool->spec());
    }

    agent->provider_ = std::move(provider_);
    agent->tools_ = std::move(tools_);
    agent->memory_ = memory_;
    agent->dispatcher_ = std::move(dispatcher_);
    agent->loader_ = loader_ ? std::move(loader_) : std::make_unique<DefaultMemoryLoader>();
    agent->prompt_builder_ = prompt_builder_.has_value()
        ? std::move(*prompt_builder_)
        : SystemPromptBuilder::with_defaults();
    agent->config_ = config_;
    agent->model_name_ = model_name_;
    agent->temperature_ = temperature_;
    agent->workspace_dir_ = workspace_dir_;
    agent->classification_config_ = classification_config_;
    agent->available_hints_ = available_hints_;
    agent->auto_save_ = auto_save_;

    return agent;
}

// ── Agent ────────────────────────────────────────────────────────

void Agent::trim_history() {
    size_t max = config_.max_history_messages;
    if (history_.size() <= max) return;

    std::vector<ConversationMessage> system_messages;
    std::vector<ConversationMessage> other_messages;

    for (auto& msg : history_) {
        if (msg.type == ConversationMessage::Type::Chat &&
            msg.chat.has_value() && msg.chat->role == "system") {
            system_messages.push_back(std::move(msg));
        } else {
            other_messages.push_back(std::move(msg));
        }
    }

    if (other_messages.size() > max) {
        size_t drop_count = other_messages.size() - max;
        other_messages.erase(other_messages.begin(),
                              other_messages.begin() + static_cast<ptrdiff_t>(drop_count));
    }

    history_ = std::move(system_messages);
    history_.insert(history_.end(),
                     std::make_move_iterator(other_messages.begin()),
                     std::make_move_iterator(other_messages.end()));
}

std::string Agent::build_system_prompt() {
    auto instructions = dispatcher_->prompt_instructions(tools_);
    PromptContext ctx;
    ctx.workspace_dir = workspace_dir_;
    ctx.model_name = model_name_;
    ctx.tools = &tools_;
    ctx.dispatcher_instructions = instructions;
    return prompt_builder_.build(ctx);
}

ToolExecutionResult Agent::execute_tool_call(const ParsedToolCall& call) {
    auto start = std::chrono::steady_clock::now();

    std::string result;
    bool success = false;

    // Find the tool
    Tool* found_tool = nullptr;
    for (const auto& tool : tools_) {
        if (tool->name() == call.name) {
            found_tool = tool.get();
            break;
        }
    }

    if (found_tool) {
        auto tool_result = found_tool->execute(call.arguments);
        if (tool_result.success) {
            result = tool_result.output;
            success = true;
        } else {
            result = "Error: " + tool_result.error.value_or(tool_result.output);
        }
    } else {
        result = "Unknown tool: " + call.name;
    }

    return {call.name, result, success, call.tool_call_id};
}

std::vector<ToolExecutionResult>
Agent::execute_tools(const std::vector<ParsedToolCall>& calls) {
    // Sequential execution (parallel would require threading)
    std::vector<ToolExecutionResult> results;
    results.reserve(calls.size());
    for (const auto& call : calls) {
        results.push_back(execute_tool_call(call));
    }
    return results;
}

std::string Agent::classify_model(const std::string& user_message) {
    auto hint = classify(classification_config_, user_message);
    if (hint.has_value()) {
        std::string hint_val = hint.value();
        for (const auto& h : available_hints_) {
            if (h == hint_val) {
                return "hint:" + hint_val;
            }
        }
    }
    return model_name_;
}

std::string Agent::turn(const std::string& user_message) {
    // Initialize history with system prompt on first turn
    if (history_.empty()) {
        auto system_prompt = build_system_prompt();
        history_.push_back(
            ConversationMessage::make_chat(ChatMessage::system(system_prompt)));
    }

    // Auto-save user message to memory
    if (auto_save_ && memory_) {
        memory_->store("user_msg", user_message, "conversation", std::nullopt);
    }

    // Load memory context
    std::string context;
    if (loader_ && memory_) {
        context = loader_->load_context(memory_, user_message);
    }

    // Enrich user message with memory context
    std::string enriched = context.empty() ? user_message : (context + user_message);
    history_.push_back(
        ConversationMessage::make_chat(ChatMessage::user(enriched)));

    std::string effective_model = classify_model(user_message);

    // Tool calling loop
    for (size_t iter = 0; iter < config_.max_tool_iterations; ++iter) {
        auto messages = dispatcher_->to_provider_messages(history_);

        ChatRequest request;
        request.messages = &messages;
        auto* tool_specs_ptr = dispatcher_->should_send_tool_specs() ? &tool_specs_ : nullptr;
        request.tools = tool_specs_ptr;

        auto response = provider_->chat(request, effective_model, temperature_);

        auto [text, calls] = dispatcher_->parse_response(response);

        if (calls.empty()) {
            std::string final_text = text.empty()
                ? response.text.value_or("") : text;

            history_.push_back(
                ConversationMessage::make_chat(ChatMessage::assistant(final_text)));
            trim_history();
            return final_text;
        }

        // Intermediate text output
        if (!text.empty()) {
            history_.push_back(
                ConversationMessage::make_chat(ChatMessage::assistant(text)));
            std::cout << text;
            std::cout.flush();
        }

        // Record tool calls in history
        history_.push_back(
            ConversationMessage::make_tool_calls(
                response.text, response.tool_calls, response.reasoning_content));

        // Execute tools and record results
        auto results = execute_tools(calls);
        auto formatted = dispatcher_->format_results(results);
        history_.push_back(std::move(formatted));
        trim_history();
    }

    return "Error: Agent exceeded maximum tool iterations ("
           + std::to_string(config_.max_tool_iterations) + ")";
}

std::string run(const zeroclaw::config::Config& config,
                const std::string& user_message,
                std::optional<std::string> provider_override,
                std::optional<std::string> model_override,
                double temperature,
                std::vector<std::string> peripherals,
                bool ensure_runtime) {
    // 1. Setup security policy
    auto sec = std::make_shared<zeroclaw::security::SecurityPolicy>();
    
    // 2. Load provider
    std::string prov_name = provider_override.value_or(config.default_provider.value_or("openai"));
    auto provider = zeroclaw::providers::create_provider(prov_name);
    
    // 3. Setup tools
    auto tools = zeroclaw::tools::default_tools(sec);
    
    // 4. Setup dispatcher
    auto dispatcher = std::make_unique<zeroclaw::agent::XmlToolDispatcher>();
    
    // 5. Build Agent
    auto agent = zeroclaw::agent::Agent::builder()
        .provider(std::move(provider))
        .tools(std::move(tools))
        .tool_dispatcher(std::move(dispatcher))
        .model_name(model_override.value_or(config.default_model.value_or("anthropic/claude-sonnet-4-20250514")))
        .temperature(temperature)
        .build();
    
    if (!agent) {
        return "Error building agent";
    }
    
    if (!user_message.empty()) {
        return agent->run_single(user_message);
    } else {
        std::cout << "Entering interactive mode (type 'exit' to quit)\n";
        std::string line;
        while (true) {
            std::cout << "> ";
            if (!std::getline(std::cin, line) || line == "exit") break;
            if (line.empty()) continue;
            auto result = agent->run_single(line);
            std::cout << "\n" << result << "\n";
        }
        return "";
    }
}

} // namespace agent
} // namespace zeroclaw
