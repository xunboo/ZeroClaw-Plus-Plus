#pragma once

#include "traits.hpp"
#include "log.hpp"
#include "multi.hpp"
#include "noop.hpp"
#include "otel.hpp"
#include "prometheus.hpp"
#include "verbose.hpp"
#include "runtime_trace.hpp"
#include "config/config.hpp"
#include <string>

namespace zeroclaw::observability {

inline ObserverPtr create_observer(const config::ObservabilityConfig& config) {
    if (config.backend == "log") {
        return make_log_observer();
    }
    if (config.backend == "prometheus") {
        return make_prometheus_observer();
    }
    if (config.backend == "otel" || config.backend == "opentelemetry" || config.backend == "otlp") {
        return make_otel_observer(config.otel_endpoint.value_or("http://localhost:4318"), config.otel_service_name.value_or("zeroclaw"));
    }
    if (config.backend == "verbose") {
        return make_verbose_observer();
    }
    return make_noop_observer();
}

inline void init_runtime_trace_from_config(const config::ObservabilityConfig& config, const std::string& workspace_dir) {
    auto mode = storage_mode_from_string(config.runtime_trace_mode);
    if (mode == RuntimeTraceStorageMode::None) {
        disable_runtime_trace();
        return;
    }

    std::string path = config.runtime_trace_path;
    if (!path.empty() && path[0] != '/' && path[1] != ':') {
        path = workspace_dir + "/" + path;
    }

    init_runtime_trace(mode, config.runtime_trace_max_entries, path);
}

}
