#pragma once

#include "types.hpp"
#include <memory>
#include <string>

namespace zeroclaw::observability {

class Observer {
public:
    virtual ~Observer() = default;
    virtual void record_event(const ObserverEvent& event) = 0;
    virtual void record_metric(const ObserverMetric& metric) = 0;
    virtual void flush() {}
    virtual std::string name() const = 0;
};

using ObserverPtr = std::shared_ptr<Observer>;

}
