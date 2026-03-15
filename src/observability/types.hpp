#pragma once

#include <string>
#include <optional>
#include <variant>
#include <chrono>
#include <cstdint>

namespace zeroclaw::observability {

struct HeartbeatTickEvent {};

struct ErrorEvent {
    std::string component;
    std::string message;
};

struct AgentStartEvent {
    std::string provider;
    std::string model;
};

struct AgentEndEvent {
    std::string provider;
    std::string model;
    uint64_t duration_ms;
    std::optional<uint64_t> tokens_used;
};

struct LlmRequestEvent {
    std::string provider;
    std::string model;
    size_t messages_count;
};

struct LlmResponseEvent {
    std::string provider;
    std::string model;
    uint64_t duration_ms;
    bool success;
    std::optional<std::string> error_message;
    std::optional<uint64_t> input_tokens;
    std::optional<uint64_t> output_tokens;
};

struct ToolCallStartEvent {
    std::string tool;
};

struct ToolCallEvent {
    std::string tool;
    uint64_t duration_ms;
    bool success;
};

struct TurnCompleteEvent {};

struct ChannelMessageEvent {
    std::string channel;
    std::string direction;
};

using ObserverEvent = std::variant<
    HeartbeatTickEvent,
    ErrorEvent,
    AgentStartEvent,
    AgentEndEvent,
    LlmRequestEvent,
    LlmResponseEvent,
    ToolCallStartEvent,
    ToolCallEvent,
    TurnCompleteEvent,
    ChannelMessageEvent
>;

struct RequestLatencyMetric {
    uint64_t duration_ms;
};

struct TokensUsedMetric {
    uint64_t count;
};

struct ActiveSessionsMetric {
    uint64_t count;
};

struct QueueDepthMetric {
    uint64_t depth;
};

using ObserverMetric = std::variant<
    RequestLatencyMetric,
    TokensUsedMetric,
    ActiveSessionsMetric,
    QueueDepthMetric
>;

}
