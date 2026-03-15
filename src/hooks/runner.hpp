#pragma once

#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <future>
#include <thread>
#include "../providers/traits.hpp"
#include "../channels/traits.hpp"
#include "../tools/traits.hpp"
#include "types.hpp"
#include "traits.hpp"

namespace zeroclaw::hooks {
class HookRunner {
public:
    HookRunner() = default;

    void register_handler(std::unique_ptr<HookHandler> handler) {
        handlers_.push_back(std::move(handler));
        std::stable_sort(handlers_.begin(), handlers_.end(),
            [](const auto& a, const auto& b) {
                return a->priority() > b->priority();
            });
    }

    void fire_gateway_start(const std::string& host, uint16_t port) {
        std::vector<std::future<void>> futures;
        for (const auto& h : handlers_) {
            futures.push_back(std::async(std::launch::async, [&h, &host, port]() {
                h->on_gateway_start(host, port);
            }));
        }
        for (auto& f : futures) f.wait();
    }

    void fire_gateway_stop() {
        std::vector<std::future<void>> futures;
        for (const auto& h : handlers_) {
            futures.push_back(std::async(std::launch::async, [&h]() {
                h->on_gateway_stop();
            }));
        }
        for (auto& f : futures) f.wait();
    }

    void fire_session_start(const std::string& session_id, const std::string& channel) {
        std::vector<std::future<void>> futures;
        for (const auto& h : handlers_) {
            futures.push_back(std::async(std::launch::async, [&h, &session_id, &channel]() {
                h->on_session_start(session_id, channel);
            }));
        }
        for (auto& f : futures) f.wait();
    }

    void fire_session_end(const std::string& session_id, const std::string& channel) {
        std::vector<std::future<void>> futures;
        for (const auto& h : handlers_) {
            futures.push_back(std::async(std::launch::async, [&h, &session_id, &channel]() {
                h->on_session_end(session_id, channel);
            }));
        }
        for (auto& f : futures) f.wait();
    }

    void fire_llm_input(const std::vector<providers::ChatMessage>& messages, const std::string& model) {
        std::vector<std::future<void>> futures;
        for (const auto& h : handlers_) {
            futures.push_back(std::async(std::launch::async, [&h, &messages, &model]() {
                h->on_llm_input(messages, model);
            }));
        }
        for (auto& f : futures) f.wait();
    }

    void fire_llm_output(const providers::ChatResponse& response) {
        std::vector<std::future<void>> futures;
        for (const auto& h : handlers_) {
            futures.push_back(std::async(std::launch::async, [&h, &response]() {
                h->on_llm_output(response);
            }));
        }
        for (auto& f : futures) f.wait();
    }

    void fire_after_tool_call(const std::string& tool, const tools::ToolResult& result, std::chrono::milliseconds duration) {
        std::vector<std::future<void>> futures;
        for (const auto& h : handlers_) {
            futures.push_back(std::async(std::launch::async, [&h, &tool, &result, duration]() {
                h->on_after_tool_call(tool, result, duration);
            }));
        }
        for (auto& f : futures) f.wait();
    }

    void fire_message_sent(const std::string& channel, const std::string& recipient, const std::string& content) {
        std::vector<std::future<void>> futures;
        for (const auto& h : handlers_) {
            futures.push_back(std::async(std::launch::async, [&h, &channel, &recipient, &content]() {
                h->on_message_sent(channel, recipient, content);
            }));
        }
        for (auto& f : futures) f.wait();
    }

    void fire_heartbeat_tick() {
        std::vector<std::future<void>> futures;
        for (const auto& h : handlers_) {
            futures.push_back(std::async(std::launch::async, [&h]() {
                h->on_heartbeat_tick();
            }));
        }
        for (auto& f : futures) f.wait();
    }

    HookResult<std::pair<std::string, std::string>> run_before_model_resolve(
        std::string provider, std::string model) {
        for (const auto& h : handlers_) {
            try {
                auto result = h->before_model_resolve(provider, model);
                if (result.is_cancel()) {
                    return result;
                }
                provider = result.value().first;
                model = result.value().second;
            } catch (...) {}
        }
        return HookResult<std::pair<std::string, std::string>>::Continue({provider, model});
    }

    HookResult<std::string> run_before_prompt_build(std::string prompt) {
        for (const auto& h : handlers_) {
            try {
                auto result = h->before_prompt_build(prompt);
                if (result.is_cancel()) {
                    return result;
                }
                prompt = result.value();
            } catch (...) {}
        }
        return HookResult<std::string>::Continue(prompt);
    }

    HookResult<std::pair<std::vector<providers::ChatMessage>, std::string>> run_before_llm_call(
        std::vector<providers::ChatMessage> messages, std::string model) {
        for (const auto& h : handlers_) {
            try {
                auto result = h->before_llm_call(messages, model);
                if (result.is_cancel()) {
                    return result;
                }
                messages = result.value().first;
                model = result.value().second;
            } catch (...) {}
        }
        return HookResult<std::pair<std::vector<providers::ChatMessage>, std::string>>::Continue({messages, model});
    }

    HookResult<std::pair<std::string, nlohmann::json>> run_before_tool_call(
        std::string name, nlohmann::json args) {
        for (const auto& h : handlers_) {
            try {
                auto result = h->before_tool_call(name, args);
                if (result.is_cancel()) {
                    return result;
                }
                name = result.value().first;
                args = result.value().second;
            } catch (...) {}
        }
        return HookResult<std::pair<std::string, nlohmann::json>>::Continue({name, args});
    }

    HookResult<channels::ChannelMessage> run_on_message_received(channels::ChannelMessage message) {
        for (const auto& h : handlers_) {
            try {
                auto result = h->on_message_received(message);
                if (result.is_cancel()) {
                    return result;
                }
                message = result.value();
            } catch (...) {}
        }
        return HookResult<channels::ChannelMessage>::Continue(message);
    }

    HookResult<std::tuple<std::string, std::string, std::string>> run_on_message_sending(
        std::string channel, std::string recipient, std::string content) {
        for (const auto& h : handlers_) {
            try {
                auto result = h->on_message_sending(channel, recipient, content);
                if (result.is_cancel()) {
                    return result;
                }
                std::tie(channel, recipient, content) = result.value();
            } catch (...) {}
        }
        return HookResult<std::tuple<std::string, std::string, std::string>>::Continue(
            {channel, recipient, content});
    }

private:
    std::vector<std::unique_ptr<HookHandler>> handlers_;
};
}
