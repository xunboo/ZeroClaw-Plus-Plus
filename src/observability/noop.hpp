#pragma once

#include "types.hpp"
#include "traits.hpp"

namespace zeroclaw::observability {

class NoopObserver : public Observer {
public:
    void record_event(const ObserverEvent&) override {}
    void record_metric(const ObserverMetric&) override {}
    std::string name() const override { return "noop"; }
};

inline ObserverPtr make_noop_observer() {
    return std::make_shared<NoopObserver>();
}

}
