#include "gateway.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>
#include <openssl/hmac.h>

namespace zeroclaw::gateway {

std::string hash_webhook_secret(const std::string& value) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(value.c_str()), value.size(), hash);
    
    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }
    return oss.str();
}

std::string webhook_memory_key() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::ostringstream oss;
    oss << "webhook_msg_";
    for (int i = 0; i < 32; ++i) {
        oss << std::hex << dis(gen);
    }
    return oss.str();
}

std::string whatsapp_memory_key(const std::string& sender, const std::string& id) {
    return "whatsapp_" + sender + "_" + id;
}

std::string linq_memory_key(const std::string& sender, const std::string& id) {
    return "linq_" + sender + "_" + id;
}

std::string nextcloud_talk_memory_key(const std::string& sender, const std::string& id) {
    return "nextcloud_talk_" + sender + "_" + id;
}

SlidingWindowRateLimiter::SlidingWindowRateLimiter(uint32_t limit_per_window,
                                                   std::chrono::seconds window,
                                                   size_t max_keys)
    : limit_per_window_(limit_per_window)
    , window_(window)
    , max_keys_(std::max(max_keys, size_t(1)))
    , last_sweep_(std::chrono::steady_clock::now()) {}

void SlidingWindowRateLimiter::prune_stale(std::chrono::steady_clock::time_point cutoff) {
    for (auto it = requests_.begin(); it != requests_.end(); ) {
        auto& timestamps = it->second;
        timestamps.erase(
            std::remove_if(timestamps.begin(), timestamps.end(),
                          [cutoff](const auto& t) { return t <= cutoff; }),
            timestamps.end()
        );
        if (timestamps.empty()) {
            it = requests_.erase(it);
        } else {
            ++it;
        }
    }
}

bool SlidingWindowRateLimiter::allow(const std::string& key) {
    if (limit_per_window_ == 0) {
        return true;
    }

    auto now = std::chrono::steady_clock::now();
    auto cutoff = now - window_;

    std::lock_guard<std::mutex> lock(mutex_);

    if (now - last_sweep_ >= std::chrono::seconds(RATE_LIMITER_SWEEP_INTERVAL_SECS)) {
        prune_stale(cutoff);
        last_sweep_ = now;
    }

    if (requests_.find(key) == requests_.end() && requests_.size() >= max_keys_) {
        prune_stale(cutoff);
        last_sweep_ = now;

        if (requests_.size() >= max_keys_) {
            std::string evict_key;
            std::chrono::steady_clock::time_point oldest = now;
            for (const auto& [k, timestamps] : requests_) {
                if (!timestamps.empty() && timestamps.back() < oldest) {
                    oldest = timestamps.back();
                    evict_key = k;
                }
            }
            if (!evict_key.empty()) {
                requests_.erase(evict_key);
            }
        }
    }

    auto& entry = requests_[key];
    entry.erase(
        std::remove_if(entry.begin(), entry.end(),
                      [cutoff](const auto& t) { return t <= cutoff; }),
        entry.end()
    );

    if (entry.size() >= limit_per_window_) {
        return false;
    }

    entry.push_back(now);
    return true;
}

GatewayRateLimiter::GatewayRateLimiter(uint32_t pair_per_minute,
                                       uint32_t webhook_per_minute,
                                       size_t max_keys)
    : pair_(pair_per_minute, std::chrono::seconds(RATE_LIMIT_WINDOW_SECS), max_keys)
    , webhook_(webhook_per_minute, std::chrono::seconds(RATE_LIMIT_WINDOW_SECS), max_keys) {}

bool GatewayRateLimiter::allow_pair(const std::string& key) {
    return pair_.allow(key);
}

bool GatewayRateLimiter::allow_webhook(const std::string& key) {
    return webhook_.allow(key);
}

IdempotencyStore::IdempotencyStore(std::chrono::seconds ttl, size_t max_keys)
    : ttl_(ttl)
    , max_keys_(std::max(max_keys, size_t(1))) {}

bool IdempotencyStore::record_if_new(const std::string& key) {
    auto now = std::chrono::steady_clock::now();
    
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto it = keys_.begin(); it != keys_.end(); ) {
        if (now - it->second >= ttl_) {
            it = keys_.erase(it);
        } else {
            ++it;
        }
    }

    if (keys_.find(key) != keys_.end()) {
        return false;
    }

    if (keys_.size() >= max_keys_) {
        std::string evict_key;
        auto oldest = now;
        for (const auto& [k, seen_at] : keys_) {
            if (seen_at < oldest) {
                oldest = seen_at;
                evict_key = k;
            }
        }
        if (!evict_key.empty()) {
            keys_.erase(evict_key);
        }
    }

    keys_[key] = now;
    return true;
}

std::optional<IpAddr> IpAddr::parse(const std::string& value) {
    auto trimmed = value;
    size_t start = trimmed.find_first_not_of(" \t\"");
    size_t end = trimmed.find_last_not_of(" \t\"");
    if (start != std::string::npos && end != std::string::npos) {
        trimmed = trimmed.substr(start, end - start + 1);
    }
    
    if (trimmed.empty()) {
        return std::nullopt;
    }

    if (trimmed.find(':') != std::string::npos) {
        if (trimmed.front() == '[' && trimmed.back() == ']') {
            trimmed = trimmed.substr(1, trimmed.size() - 2);
        }
    }
    
    return IpAddr{trimmed};
}

std::optional<SocketAddr> SocketAddr::parse(const std::string& value) {
    auto colon_pos = value.rfind(':');
    if (colon_pos == std::string::npos) {
        return std::nullopt;
    }
    
    auto ip_str = value.substr(0, colon_pos);
    auto port_str = value.substr(colon_pos + 1);
    
    auto ip = IpAddr::parse(ip_str);
    if (!ip) {
        return std::nullopt;
    }
    
    try {
        uint16_t port = static_cast<uint16_t>(std::stoul(port_str));
        return SocketAddr{*ip, port};
    } catch (...) {
        return std::nullopt;
    }
}

namespace http {

std::optional<std::string> HeaderMap::get(const std::string& key) const {
    auto it = headers.find(key);
    if (it != headers.end()) {
        return it->second;
    }
    for (const auto& [k, v] : headers) {
        if (k.size() == key.size()) {
            bool match = true;
            for (size_t i = 0; i < k.size(); ++i) {
                if (std::tolower(k[i]) != std::tolower(key[i])) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return v;
            }
        }
    }
    return std::nullopt;
}

void HeaderMap::set(const std::string& key, const std::string& value) {
    headers[key] = value;
}

}

std::optional<IpAddr> forwarded_client_ip(const http::HeaderMap& headers) {
    if (auto xff = headers.get("X-Forwarded-For")) {
        std::istringstream iss(*xff);
        std::string candidate;
        while (std::getline(iss, candidate, ',')) {
            if (auto ip = IpAddr::parse(candidate)) {
                return ip;
            }
        }
    }
    
    if (auto real_ip = headers.get("X-Real-IP")) {
        return IpAddr::parse(*real_ip);
    }
    
    return std::nullopt;
}

std::string client_key_from_request(const std::optional<SocketAddr>& peer_addr,
                                    const http::HeaderMap& headers,
                                    bool trust_forwarded_headers) {
    if (trust_forwarded_headers) {
        if (auto ip = forwarded_client_ip(headers)) {
            return ip->value;
        }
    }
    
    if (peer_addr) {
        return peer_addr->ip.value;
    }
    
    return "unknown";
}

size_t normalize_max_keys(size_t configured, size_t fallback) {
    if (configured == 0) {
        return std::max(fallback, size_t(1));
    }
    return configured;
}

bool verify_whatsapp_signature(const std::string& app_secret,
                               const std::vector<uint8_t>& body,
                               const std::string& signature_header) {
    if (signature_header.compare(0, 7, "sha256=") != 0) {
        return false;
    }
    
    auto hex_sig = signature_header.substr(7);
    std::vector<uint8_t> expected(hex_sig.size() / 2);
    for (size_t i = 0; i < expected.size(); ++i) {
        std::string byte_str = hex_sig.substr(i * 2, 2);
        expected[i] = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
    }
    
    unsigned char result[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    
    HMAC(EVP_sha256(), app_secret.c_str(), app_secret.size(),
         body.data(), body.size(), result, &len);
    
    if (len != expected.size()) {
        return false;
    }
    
    return CRYPTO_memcmp(result, expected.data(), len) == 0;
}

}
