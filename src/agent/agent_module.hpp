#pragma once

/// Agent module — main header aggregating all agent submodules.
///
/// This module implements the core agentic loop orchestration: receiving user messages,
/// building prompts, calling LLM providers, parsing tool calls, executing tools,
/// and managing conversation history.

#include "agent/agent.hpp"
#include "agent/classifier.hpp"
#include "agent/dispatcher.hpp"
#include "agent/loop_.hpp"
#include "agent/memory_loader.hpp"
#include "agent/prompt.hpp"

namespace zeroclaw {
namespace config { struct Config; }

namespace agent {
    // All types re-exported from sub-headers above

    // Main entry point for one-shot agent execution
    std::string run(const config::Config& config,
                    const std::string& user_message,
                    std::optional<std::string> provider_override,
                    std::optional<std::string> model_override,
                    double temperature,
                    std::vector<std::string> peripherals,
                    bool ensure_runtime);

} // namespace agent
} // namespace zeroclaw
