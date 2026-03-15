#pragma once

#include "types.hpp"
#include "traits.hpp"
#include <map>
#include <mutex>
#include <sstream>
#include <string>

namespace zeroclaw::observability {

class PrometheusObserver : public Observer {
public:
    PrometheusObserver() = default;

    void record_event(const ObserverEvent& event) override {
        std::visit([this](auto&& arg) { this->record_event_impl(arg); }, event);
    }

    void record_metric(const ObserverMetric& metric) override {
        std::visit([this](auto&& arg) { this->record_metric_impl(arg); }, metric);
    }

    std::string name() const override { return "prometheus"; }

    std::string encode() const {
        std::ostringstream oss;
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (const auto& [name, counter] : counters_) {
            oss << "# HELP " << name << " " << counter.description << "\n";
            oss << "# TYPE " << name << " counter\n";
            for (const auto& [labels, value] : counter.values) {
                oss << name << "{" << labels << "} " << value << "\n";
            }
        }
        
        for (const auto& [name, gauge] : gauges_) {
            oss << "# HELP " << name << " " << gauge.description << "\n";
            oss << "# TYPE " << name << " gauge\n";
            for (const auto& [labels, value] : gauge.values) {
                oss << name << "{" << labels << "} " << value << "\n";
            }
        }
        
        for (const auto& [name, hist] : histograms_) {
            oss << "# HELP " << name << " " << hist.description << "\n";
            oss << "# TYPE " << name << " histogram\n";
            for (const auto& [labels, data] : hist.buckets) {
                double cumulative = 0;
                for (const auto& [bound, count] : data.bucket_counts) {
                    cumulative += count;
                    oss << name << "_bucket{" << labels << ",le=\"" << bound << "\"} " << cumulative << "\n";
                }
                oss << name << "_bucket{" << labels << ",le=\"+Inf\"} " << cumulative << "\n";
                oss << name << "_sum{" << labels << "} " << data.sum << "\n";
                oss << name << "_count{" << labels << "} " << data.count << "\n";
            }
        }
        
        return oss.str();
    }

private:
    struct CounterData {
        std::string description;
        std::map<std::string, uint64_t> values;
    };

    struct GaugeData {
        std::string description;
        std::map<std::string, double> values;
    };

    struct HistogramBucketData {
        std::map<double, uint64_t> bucket_counts;
        double sum = 0.0;
        uint64_t count = 0;
    };

    struct HistogramData {
        std::string description;
        std::map<std::string, HistogramBucketData> buckets;
    };

    mutable std::mutex mutex_;
    std::map<std::string, CounterData> counters_;
    std::map<std::string, GaugeData> gauges_;
    std::map<std::string, HistogramData> histograms_;

    void inc_counter(const std::string& name, const std::string& description, 
                     const std::string& labels = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& counter = counters_[name];
        counter.description = description;
        counter.values[labels]++;
    }

    void set_gauge(const std::string& name, const std::string& description,
                   double value, const std::string& labels = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& gauge = gauges_[name];
        gauge.description = description;
        gauge.values[labels] = value;
    }

    void observe_histogram(const std::string& name, const std::string& description,
                           double value, const std::string& labels = "") {
        static const std::vector<double> default_buckets = {
            0.01, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0, 30.0, 60.0
        };
        
        std::lock_guard<std::mutex> lock(mutex_);
        auto& hist = histograms_[name];
        hist.description = description;
        auto& bucket = hist.buckets[labels];
        
        for (double bound : default_buckets) {
            if (value <= bound) {
                bucket.bucket_counts[bound]++;
            }
        }
        bucket.sum += value;
        bucket.count++;
    }

    void record_event_impl(const AgentStartEvent& e) {
        std::string labels = "provider=\"" + e.provider + "\",model=\"" + e.model + "\"";
        inc_counter("zeroclaw_agent_starts_total", "Total agent invocations", labels);
    }

    void record_event_impl(const LlmRequestEvent&) {}

    void record_event_impl(const LlmResponseEvent& e) {
        std::string labels = "provider=\"" + e.provider + "\",model=\"" + e.model + 
                             "\",success=\"" + (e.success ? "true" : "false") + "\"";
        inc_counter("zeroclaw_llm_requests_total", "Total LLM provider requests", labels);
        
        if (e.input_tokens) {
            std::string token_labels = "provider=\"" + e.provider + "\",model=\"" + e.model + "\"";
            std::lock_guard<std::mutex> lock(mutex_);
            counters_["zeroclaw_tokens_input_total"].description = "Total input tokens consumed";
            counters_["zeroclaw_tokens_input_total"].values[token_labels] += *e.input_tokens;
        }
        if (e.output_tokens) {
            std::string token_labels = "provider=\"" + e.provider + "\",model=\"" + e.model + "\"";
            std::lock_guard<std::mutex> lock(mutex_);
            counters_["zeroclaw_tokens_output_total"].description = "Total output tokens consumed";
            counters_["zeroclaw_tokens_output_total"].values[token_labels] += *e.output_tokens;
        }
    }

    void record_event_impl(const AgentEndEvent& e) {
        double secs = static_cast<double>(e.duration_ms) / 1000.0;
        std::string labels = "provider=\"" + e.provider + "\",model=\"" + e.model + "\"";
        observe_histogram("zeroclaw_agent_duration_seconds", "Agent invocation duration in seconds", secs, labels);
        
        if (e.tokens_used) {
            set_gauge("zeroclaw_tokens_used_last", "Tokens used in the last request", 
                      static_cast<double>(*e.tokens_used));
        }
    }

    void record_event_impl(const ToolCallStartEvent&) {}

    void record_event_impl(const ToolCallEvent& e) {
        double secs = static_cast<double>(e.duration_ms) / 1000.0;
        std::string labels = "tool=\"" + e.tool + "\",success=\"" + (e.success ? "true" : "false") + "\"";
        inc_counter("zeroclaw_tool_calls_total", "Total tool calls", labels);
        
        std::string dur_labels = "tool=\"" + e.tool + "\"";
        observe_histogram("zeroclaw_tool_duration_seconds", "Tool execution duration in seconds", secs, dur_labels);
    }

    void record_event_impl(const TurnCompleteEvent&) {}

    void record_event_impl(const ChannelMessageEvent& e) {
        std::string labels = "channel=\"" + e.channel + "\",direction=\"" + e.direction + "\"";
        inc_counter("zeroclaw_channel_messages_total", "Total channel messages", labels);
    }

    void record_event_impl(const HeartbeatTickEvent&) {
        inc_counter("zeroclaw_heartbeat_ticks_total", "Total heartbeat ticks");
    }

    void record_event_impl(const ErrorEvent& e) {
        std::string labels = "component=\"" + e.component + "\"";
        inc_counter("zeroclaw_errors_total", "Total errors by component", labels);
    }

    void record_metric_impl(const RequestLatencyMetric& m) {
        double secs = static_cast<double>(m.duration_ms) / 1000.0;
        observe_histogram("zeroclaw_request_latency_seconds", "Request latency in seconds", secs);
    }

    void record_metric_impl(const TokensUsedMetric& m) {
        set_gauge("zeroclaw_tokens_used_last", "Tokens used in the last request", static_cast<double>(m.count));
    }

    void record_metric_impl(const ActiveSessionsMetric& m) {
        set_gauge("zeroclaw_active_sessions", "Number of active sessions", static_cast<double>(m.count));
    }

    void record_metric_impl(const QueueDepthMetric& m) {
        set_gauge("zeroclaw_queue_depth", "Message queue depth", static_cast<double>(m.depth));
    }
};

inline ObserverPtr make_prometheus_observer() {
    return std::make_shared<PrometheusObserver>();
}

}
