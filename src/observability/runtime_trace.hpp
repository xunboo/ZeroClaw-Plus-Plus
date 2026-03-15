#pragma once

#include "types.hpp"
#include <ctime>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include <random>
#include <iomanip>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/stat.h>
#endif

namespace zeroclaw::observability {

enum class RuntimeTraceStorageMode {
    None,
    Rolling,
    Full
};

inline RuntimeTraceStorageMode storage_mode_from_string(const std::string& s) {
    std::string lower;
    for (char c : s) lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    
    if (lower == "none") return RuntimeTraceStorageMode::None;
    if (lower == "rolling") return RuntimeTraceStorageMode::Rolling;
    if (lower == "full") return RuntimeTraceStorageMode::Full;
    return RuntimeTraceStorageMode::None;
}

struct RuntimeTraceEvent {
    std::string id;
    std::string timestamp;
    std::string event_type;
    std::optional<std::string> channel;
    std::optional<std::string> provider;
    std::optional<std::string> model;
    std::optional<std::string> turn_id;
    std::optional<bool> success;
    std::optional<std::string> message;
    std::string payload;

    std::string to_json() const {
        std::ostringstream oss;
        oss << "{\"id\":\"" << id << "\""
            << ",\"timestamp\":\"" << timestamp << "\""
            << ",\"event_type\":\"" << event_type << "\"";
        
        if (channel) oss << ",\"channel\":\"" << *channel << "\"";
        if (provider) oss << ",\"provider\":\"" << *provider << "\"";
        if (model) oss << ",\"model\":\"" << *model << "\"";
        if (turn_id) oss << ",\"turn_id\":\"" << *turn_id << "\"";
        if (success) oss << ",\"success\":" << (*success ? "true" : "false");
        if (message) oss << ",\"message\":\"" << *message << "\"";
        oss << ",\"payload\":" << (payload.empty() ? "null" : payload);
        oss << "}";
        return oss.str();
    }

    static std::optional<RuntimeTraceEvent> from_json(const std::string& line) {
        RuntimeTraceEvent e;
        
        auto extract = [&line](const std::string& key) -> std::optional<std::string> {
            std::string search = "\"" + key + "\":\"";
            auto pos = line.find(search);
            if (pos == std::string::npos) {
                search = "\"" + key + "\":";
                pos = line.find(search);
                if (pos == std::string::npos) return std::nullopt;
                pos += search.length();
                auto end = line.find_first_of(",}", pos);
                if (end == std::string::npos) return std::nullopt;
                return line.substr(pos, end - pos);
            }
            pos += search.length();
            auto end = line.find('"', pos);
            if (end == std::string::npos) return std::nullopt;
            return line.substr(pos, end - pos);
        };

        auto id_opt = extract("id");
        if (!id_opt) return std::nullopt;
        e.id = *id_opt;

        auto ts_opt = extract("timestamp");
        if (!ts_opt) return std::nullopt;
        e.timestamp = *ts_opt;

        auto et_opt = extract("event_type");
        if (!et_opt) return std::nullopt;
        e.event_type = *et_opt;

        e.channel = extract("channel");
        e.provider = extract("provider");
        e.model = extract("model");
        e.turn_id = extract("turn_id");
        e.message = extract("message");
        e.payload = extract("payload").value_or("");

        auto succ = extract("success");
        if (succ) {
            e.success = (*succ == "true");
        }

        return e;
    }
};

class RuntimeTraceLogger {
public:
    RuntimeTraceLogger(RuntimeTraceStorageMode mode, size_t max_entries, std::string path)
        : mode_(mode)
        , max_entries_(std::max(size_t(1), max_entries))
        , path_(std::move(path)) {}

    void append(const RuntimeTraceEvent& event) {
        if (mode_ == RuntimeTraceStorageMode::None) return;

        std::lock_guard<std::mutex> lock(mutex_);

        {
            std::ofstream file(path_, std::ios::app);
            if (!file) return;
            file << event.to_json() << "\n";
        }

        if (mode_ == RuntimeTraceStorageMode::Rolling) {
            trim_to_last_entries();
        }
    }

private:
    void trim_to_last_entries() {
        std::ifstream in(path_);
        if (!in) return;
        
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(in, line)) {
            if (!line.empty()) lines.push_back(line);
        }
        in.close();

        if (lines.size() <= max_entries_) return;

        size_t keep_from = lines.size() - max_entries_;
        std::ofstream out(path_, std::ios::trunc);
        if (!out) return;
        for (size_t i = keep_from; i < lines.size(); ++i) {
            out << lines[i] << "\n";
        }
    }

    RuntimeTraceStorageMode mode_;
    size_t max_entries_;
    std::string path_;
    std::mutex mutex_;
};

inline std::string generate_uuid() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << dis(gen);
    ss << "-4";
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    ss << dis2(gen);
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << dis(gen);
    return ss.str();
}

inline std::string current_timestamp_rfc3339() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    
    auto duration = now.time_since_epoch();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration) % 1000;
    oss << "." << std::setfill('0') << std::setw(3) << millis.count() << "Z";
    return oss.str();
}

class RuntimeTrace {
public:
    static RuntimeTrace& instance() {
        static RuntimeTrace instance;
        return instance;
    }

    void init(RuntimeTraceStorageMode mode, size_t max_entries, const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        logger_ = std::make_unique<RuntimeTraceLogger>(mode, max_entries, path);
    }

    void disable() {
        std::lock_guard<std::mutex> lock(mutex_);
        logger_.reset();
    }

    void record_event(
        const std::string& event_type,
        const std::optional<std::string>& channel = std::nullopt,
        const std::optional<std::string>& provider = std::nullopt,
        const std::optional<std::string>& model = std::nullopt,
        const std::optional<std::string>& turn_id = std::nullopt,
        const std::optional<bool>& success = std::nullopt,
        const std::optional<std::string>& message = std::nullopt,
        const std::string& payload = "{}") {
        
        std::lock_guard<std::mutex> lock(mutex_);
        if (!logger_) return;

        RuntimeTraceEvent event{
            generate_uuid(),
            current_timestamp_rfc3339(),
            event_type,
            channel,
            provider,
            model,
            turn_id,
            success,
            message,
            payload
        };
        logger_->append(event);
    }

    std::vector<RuntimeTraceEvent> load_events(
        const std::string& path,
        size_t limit = 100,
        const std::optional<std::string>& event_filter = std::nullopt,
        const std::optional<std::string>& contains = std::nullopt) {
        
        std::vector<RuntimeTraceEvent> events;

        std::ifstream file(path);
        if (!file) return events;
        
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            auto event = RuntimeTraceEvent::from_json(line);
            if (event) events.push_back(*event);
        }

        if (event_filter && !event_filter->empty()) {
            std::string filter_lower;
            for (char c : *event_filter) filter_lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            events.erase(
                std::remove_if(events.begin(), events.end(), [&](const RuntimeTraceEvent& e) {
                    std::string et_lower;
                    for (char c : e.event_type) et_lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    return et_lower != filter_lower;
                }),
                events.end()
            );
        }

        if (contains && !contains->empty()) {
            std::string needle_lower;
            for (char c : *contains) needle_lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            events.erase(
                std::remove_if(events.begin(), events.end(), [&](const RuntimeTraceEvent& e) {
                    std::string haystack = e.event_type + " " + 
                        e.message.value_or("") + " " + 
                        e.payload + " " +
                        e.channel.value_or("") + " " +
                        e.provider.value_or("") + " " +
                        e.model.value_or("");
                    std::string haystack_lower;
                    for (char c : haystack) haystack_lower += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    return haystack_lower.find(needle_lower) == std::string::npos;
                }),
                events.end()
            );
        }

        if (events.size() > limit) {
            events.erase(events.begin(), events.begin() + (events.size() - limit));
        }

        std::reverse(events.begin(), events.end());
        return events;
    }

    std::optional<RuntimeTraceEvent> find_event_by_id(const std::string& path, const std::string& id) {
        std::ifstream file(path);
        if (!file) return std::nullopt;
        
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) lines.push_back(line);
        }

        for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
            auto event = RuntimeTraceEvent::from_json(*it);
            if (event && event->id == id) return event;
        }

        return std::nullopt;
    }

private:
    RuntimeTrace() = default;
    std::unique_ptr<RuntimeTraceLogger> logger_;
    std::mutex mutex_;
};

inline void init_runtime_trace(RuntimeTraceStorageMode mode, size_t max_entries, const std::string& path) {
    RuntimeTrace::instance().init(mode, max_entries, path);
}

inline void disable_runtime_trace() {
    RuntimeTrace::instance().disable();
}

inline void record_runtime_trace_event(
    const std::string& event_type,
    const std::optional<std::string>& channel = std::nullopt,
    const std::optional<std::string>& provider = std::nullopt,
    const std::optional<std::string>& model = std::nullopt,
    const std::optional<std::string>& turn_id = std::nullopt,
    const std::optional<bool>& success = std::nullopt,
    const std::optional<std::string>& message = std::nullopt,
    const std::string& payload = "{}") {
    RuntimeTrace::instance().record_event(event_type, channel, provider, model, turn_id, success, message, payload);
}

}
