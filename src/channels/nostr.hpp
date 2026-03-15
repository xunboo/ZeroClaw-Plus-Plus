#pragma once

/// Nostr channel ?NIP-04 (legacy) and NIP-17 (gift-wrapped) private messages.

#include <string>
#include <vector>
#include <functional>
#include "traits.hpp"

namespace zeroclaw {
namespace channels {

/// Nostr channel supporting NIP-04 and NIP-17 private messages
class NostrChannel : public Channel {
public:
    NostrChannel(const std::string& private_key,
                 const std::vector<std::string>& relays,
                 const std::vector<std::string>& allowed_pubkeys);

    std::string name() const override { return "nostr"; }
    bool send(const SendMessage& message) override;
    bool listen(std::function<void(const ChannelMessage&)> callback) override;
    bool health_check() const override;

private:
    std::string private_key_;
    std::vector<std::string> relays_;
    std::vector<std::string> allowed_pubkeys_;
    bool allow_all_ = false;
};

} // namespace channels
} // namespace zeroclaw

