#pragma once

/// Tunnel module — reverse tunnel providers for webhook/API exposure.
/// Supports Cloudflare Tunnel, ngrok, Tailscale Funnel, and custom endpoints.

#include <string>
#include <optional>
#include <memory>

namespace zeroclaw {
namespace tunnel {

/// Tunnel status
enum class TunnelStatus {
    Inactive,
    Starting,
    Active,
    Error
};

/// Abstract tunnel provider
class TunnelProvider {
public:
    virtual ~TunnelProvider() = default;
    virtual std::string name() const = 0;
    virtual TunnelStatus status() const = 0;
    virtual std::optional<std::string> start(uint16_t local_port) = 0;
    virtual bool stop() = 0;
    virtual std::optional<std::string> public_url() const = 0;
};

/// Cloudflare Tunnel — uses cloudflared
class CloudflareTunnel : public TunnelProvider {
public:
    explicit CloudflareTunnel(const std::optional<std::string>& token = std::nullopt);
    std::string name() const override { return "cloudflare"; }
    TunnelStatus status() const override { return status_; }
    std::optional<std::string> start(uint16_t local_port) override;
    bool stop() override;
    std::optional<std::string> public_url() const override;
private:
    std::optional<std::string> token_;
    TunnelStatus status_ = TunnelStatus::Inactive;
    std::string url_;
};

/// ngrok tunnel
class NgrokTunnel : public TunnelProvider {
public:
    explicit NgrokTunnel(const std::optional<std::string>& auth_token = std::nullopt);
    std::string name() const override { return "ngrok"; }
    TunnelStatus status() const override { return status_; }
    std::optional<std::string> start(uint16_t local_port) override;
    bool stop() override;
    std::optional<std::string> public_url() const override;
private:
    std::optional<std::string> auth_token_;
    TunnelStatus status_ = TunnelStatus::Inactive;
    std::string url_;
};

/// Tailscale Funnel
class TailscaleTunnel : public TunnelProvider {
public:
    std::string name() const override { return "tailscale"; }
    TunnelStatus status() const override { return status_; }
    std::optional<std::string> start(uint16_t local_port) override;
    bool stop() override;
    std::optional<std::string> public_url() const override;
private:
    TunnelStatus status_ = TunnelStatus::Inactive;
    std::string url_;
};

/// Custom static endpoint (no tunnel, just a known URL)
class CustomEndpoint : public TunnelProvider {
public:
    explicit CustomEndpoint(const std::string& url);
    std::string name() const override { return "custom"; }
    TunnelStatus status() const override { return TunnelStatus::Active; }
    std::optional<std::string> start(uint16_t /*local_port*/) override { return url_; }
    bool stop() override { return true; }
    std::optional<std::string> public_url() const override { return url_; }
private:
    std::string url_;
};

/// No tunnel — disabled
class NoneTunnel : public TunnelProvider {
public:
    std::string name() const override { return "none"; }
    TunnelStatus status() const override { return TunnelStatus::Inactive; }
    std::optional<std::string> start(uint16_t) override { return std::nullopt; }
    bool stop() override { return true; }
    std::optional<std::string> public_url() const override { return std::nullopt; }
};

/// Create a tunnel provider by name
std::unique_ptr<TunnelProvider> create_tunnel(const std::string& provider_name,
                                                const std::optional<std::string>& token = std::nullopt);

} // namespace tunnel
} // namespace zeroclaw
