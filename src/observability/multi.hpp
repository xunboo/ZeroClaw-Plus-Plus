#pragma once

#include "types.hpp"
#include "traits.hpp"
#include <vector>

namespace zeroclaw::observability {

class MultiObserver : public Observer {
public:
    explicit MultiObserver(std::vector<ObserverPtr> observers)
        : observers_(std::move(observers)) {}

    void record_event(const ObserverEvent& event) override {
        for (const auto& obs : observers_) {
            obs->record_event(event);
        }
    }

    void record_metric(const ObserverMetric& metric) override {
        for (const auto& obs : observers_) {
            obs->record_metric(metric);
        }
    }

    void flush() override {
        for (const auto& obs : observers_) {
            obs->flush();
        }
    }

    std::string name() const override { return "multi"; }

private:
    std::vector<ObserverPtr> observers_;
};

inline ObserverPtr make_multi_observer(std::vector<ObserverPtr> observers) {
    return std::make_shared<MultiObserver>(std::move(observers));
}

}
