#pragma once

#include "types.hpp"
#include "traits.hpp"
#include <iostream>

namespace zeroclaw::observability {

class VerboseObserver : public Observer {
public:
    VerboseObserver() = default;

    void record_event(const ObserverEvent& event) override {
        std::visit([this](auto&& arg) { this->print_event(arg); }, event);
    }

    void record_metric(const ObserverMetric&) override {}

    std::string name() const override { return "verbose"; }

private:
    void print_event(const AgentStartEvent&) {}
    
    void print_event(const LlmRequestEvent& e) {
        std::cerr << "> Thinking\n";
        std::cerr << "> Send (provider=" << e.provider
                  << ", model=" << e.model
                  << ", messages=" << e.messages_count << ")\n";
    }

    void print_event(const LlmResponseEvent& e) {
        std::cerr << "< Receive (success=" << (e.success ? "true" : "false")
                  << ", duration_ms=" << e.duration_ms << ")\n";
    }

    void print_event(const AgentEndEvent&) {}
    
    void print_event(const ToolCallStartEvent& e) {
        std::cerr << "> Tool " << e.tool << "\n";
    }

    void print_event(const ToolCallEvent& e) {
        std::cerr << "< Tool " << e.tool
                  << " (success=" << (e.success ? "true" : "false")
                  << ", duration_ms=" << e.duration_ms << ")\n";
    }

    void print_event(const TurnCompleteEvent&) {
        std::cerr << "< Complete\n";
    }

    void print_event(const ChannelMessageEvent&) {}
    
    void print_event(const HeartbeatTickEvent&) {}
    
    void print_event(const ErrorEvent&) {}
};

inline ObserverPtr make_verbose_observer() {
    return std::make_shared<VerboseObserver>();
}

}
