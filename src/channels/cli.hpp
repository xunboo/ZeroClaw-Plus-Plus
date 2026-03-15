#pragma once

/// CLI channel ?stdin/stdout, always available, zero deps.

#include <string>
#include <iostream>
#include <functional>
#include <chrono>
#include "traits.hpp"

namespace zeroclaw {
namespace channels {

/// CLI channel implementation ?reads from stdin, writes to stdout
class CliChannel : public Channel {
public:
    CliChannel() = default;

    std::string name() const override { return "cli"; }

    bool send(const SendMessage& message) override {
        std::cout << message.content << std::endl;
        return true;
    }

    bool listen(std::function<void(const ChannelMessage&)> callback) override {
        std::string line;
        while (std::getline(std::cin, line)) {
            // Trim whitespace
            size_t s = line.find_first_not_of(" \t\n\r");
            size_t e = line.find_last_not_of(" \t\n\r");
            if (s == std::string::npos) continue;
            line = line.substr(s, e - s + 1);

            if (line == "/quit" || line == "/exit") break;

            auto now = std::chrono::system_clock::now();
            auto epoch = now.time_since_epoch();
            uint64_t ts = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::seconds>(epoch).count());

            ChannelMessage msg;
            msg.id = "cli_" + std::to_string(ts);
            msg.sender = "user";
            msg.reply_target = "user";
            msg.content = line;
            msg.channel = "cli";
            msg.timestamp = ts;

            callback(msg);
        }
    }

    bool health_check() const override { return true; }
};

} // namespace channels
} // namespace zeroclaw

