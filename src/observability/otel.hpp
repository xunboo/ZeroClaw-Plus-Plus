#pragma once

#include "types.hpp"
#include "traits.hpp"
#include <map>
#include <mutex>
#include <string>

namespace zeroclaw::observability {

class OtelObserver : public Observer {
public:
    static std::shared_ptr<OtelObserver> create(
        const std::string& endpoint = "http://localhost:4318",
        const std::string& service_name = "zeroclaw") {
        return std::shared_ptr<OtelObserver>(new OtelObserver(endpoint, service_name));
    }

    void record_event(const ObserverEvent& event) override {
        std::visit([this](auto&& arg) { this->record_event_impl(arg); }, event);
    }

    void record_metric(const ObserverMetric& metric) override {
        std::visit([this](auto&& arg) { this->record_metric_impl(arg); }, metric);
    }

    void flush() override {
        std::lock_guard<std::mutex> lock(mutex_);
        flushed_ = true;
    }

    std::string name() const override { return "otel"; }

    const std::string& endpoint() const { return endpoint_; }
    const std::string& service_name() const { return service_name_; }

private:
    OtelObserver(std::string endpoint, std::string service_name)
        : endpoint_(std::move(endpoint))
        , service_name_(std::move(service_name)) {}

    std::string endpoint_;
    std::string service_name_;
    mutable std::mutex mutex_;
    bool flushed_ = false;
    
    std::map<std::string, uint64_t> counters_;
    std::map<std::string, double> histograms_;
    std::map<std::string, uint64_t> gauges_;

    void inc_counter(const std::string& name, uint64_t delta = 1) {
        std::lock_guard<std::mutex> lock(mutex_);
        counters_[name] += delta;
    }

    void record_histogram(const std::string& name, double value) {
        std::lock_guard<std::mutex> lock(mutex_);
        histograms_[name] += value;
    }

    void set_gauge(const std::string& name, uint64_t value) {
        std::lock_guard<std::mutex> lock(mutex_);
        gauges_[name] = value;
    }

    void record_event_impl(const AgentStartEvent&) {
        inc_counter("zeroclaw.agent.starts");
    }

    void record_event_impl(const LlmRequestEvent&) {}

    void record_event_impl(const LlmResponseEvent& e) {
        double secs = static_cast<double>(e.duration_ms) / 1000.0;
        inc_counter("zeroclaw.llm.calls");
        record_histogram("zeroclaw.llm.duration", secs);
    }

    void record_event_impl(const AgentEndEvent& e) {
        double secs = static_cast<double>(e.duration_ms) / 1000.0;
        record_histogram("zeroclaw.agent.duration", secs);
    }

    void record_event_impl(const ToolCallStartEvent&) {}

    void record_event_impl(const ToolCallEvent& e) {
        double secs = static_cast<double>(e.duration_ms) / 1000.0;
        inc_counter("zeroclaw.tool.calls");
        record_histogram("zeroclaw.tool.duration", secs);
    }

    void record_event_impl(const TurnCompleteEvent&) {}

    void record_event_impl(const ChannelMessageEvent&) {
        inc_counter("zeroclaw.channel.messages");
    }

    void record_event_impl(const HeartbeatTickEvent&) {
        inc_counter("zeroclaw.heartbeat.ticks");
    }

    void record_event_impl(const ErrorEvent&) {
        inc_counter("zeroclaw.errors");
    }

    void record_metric_impl(const RequestLatencyMetric& m) {
        double secs = static_cast<double>(m.duration_ms) / 1000.0;
        record_histogram("zeroclaw.request.latency", secs);
    }

    void record_metric_impl(const TokensUsedMetric& m) {
        inc_counter("zeroclaw.tokens.used", m.count);
    }

    void record_metric_impl(const ActiveSessionsMetric& m) {
        set_gauge("zeroclaw.sessions.active", m.count);
    }

    void record_metric_impl(const QueueDepthMetric& m) {
        set_gauge("zeroclaw.queue.depth", m.depth);
    }
};

inline ObserverPtr make_otel_observer(
    const std::string& endpoint = "http://localhost:4318",
    const std::string& service_name = "zeroclaw") {
    return OtelObserver::create(endpoint, service_name);
}

}
