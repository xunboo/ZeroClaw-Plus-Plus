#include "tunnel.hpp"

namespace zeroclaw {
namespace tunnel {

// Cloudflare
CloudflareTunnel::CloudflareTunnel(const std::optional<std::string>& token) : token_(token) {}
std::optional<std::string> CloudflareTunnel::start(uint16_t) { status_ = TunnelStatus::Active; return url_; }
bool CloudflareTunnel::stop() { status_ = TunnelStatus::Inactive; return true; }
std::optional<std::string> CloudflareTunnel::public_url() const { return url_.empty() ? std::nullopt : std::optional(url_); }

// ngrok
NgrokTunnel::NgrokTunnel(const std::optional<std::string>& auth_token) : auth_token_(auth_token) {}
std::optional<std::string> NgrokTunnel::start(uint16_t) { status_ = TunnelStatus::Active; return url_; }
bool NgrokTunnel::stop() { status_ = TunnelStatus::Inactive; return true; }
std::optional<std::string> NgrokTunnel::public_url() const { return url_.empty() ? std::nullopt : std::optional(url_); }

// Tailscale
std::optional<std::string> TailscaleTunnel::start(uint16_t) { status_ = TunnelStatus::Active; return url_; }
bool TailscaleTunnel::stop() { status_ = TunnelStatus::Inactive; return true; }
std::optional<std::string> TailscaleTunnel::public_url() const { return url_.empty() ? std::nullopt : std::optional(url_); }

// Custom
CustomEndpoint::CustomEndpoint(const std::string& url) : url_(url) {}

// Factory
std::unique_ptr<TunnelProvider> create_tunnel(const std::string& provider_name,
                                                const std::optional<std::string>& token) {
    if (provider_name == "cloudflare") return std::make_unique<CloudflareTunnel>(token);
    if (provider_name == "ngrok") return std::make_unique<NgrokTunnel>(token);
    if (provider_name == "tailscale") return std::make_unique<TailscaleTunnel>();
    if (provider_name == "custom") return std::make_unique<CustomEndpoint>(token.value_or(""));
    return std::make_unique<NoneTunnel>();
}

} // namespace tunnel
} // namespace zeroclaw
