#pragma once

#include "gateway.hpp"
#include <memory>
#include <functional>
#include <string>
#include "observability/observer.hpp"

namespace zeroclaw::gateway::sse {

struct Event {
    std::string data;
    std::optional<std::string> event_type;
    std::optional<std::string> id;
};

class ISseConnection {
public:
    virtual ~ISseConnection() = default;
    virtual void send(const Event& event) = 0;
    virtual void close() = 0;
    virtual bool is_open() const = 0;
};

http::Response handle_sse_events(AppState& state, const http::Request& req);

class BroadcastObserver : public observability::Observer {
public:
    BroadcastObserver(std::unique_ptr<observability::Observer> inner,
                      std::function<void(const std::string&)> broadcaster);
    
    void record_event(const observability::ObserverEvent& event) override;
    void record_metric(const observability::ObserverMetric& metric) override;
    void flush() override;
    std::string name() const override;

private:
    std::unique_ptr<observability::Observer> inner_;
    std::function<void(const std::string&)> broadcaster_;
};

}
