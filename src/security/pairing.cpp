#include "pairing.hpp"
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstring>

namespace {

// ── Minimal SHA-256 implementation for token hashing ─────────────

static const uint32_t SHA256_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static uint32_t sha256_rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static uint32_t sha256_ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
static uint32_t sha256_maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
static uint32_t sha256_sig0(uint32_t x) { return sha256_rotr(x, 2) ^ sha256_rotr(x, 13) ^ sha256_rotr(x, 22); }
static uint32_t sha256_sig1(uint32_t x) { return sha256_rotr(x, 6) ^ sha256_rotr(x, 11) ^ sha256_rotr(x, 25); }
static uint32_t sha256_gam0(uint32_t x) { return sha256_rotr(x, 7) ^ sha256_rotr(x, 18) ^ (x >> 3); }
static uint32_t sha256_gam1(uint32_t x) { return sha256_rotr(x, 17) ^ sha256_rotr(x, 19) ^ (x >> 10); }

static std::vector<uint8_t> sha256(const uint8_t* data, size_t len) {
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    // Padding
    uint64_t bit_len = static_cast<uint64_t>(len) * 8;
    std::vector<uint8_t> msg(data, data + len);
    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) msg.push_back(0);
    for (int i = 7; i >= 0; --i) msg.push_back(static_cast<uint8_t>(bit_len >> (i * 8)));

    // Process blocks
    for (size_t offset = 0; offset < msg.size(); offset += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (uint32_t(msg[offset + i * 4]) << 24) |
                   (uint32_t(msg[offset + i * 4 + 1]) << 16) |
                   (uint32_t(msg[offset + i * 4 + 2]) << 8) |
                   uint32_t(msg[offset + i * 4 + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            w[i] = sha256_gam1(w[i - 2]) + w[i - 7] + sha256_gam0(w[i - 15]) + w[i - 13];
        }

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

        for (int i = 0; i < 64; ++i) {
            uint32_t t1 = hh + sha256_sig1(e) + sha256_ch(e, f, g) + SHA256_K[i] + w[i];
            uint32_t t2 = sha256_sig0(a) + sha256_maj(a, b, c);
            hh = g; g = f; f = e; e = d + t1;
            d = c; c = b; b = a; a = t1 + t2;
        }

        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    std::vector<uint8_t> digest(32);
    for (int i = 0; i < 8; ++i) {
        digest[i * 4 + 0] = static_cast<uint8_t>(h[i] >> 24);
        digest[i * 4 + 1] = static_cast<uint8_t>(h[i] >> 16);
        digest[i * 4 + 2] = static_cast<uint8_t>(h[i] >> 8);
        digest[i * 4 + 3] = static_cast<uint8_t>(h[i]);
    }
    return digest;
}

static std::string bytes_to_hex(const std::vector<uint8_t>& bytes) {
    std::ostringstream oss;
    for (uint8_t b : bytes) {
        oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}

} // anonymous namespace

namespace zeroclaw {
namespace security {

// ── Token hashing ────────────────────────────────────────────────

std::string hash_token(const std::string& token) {
    auto digest = sha256(reinterpret_cast<const uint8_t*>(token.data()), token.size());
    return bytes_to_hex(digest);
}

bool is_token_hash(const std::string& value) {
    if (value.size() != 64) return false;
    return std::all_of(value.begin(), value.end(), [](char c) {
        return std::isxdigit(static_cast<unsigned char>(c));
    });
}

bool constant_time_eq(const std::string& a, const std::string& b) {
    size_t len_diff = a.size() ^ b.size();
    size_t max_len = std::max(a.size(), b.size());
    uint8_t byte_diff = 0;
    for (size_t i = 0; i < max_len; ++i) {
        uint8_t x = (i < a.size()) ? static_cast<uint8_t>(a[i]) : 0;
        uint8_t y = (i < b.size()) ? static_cast<uint8_t>(b[i]) : 0;
        byte_diff |= x ^ y;
    }
    return (len_diff == 0) && (byte_diff == 0);
}

bool is_public_bind(const std::string& host) {
    return host != "127.0.0.1" && host != "localhost" &&
           host != "::1" && host != "[::1]" &&
           host != "0:0:0:0:0:0:0:1";
}

std::string generate_pairing_code() {
    static constexpr uint32_t UPPER_BOUND = 1000000;
    static constexpr uint32_t REJECT_THRESHOLD = (0xFFFFFFFF / UPPER_BOUND) * UPPER_BOUND;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);

    while (true) {
        uint32_t raw = dist(gen);
        if (raw < REJECT_THRESHOLD) {
            std::ostringstream oss;
            oss << std::setw(6) << std::setfill('0') << (raw % UPPER_BOUND);
            return oss.str();
        }
    }
}

std::string generate_token() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint16_t> dist(0, 255);
    std::vector<uint8_t> bytes(32);
    for (auto& b : bytes) b = static_cast<uint8_t>(dist(gen));
    return "zc_" + bytes_to_hex(bytes);
}

// ── PairingGuard ─────────────────────────────────────────────────

PairingGuard::PairingGuard(bool require_pairing, const std::vector<std::string>& existing_tokens)
    : require_pairing_(require_pairing) {
    for (const auto& t : existing_tokens) {
        if (is_token_hash(t)) {
            paired_tokens_.insert(t);
        } else {
            paired_tokens_.insert(hash_token(t));
        }
    }

    if (require_pairing && paired_tokens_.empty()) {
        pairing_code_ = generate_pairing_code();
    }
}

std::optional<std::string> PairingGuard::pairing_code() const {
    std::lock_guard<std::mutex> lock(code_mutex_);
    return pairing_code_;
}

PairingGuard::PairResult PairingGuard::try_pair(const std::string& code, const std::string& client_id) {
    PairResult result;

    // Check brute force lockout
    {
        std::lock_guard<std::mutex> lock(attempts_mutex_);
        auto it = failed_attempts_.find(client_id);
        if (it != failed_attempts_.end()) {
            auto& entry = it->second;
            if (entry.count >= MAX_PAIR_ATTEMPTS && entry.locked_at.has_value()) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::steady_clock::now() - *entry.locked_at).count();
                if (static_cast<uint64_t>(elapsed) < PAIR_LOCKOUT_SECS) {
                    result.locked_out = true;
                    result.lockout_seconds = PAIR_LOCKOUT_SECS - static_cast<uint64_t>(elapsed);
                    return result;
                }
            }
        }
    }

    // Try to match pairing code
    {
        std::lock_guard<std::mutex> lock(code_mutex_);
        if (pairing_code_.has_value()) {
            std::string trimmed_code = code;
            size_t s = trimmed_code.find_first_not_of(" \t");
            size_t e = trimmed_code.find_last_not_of(" \t");
            if (s != std::string::npos) trimmed_code = trimmed_code.substr(s, e - s + 1);

            std::string expected = *pairing_code_;
            size_t es = expected.find_first_not_of(" \t");
            size_t ee = expected.find_last_not_of(" \t");
            if (es != std::string::npos) expected = expected.substr(es, ee - es + 1);

            if (constant_time_eq(trimmed_code, expected)) {
                // Success - reset failed attempts
                {
                    std::lock_guard<std::mutex> alock(attempts_mutex_);
                    failed_attempts_.erase(client_id);
                }

                auto token = generate_token();
                {
                    std::lock_guard<std::mutex> tlock(tokens_mutex_);
                    paired_tokens_.insert(hash_token(token));
                }

                // Consume pairing code
                pairing_code_ = std::nullopt;
                result.token = token;
                return result;
            }
        }
    }

    // Failed attempt
    {
        std::lock_guard<std::mutex> lock(attempts_mutex_);
        // Evict expired entries when approaching bound
        if (failed_attempts_.size() >= MAX_TRACKED_CLIENTS) {
            for (auto it = failed_attempts_.begin(); it != failed_attempts_.end(); ) {
                if (it->second.locked_at.has_value()) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::steady_clock::now() - *it->second.locked_at).count();
                    if (static_cast<uint64_t>(elapsed) >= PAIR_LOCKOUT_SECS) {
                        it = failed_attempts_.erase(it);
                        continue;
                    }
                }
                ++it;
            }
        }

        auto& entry = failed_attempts_[client_id];
        // Reset if previous lockout has expired
        if (entry.locked_at.has_value()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - *entry.locked_at).count();
            if (static_cast<uint64_t>(elapsed) >= PAIR_LOCKOUT_SECS) {
                entry.count = 0;
                entry.locked_at = std::nullopt;
            }
        }
        entry.count++;
        if (entry.count >= MAX_PAIR_ATTEMPTS) {
            entry.locked_at = std::chrono::steady_clock::now();
        }
    }

    return result;
}

bool PairingGuard::is_authenticated(const std::string& token) const {
    if (!require_pairing_) return true;
    auto hashed = hash_token(token);
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    return paired_tokens_.count(hashed) > 0;
}

bool PairingGuard::is_paired() const {
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    return !paired_tokens_.empty();
}

std::vector<std::string> PairingGuard::tokens() const {
    std::lock_guard<std::mutex> lock(tokens_mutex_);
    return std::vector<std::string>(paired_tokens_.begin(), paired_tokens_.end());
}

} // namespace security
} // namespace zeroclaw
