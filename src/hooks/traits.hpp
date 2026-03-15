#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include "../providers/traits.hpp"
#include "../channels/traits.hpp"
#include "../tools/traits.hpp"
#include "types.hpp"

namespace zeroclaw::hooks {
class HookHandler {
public:
    virtual ~HookHandler() = default;
    virtual std::string name() const = 0;
    virtual int32_t priority() const { return 0; }

    virtual void on_gateway_start(const std::string&, uint16_t) {}
    virtual void on_gateway_stop() {}
    virtual void on_session_start(const std::string&, const std::string&) {}
    virtual void on_session_end(const std::string&, const std::string&) {}
    virtual void on_llm_input(const std::vector<providers::ChatMessage>&, const std::string&) {}
    virtual void on_llm_output(const providers::ChatResponse&) {}
    virtual void on_after_tool_call(const std::string&, const tools::ToolResult&, std::chrono::milliseconds) {}
    virtual void on_message_sent(const std::string&, const std::string&, const std::string&) {}
    virtual void on_heartbeat_tick() {}

    virtual HookResult<std::pair<std::string, std::string>> before_model_resolve(
        std::string provider, std::string model) {
        return HookResult<std::pair<std::string, std::string>>::Continue({std::move(provider), std::move(model)});
    }

    virtual HookResult<std::string> before_prompt_build(std::string prompt) {
        return HookResult<std::string>::Continue(std::move(prompt));
    }

    virtual HookResult<std::pair<std::vector<providers::ChatMessage>, std::string>> before_llm_call(
        std::vector<providers::ChatMessage> messages, std::string model) {
        return HookResult<std::pair<std::vector<providers::ChatMessage>, std::string>>::Continue(
            {std::move(messages), std::move(model)});
    }

    virtual HookResult<std::pair<std::string, nlohmann::json>> before_tool_call(
        std::string name, nlohmann::json args) {
        return HookResult<std::pair<std::string, nlohmann::json>>::Continue({std::move(name), std::move(args)});
    }

    virtual HookResult<channels::ChannelMessage> on_message_received(channels::ChannelMessage message) {
        return HookResult<channels::ChannelMessage>::Continue(std::move(message));
    }

    virtual HookResult<std::tuple<std::string, std::string, std::string>> on_message_sending(
        std::string channel, std::string recipient, std::string content) {
        return HookResult<std::tuple<std::string, std::string, std::string>>::Continue(
            {std::move(channel), std::move(recipient), std::move(content)});
    }
};
}
