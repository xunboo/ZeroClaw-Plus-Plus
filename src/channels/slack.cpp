#include "slack.hpp"
#include "../http/http_client.hpp"

namespace zeroclaw {
namespace channels {

static const std::string SLACK_API_BASE = "https://slack.com/api";

SlackChannel::SlackChannel(const std::string& bot_token,
                            const std::optional<std::string>& channel_id,
                            const std::vector<std::string>& allowed_users)
    : bot_token_(bot_token), channel_id_(channel_id), allowed_users_(allowed_users) {}

bool SlackChannel::is_user_allowed(const std::string& user_id) const {
    if (allowed_users_.empty()) return false;
    for (const auto& allowed : allowed_users_) {
        if (allowed == "*" || allowed == user_id) return true;
    }
    return false;
}

std::optional<std::string> SlackChannel::get_bot_user_id() const {
    http::HttpClient client;
    client.with_bearer_token(bot_token_);

    auto resp = client.post_json(SLACK_API_BASE + "/auth.test", nlohmann::json::object());
    if (!resp.ok()) return std::nullopt;

    auto json = resp.json();
    if (json.value("ok", false) && json.contains("user_id")) {
        return json["user_id"].get<std::string>();
    }
    return std::nullopt;
}

std::vector<std::string> SlackChannel::list_accessible_channels() const {
    http::HttpClient client;
    client.with_bearer_token(bot_token_);

    std::vector<std::string> channels;
    std::string cursor;

    do {
        nlohmann::json params = {
            {"types", "public_channel,private_channel"},
            {"limit", 200}
        };
        if (!cursor.empty()) {
            params["cursor"] = cursor;
        }

        auto resp = client.post_json(SLACK_API_BASE + "/conversations.list", params);
        if (!resp.ok()) break;

        auto json = resp.json();
        if (!json.value("ok", false)) break;

        if (json.contains("channels") && json["channels"].is_array()) {
            for (const auto& ch : json["channels"]) {
                if (ch.contains("id")) {
                    channels.push_back(ch["id"].get<std::string>());
                }
            }
        }

        // Pagination
        cursor.clear();
        if (json.contains("response_metadata") &&
            json["response_metadata"].contains("next_cursor")) {
            cursor = json["response_metadata"]["next_cursor"].get<std::string>();
        }
    } while (!cursor.empty());

    return channels;
}

bool SlackChannel::send(const SendMessage& message) {
    http::HttpClient client;
    client.with_bearer_token(bot_token_);

    nlohmann::json body = {
        {"channel", message.recipient},
        {"text", message.content}
    };

    // Thread reply support
    if (message.thread_ts.has_value() && !message.thread_ts->empty()) {
        body["thread_ts"] = *message.thread_ts;
    }

    auto resp = client.post_json(SLACK_API_BASE + "/chat.postMessage", body);
    if (!resp.ok()) return false;

    auto json = resp.json();
    return json.value("ok", false);
}

bool SlackChannel::listen(std::function<void(const ChannelMessage&)> callback) {
    // Slack prefers Socket Mode (WebSocket) for real-time events.
    // As a fallback, we poll conversations.history.
    // A full implementation would use app_token for WebSocket connection.
    if (!channel_id_.has_value()) return false;

    http::HttpClient client;
    client.with_bearer_token(bot_token_);

    std::string latest_ts;

    while (true) {
        nlohmann::json params = {
            {"channel", *channel_id_},
            {"limit", 10}
        };
        if (!latest_ts.empty()) {
            params["oldest"] = latest_ts;
        }

        auto resp = client.post_json(SLACK_API_BASE + "/conversations.history", params);
        if (!resp.ok()) continue;

        auto json = resp.json();
        if (!json.value("ok", false)) continue;

        if (json.contains("messages") && json["messages"].is_array()) {
            // Messages come newest-first, reverse for chronological
            auto& msgs = json["messages"];
            for (auto it = msgs.rbegin(); it != msgs.rend(); ++it) {
                auto& msg = *it;

                std::string ts = msg.value("ts", "");
                if (!latest_ts.empty() && ts <= latest_ts) continue;
                latest_ts = ts;

                // Skip bot messages
                if (msg.value("subtype", "") == "bot_message") continue;

                std::string user = msg.value("user", "");
                if (!is_user_allowed(user)) continue;

                ChannelMessage ch_msg;
                ch_msg.content = msg.value("text", "");
                ch_msg.sender = user;
                ch_msg.reply_target = *channel_id_;
                ch_msg.channel = "slack";
                ch_msg.id = ts;

                // Thread support
                if (msg.contains("thread_ts")) {
                    ch_msg.reply_target += ":" + msg["thread_ts"].get<std::string>();
                }

                if (!ch_msg.content.empty()) {
                    callback(ch_msg);
                }
            }
        }

        // Poll interval
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

bool SlackChannel::health_check() const {
    if (bot_token_.empty()) return false;

    http::HttpClient client;
    client.with_bearer_token(bot_token_);

    auto resp = client.post_json(SLACK_API_BASE + "/auth.test", nlohmann::json::object());
    if (!resp.ok()) return false;

    auto json = resp.json();
    return json.value("ok", false);
}

} // namespace channels
} // namespace zeroclaw


