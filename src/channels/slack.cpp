#include "slack.hpp"
#include "../http/http_client.hpp"
#include <thread>
#include <chrono>
#include <regex>
#include <algorithm>
#include "nlohmann/json.hpp"

namespace zeroclaw {
namespace channels {

static const std::string SLACK_API_BASE = "https://slack.com/api";

SlackChannel::SlackChannel(const std::string& bot_token,
                            const std::optional<std::string>& channel_id,
                            const std::vector<std::string>& allowed_users,
                            bool mention_only,
                            const std::vector<std::string>& group_reply_allowed_sender_ids,
                            const std::vector<std::string>& channel_ids)
    : bot_token_(bot_token), channel_id_(channel_id), allowed_users_(allowed_users),
      mention_only_(mention_only),
      group_reply_allowed_sender_ids_(group_reply_allowed_sender_ids),
      channel_ids_(channel_ids) {}

bool SlackChannel::is_user_allowed(const std::string& user_id) const {
    if (allowed_users_.empty()) return false;
    for (const auto& allowed : allowed_users_) {
        if (allowed == "*" || allowed == user_id) return true;
    }
    return false;
}

// Matching Rust: channels starting with 'C' are public channels,
// 'G' are group/private channels — i.e., multi-person channels.
bool SlackChannel::is_group_channel_id(const std::string& cid) const {
    if (cid.empty()) return false;
    return cid[0] == 'C' || cid[0] == 'G';
}

// Matching Rust: detect if text contains <@BOT_USER_ID>
bool SlackChannel::contains_bot_mention(const std::string& text,
                                         const std::string& bot_user_id) const {
    if (bot_user_id.empty()) return false;
    std::string mention = "<@" + bot_user_id + ">";
    return text.find(mention) != std::string::npos;
}

// Matching Rust: remove all <@BOT_USER_ID> mentions and trim the result
std::string SlackChannel::strip_bot_mentions(const std::string& text,
                                              const std::string& bot_user_id) const {
    if (bot_user_id.empty()) return text;
    std::string mention = "<@" + bot_user_id + ">";
    std::string result = text;
    size_t pos;
    while ((pos = result.find(mention)) != std::string::npos) {
        result.erase(pos, mention.size());
    }
    // Trim leading/trailing whitespace
    size_t start = result.find_first_not_of(" \t\r\n");
    size_t end = result.find_last_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    return result.substr(start, end - start + 1);
}

// Matching Rust: return thread_ts if the message is a threaded reply (thread_ts != ts)
std::optional<std::string> SlackChannel::inbound_thread_ts(const nlohmann::json& msg) const {
    if (!msg.contains("thread_ts") || !msg.contains("ts")) return std::nullopt;
    std::string thread_ts = msg["thread_ts"].get<std::string>();
    std::string ts = msg["ts"].get<std::string>();
    if (thread_ts == ts) return std::nullopt;  // root of thread, not a reply
    return thread_ts;
}

// Matching Rust: deduped union of channel_id_ and channel_ids_
std::vector<std::string> SlackChannel::scoped_channel_ids() const {
    std::vector<std::string> result;
    if (channel_id_.has_value() && !channel_id_->empty()) {
        result.push_back(*channel_id_);
    }
    for (const auto& id : channel_ids_) {
        if (std::find(result.begin(), result.end(), id) == result.end()) {
            result.push_back(id);
        }
    }
    return result;
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
    // Resolve bot user ID once for mention detection
    std::string bot_user_id;
    auto bot_id_opt = get_bot_user_id();
    if (bot_id_opt.has_value()) {
        bot_user_id = *bot_id_opt;
    }

    // Determine the set of channels to poll (matching Rust's scoped_channel_ids())
    auto poll_channels = scoped_channel_ids();
    if (poll_channels.empty()) return false;

    http::HttpClient client;
    client.with_bearer_token(bot_token_);

    // Per-channel cursor tracking
    std::unordered_map<std::string, std::string> latest_ts_map;

    while (true) {
        for (const auto& chan_id : poll_channels) {
            std::string& latest_ts = latest_ts_map[chan_id];

            nlohmann::json params = {
                {"channel", chan_id},
                {"limit", 10}
            };
            if (!latest_ts.empty()) {
                params["oldest"] = latest_ts;
            }

            auto resp = client.post_json(SLACK_API_BASE + "/conversations.history", params);
            if (!resp.ok()) continue;

            auto json = resp.json();
            if (!json.value("ok", false)) continue;

            if (!json.contains("messages") || !json["messages"].is_array()) continue;

            // Messages come newest-first, reverse for chronological
            auto& msgs = json["messages"];
            for (auto it = msgs.rbegin(); it != msgs.rend(); ++it) {
                auto& msg = *it;

                std::string ts = msg.value("ts", "");
                if (!latest_ts.empty() && ts <= latest_ts) continue;
                latest_ts = ts;

                // Skip bot messages
                if (msg.value("subtype", "") == "bot_message") continue;
                if (msg.contains("bot_id")) continue;

                std::string user = msg.value("user", "");

                // Matching Rust: apply group channel filtering
                bool is_group = is_group_channel_id(chan_id);
                if (is_group) {
                    // mention_only: skip if bot not @mentioned
                    if (mention_only_) {
                        std::string text = msg.value("text", "");
                        if (!contains_bot_mention(text, bot_user_id)) continue;
                    }
                    // group_reply_allowed_sender_ids: if non-empty, restrict to listed users
                    if (!group_reply_allowed_sender_ids_.empty()) {
                        bool allowed = false;
                        for (const auto& sid : group_reply_allowed_sender_ids_) {
                            if (sid == user || sid == "*") { allowed = true; break; }
                        }
                        if (!allowed) continue;
                    }
                }

                if (!is_user_allowed(user)) continue;

                std::string text = msg.value("text", "");
                // Strip bot mentions from text before forwarding
                if (!bot_user_id.empty()) {
                    text = strip_bot_mentions(text, bot_user_id);
                }
                if (text.empty()) continue;

                ChannelMessage ch_msg;
                ch_msg.content = text;
                ch_msg.sender = user;
                ch_msg.reply_target = chan_id;
                ch_msg.channel = "slack";
                ch_msg.id = ts;

                // Thread support: inbound_thread_ts() returns thread_ts for replies
                auto thread_ts = inbound_thread_ts(msg);
                if (thread_ts.has_value()) {
                    ch_msg.thread_ts = *thread_ts;
                }

                callback(ch_msg);
            }
        }

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
