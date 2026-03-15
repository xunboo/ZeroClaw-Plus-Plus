#pragma once

#include "types.hpp"
#include "traits.hpp"
#include <iostream>

namespace zeroclaw::observability {

class LogObserver : public Observer {
public:
    LogObserver() = default;

    void record_event(const ObserverEvent& event) override {
        std::visit([this](auto&& arg) { this->log_event(arg); }, event);
    }

    void record_metric(const ObserverMetric& metric) override {
        std::visit([this](auto&& arg) { this->log_metric(arg); }, metric);
    }

    std::string name() const override { return "log"; }

private:
    void log_event(const AgentStartEvent& e) {
        std::clog << "[INFO] agent.start provider=" << e.provider 
                  << " model=" << e.model << "\n";
    }

    void log_event(const LlmRequestEvent& e) {
        std::clog << "[INFO] llm.request provider=" << e.provider
                  << " model=" << e.model
                  << " messages_count=" << e.messages_count << "\n";
    }

    void log_event(const LlmResponseEvent& e) {
        std::clog << "[INFO] llm.response provider=" << e.provider
                  << " model=" << e.model
                  << " duration_ms=" << e.duration_ms
                  << " success=" << (e.success ? "true" : "false");
        if (e.error_message) std::clog << " error=\"" << *e.error_message << "\"";
        if (e.input_tokens) std::clog << " input_tokens=" << *e.input_tokens;
        if (e.output_tokens) std::clog << " output_tokens=" << *e.output_tokens;
        std::clog << "\n";
    }

    void log_event(const AgentEndEvent& e) {
        std::clog << "[INFO] agent.end provider=" << e.provider
                  << " model=" << e.model
                  << " duration_ms=" << e.duration_ms;
        if (e.tokens_used) std::clog << " tokens=" << *e.tokens_used;
        std::clog << "\n";
    }

    void log_event(const ToolCallStartEvent& e) {
        std::clog << "[INFO] tool.start tool=" << e.tool << "\n";
    }

    void log_event(const ToolCallEvent& e) {
        std::clog << "[INFO] tool.call tool=" << e.tool
                  << " duration_ms=" << e.duration_ms
                  << " success=" << (e.success ? "true" : "false") << "\n";
    }

    void log_event(const TurnCompleteEvent&) {
        std::clog << "[INFO] turn.complete\n";
    }

    void log_event(const ChannelMessageEvent& e) {
        std::clog << "[INFO] channel.message channel=" << e.channel
                  << " direction=" << e.direction << "\n";
    }

    void log_event(const HeartbeatTickEvent&) {
        std::clog << "[INFO] heartbeat.tick\n";
    }

    void log_event(const ErrorEvent& e) {
        std::clog << "[INFO] error component=" << e.component
                  << " message=\"" << e.message << "\"\n";
    }

    void log_metric(const RequestLatencyMetric& m) {
        std::clog << "[INFO] metric.request_latency latency_ms=" << m.duration_ms << "\n";
    }

    void log_metric(const TokensUsedMetric& m) {
        std::clog << "[INFO] metric.tokens_used tokens=" << m.count << "\n";
    }

    void log_metric(const ActiveSessionsMetric& m) {
        std::clog << "[INFO] metric.active_sessions sessions=" << m.count << "\n";
    }

    void log_metric(const QueueDepthMetric& m) {
        std::clog << "[INFO] metric.queue_depth depth=" << m.depth << "\n";
    }
};

inline ObserverPtr make_log_observer() {
    return std::make_shared<LogObserver>();
}

}
