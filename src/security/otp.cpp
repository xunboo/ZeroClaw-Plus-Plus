#include "otp.hpp"
#include "secrets.hpp"
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <random>
#include <cstring>
#include <iomanip>
#include <sstream>

// For HMAC-SHA1 we use a minimal implementation.
// In a production environment, use OpenSSL or a crypto library.
// This is a self-contained HMAC-SHA1 implementation for portability.

namespace {

// ── Minimal SHA-1 implementation ─────────────────────────────────

struct SHA1Context {
    uint32_t state[5];
    uint64_t count;
    uint8_t buffer[64];
};

static uint32_t sha1_rotl(uint32_t value, int bits) {
    return (value << bits) | (value >> (32 - bits));
}

static void sha1_transform(uint32_t state[5], const uint8_t block[64]) {
    uint32_t w[80];
    for (int i = 0; i < 16; ++i) {
        w[i] = (uint32_t(block[i * 4]) << 24) | (uint32_t(block[i * 4 + 1]) << 16) |
               (uint32_t(block[i * 4 + 2]) << 8) | uint32_t(block[i * 4 + 3]);
    }
    for (int i = 16; i < 80; ++i) {
        w[i] = sha1_rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];
    for (int i = 0; i < 80; ++i) {
        uint32_t f, k;
        if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else { f = b ^ c ^ d; k = 0xCA62C1D6; }
        uint32_t temp = sha1_rotl(a, 5) + f + e + k + w[i];
        e = d; d = c; c = sha1_rotl(b, 30); b = a; a = temp;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e;
}

static void sha1_init(SHA1Context& ctx) {
    ctx.state[0] = 0x67452301; ctx.state[1] = 0xEFCDAB89;
    ctx.state[2] = 0x98BADCFE; ctx.state[3] = 0x10325476;
    ctx.state[4] = 0xC3D2E1F0;
    ctx.count = 0;
    std::memset(ctx.buffer, 0, 64);
}

static void sha1_update(SHA1Context& ctx, const uint8_t* data, size_t len) {
    size_t index = static_cast<size_t>(ctx.count & 63);
    ctx.count += len;
    size_t i = 0;
    if (index) {
        size_t part_len = 64 - index;
        if (len >= part_len) {
            std::memcpy(ctx.buffer + index, data, part_len);
            sha1_transform(ctx.state, ctx.buffer);
            i = part_len;
        } else {
            std::memcpy(ctx.buffer + index, data, len);
            return;
        }
    }
    for (; i + 64 <= len; i += 64) {
        sha1_transform(ctx.state, data + i);
    }
    if (i < len) {
        std::memcpy(ctx.buffer, data + i, len - i);
    }
}

static void sha1_final(SHA1Context& ctx, uint8_t digest[20]) {
    uint8_t padding[64];
    std::memset(padding, 0, 64);
    padding[0] = 0x80;
    uint64_t bits = ctx.count * 8;
    size_t index = static_cast<size_t>(ctx.count & 63);
    size_t pad_len = (index < 56) ? (56 - index) : (120 - index);
    sha1_update(ctx, padding, pad_len);
    uint8_t bits_be[8];
    for (int i = 0; i < 8; ++i) bits_be[i] = uint8_t(bits >> (56 - i * 8));
    sha1_update(ctx, bits_be, 8);
    for (int i = 0; i < 5; ++i) {
        digest[i * 4 + 0] = uint8_t(ctx.state[i] >> 24);
        digest[i * 4 + 1] = uint8_t(ctx.state[i] >> 16);
        digest[i * 4 + 2] = uint8_t(ctx.state[i] >> 8);
        digest[i * 4 + 3] = uint8_t(ctx.state[i]);
    }
}

// ── HMAC-SHA1 ────────────────────────────────────────────────────

static std::vector<uint8_t> hmac_sha1(const std::vector<uint8_t>& key,
                                       const std::vector<uint8_t>& data) {
    const size_t block_size = 64;
    std::vector<uint8_t> k(block_size, 0);

    if (key.size() > block_size) {
        SHA1Context ctx;
        sha1_init(ctx);
        sha1_update(ctx, key.data(), key.size());
        uint8_t digest[20];
        sha1_final(ctx, digest);
        std::memcpy(k.data(), digest, 20);
    } else {
        std::memcpy(k.data(), key.data(), key.size());
    }

    std::vector<uint8_t> ipad(block_size), opad(block_size);
    for (size_t i = 0; i < block_size; ++i) {
        ipad[i] = k[i] ^ 0x36;
        opad[i] = k[i] ^ 0x5c;
    }

    SHA1Context ctx;
    sha1_init(ctx);
    sha1_update(ctx, ipad.data(), ipad.size());
    sha1_update(ctx, data.data(), data.size());
    uint8_t inner_hash[20];
    sha1_final(ctx, inner_hash);

    sha1_init(ctx);
    sha1_update(ctx, opad.data(), opad.size());
    sha1_update(ctx, inner_hash, 20);
    uint8_t outer_hash[20];
    sha1_final(ctx, outer_hash);

    return std::vector<uint8_t>(outer_hash, outer_hash + 20);
}

} // anonymous namespace

namespace zeroclaw {
namespace security {

static const char* OTP_SECRET_FILE = "otp-secret";
static const uint32_t OTP_DIGITS = 6;
static const char* OTP_ISSUER = "ZeroClaw";

uint64_t unix_timestamp_now() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(duration).count());
}

std::filesystem::path otp_secret_file_path(const std::filesystem::path& zeroclaw_dir) {
    return zeroclaw_dir / OTP_SECRET_FILE;
}

std::string encode_base32_secret(const std::vector<uint8_t>& input) {
    static const char ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    if (input.empty()) return "";

    std::string result;
    uint16_t buffer = 0;
    uint8_t bits_left = 0;

    for (uint8_t byte : input) {
        buffer = (buffer << 8) | byte;
        bits_left += 8;
        while (bits_left >= 5) {
            uint8_t index = (buffer >> (bits_left - 5)) & 0x1f;
            result += ALPHABET[index];
            bits_left -= 5;
        }
    }

    if (bits_left > 0) {
        uint8_t index = (buffer << (5 - bits_left)) & 0x1f;
        result += ALPHABET[index];
    }

    return result;
}

std::vector<uint8_t> decode_base32_secret(const std::string& raw) {
    auto decode_char = [](char ch) -> int {
        if (ch >= 'A' && ch <= 'Z') return ch - 'A';
        if (ch >= '2' && ch <= '7') return ch - '2' + 26;
        return -1;
    };

    // Clean and uppercase
    std::string cleaned;
    for (char ch : raw) {
        if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r' && ch != '-') {
            cleaned += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        }
    }
    // Strip trailing padding
    while (!cleaned.empty() && cleaned.back() == '=') cleaned.pop_back();
    if (cleaned.empty()) throw std::runtime_error("OTP secret is empty");

    std::vector<uint8_t> output;
    uint32_t buffer = 0;
    uint8_t bits_left = 0;

    for (char ch : cleaned) {
        int value = decode_char(ch);
        if (value < 0) {
            throw std::runtime_error(std::string("OTP secret contains invalid base32 character '") + ch + "'");
        }
        buffer = (buffer << 5) | static_cast<uint32_t>(value);
        bits_left += 5;
        if (bits_left >= 8) {
            uint8_t byte = (buffer >> (bits_left - 8)) & 0xff;
            output.push_back(byte);
            bits_left -= 8;
        }
    }

    if (output.empty()) throw std::runtime_error("OTP secret did not decode to any bytes");
    return output;
}

std::string compute_totp_code(const std::vector<uint8_t>& secret, uint64_t counter) {
    // Counter to big-endian bytes
    std::vector<uint8_t> counter_bytes(8);
    for (int i = 7; i >= 0; --i) {
        counter_bytes[i] = static_cast<uint8_t>(counter & 0xff);
        counter >>= 8;
    }

    auto hash = hmac_sha1(secret, counter_bytes);

    uint8_t offset = hash[19] & 0x0f;
    uint32_t binary = ((uint32_t(hash[offset]) & 0x7f) << 24) |
                      (uint32_t(hash[offset + 1]) << 16) |
                      (uint32_t(hash[offset + 2]) << 8) |
                      uint32_t(hash[offset + 3]);

    uint32_t code = binary % 1000000; // 10^OTP_DIGITS
    std::ostringstream oss;
    oss << std::setw(OTP_DIGITS) << std::setfill('0') << code;
    return oss.str();
}

// ── OtpValidator ─────────────────────────────────────────────────

std::pair<OtpValidator, std::optional<std::string>>
OtpValidator::from_config(const OtpConfig& config,
                           const std::filesystem::path& zeroclaw_dir,
                           const SecretStore& store) {
    auto secret_path = otp_secret_file_path(zeroclaw_dir);
    OtpValidator validator;
    validator.config_ = config;
    std::optional<std::string> uri;

    if (std::filesystem::exists(secret_path)) {
        // Read existing secret
        std::ifstream file(secret_path);
        std::string encoded((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        // Trim
        size_t s = encoded.find_first_not_of(" \t\n\r");
        size_t e = encoded.find_last_not_of(" \t\n\r");
        if (s != std::string::npos) encoded = encoded.substr(s, e - s + 1);

        std::string decrypted = store.decrypt(encoded);
        validator.secret_ = decode_base32_secret(decrypted);
    } else {
        // Generate new secret
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint16_t> dist(0, 255);
        std::vector<uint8_t> raw(20);
        for (auto& b : raw) b = static_cast<uint8_t>(dist(gen));

        std::string encoded_secret = encode_base32_secret(raw);
        std::string encrypted = store.encrypt(encoded_secret);

        // Write to file
        if (auto parent = secret_path.parent_path(); !parent.empty()) {
            std::filesystem::create_directories(parent);
        }
        std::ofstream file(secret_path);
        file << encrypted;
        file.close();

        validator.secret_ = raw;
        uri = validator.otpauth_uri();
    }

    return {std::move(validator), uri};
}

bool OtpValidator::validate(const std::string& code) const {
    return validate_at(code, unix_timestamp_now());
}

bool OtpValidator::validate_at(const std::string& code, uint64_t now_secs) const {
    // Normalize
    std::string normalized = code;
    size_t s = normalized.find_first_not_of(" \t");
    size_t e = normalized.find_last_not_of(" \t");
    if (s == std::string::npos) return false;
    normalized = normalized.substr(s, e - s + 1);

    if (normalized.size() != OTP_DIGITS) return false;
    for (char ch : normalized) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) return false;
    }

    // Check cache
    {
        std::lock_guard<std::mutex> lock(*mutex_);
        // Clean expired cache entries
        for (auto it = cached_codes_.begin(); it != cached_codes_.end(); ) {
            if (it->second < now_secs) it = cached_codes_.erase(it);
            else ++it;
        }
        auto it = cached_codes_.find(normalized);
        if (it != cached_codes_.end() && it->second >= now_secs) {
            return true;
        }
    }

    uint64_t step = std::max(config_.token_ttl_secs, uint64_t(1));
    uint64_t counter = now_secs / step;

    uint64_t counters[] = {
        counter > 0 ? counter - 1 : 0,
        counter,
        counter + 1
    };

    bool is_valid = false;
    for (auto c : counters) {
        if (compute_totp_code(secret_, c) == normalized) {
            is_valid = true;
            break;
        }
    }

    if (is_valid) {
        std::lock_guard<std::mutex> lock(*mutex_);
        cached_codes_[normalized] = now_secs + config_.cache_valid_secs;
    }

    return is_valid;
}

std::string OtpValidator::otpauth_uri() const {
    std::string secret = encode_base32_secret(secret_);
    uint64_t period = std::max(config_.token_ttl_secs, uint64_t(1));
    return "otpauth://totp/" + std::string(OTP_ISSUER) + ":zeroclaw?secret=" + secret +
           "&issuer=" + OTP_ISSUER + "&period=" + std::to_string(period);
}

} // namespace security
} // namespace zeroclaw
