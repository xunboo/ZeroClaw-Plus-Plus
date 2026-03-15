#pragma once

/// OTP (Time-based One-Time Password) validator for security authentication.
/// Uses HMAC-SHA1 for TOTP code generation per RFC 6238.

#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include <optional>
#include <filesystem>

namespace zeroclaw {
namespace security {

// Forward declaration
class SecretStore;

/// OTP configuration (mirrors Rust OtpConfig)
struct OtpConfig {
    bool enabled = false;
    uint64_t token_ttl_secs = 30;
    uint64_t cache_valid_secs = 120;
};

/// OTP Validator — generates and validates TOTP codes
class OtpValidator {
public:
    /// Create from config; generates a new secret if none exists.
    /// Returns the validator and optionally the otpauth URI (if secret was just generated).
    static std::pair<OtpValidator, std::optional<std::string>>
    from_config(const OtpConfig& config,
                const std::filesystem::path& zeroclaw_dir,
                const SecretStore& store);

    OtpValidator(OtpValidator&&) = default;
    OtpValidator& operator=(OtpValidator&&) = default;
    OtpValidator(const OtpValidator&) = delete;
    OtpValidator& operator=(const OtpValidator&) = delete;

    /// Validate a TOTP code against current time
    bool validate(const std::string& code) const;

    /// Generate the otpauth:// URI for QR code enrollment
    std::string otpauth_uri() const;

private:
    OtpValidator() : mutex_(std::make_unique<std::mutex>()) {}

    bool validate_at(const std::string& code, uint64_t now_secs) const;

    OtpConfig config_;
    std::vector<uint8_t> secret_;
    mutable std::unique_ptr<std::mutex> mutex_;
    mutable std::unordered_map<std::string, uint64_t> cached_codes_;
};

/// Get the path to the OTP secret file
std::filesystem::path otp_secret_file_path(const std::filesystem::path& zeroclaw_dir);

/// Base32 encoding/decoding for TOTP secrets
std::string encode_base32_secret(const std::vector<uint8_t>& input);
std::vector<uint8_t> decode_base32_secret(const std::string& raw);

/// Compute a TOTP code for the given secret and counter
std::string compute_totp_code(const std::vector<uint8_t>& secret, uint64_t counter);

/// Get current Unix timestamp in seconds
uint64_t unix_timestamp_now();

} // namespace security
} // namespace zeroclaw
