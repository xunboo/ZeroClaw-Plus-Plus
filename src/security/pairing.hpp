#pragma once

/// Gateway pairing mode — first-connect authentication.
///
/// On startup the gateway generates a one-time pairing code printed to the
/// terminal. The first client must present this code. The server responds with
/// a bearer token that must be sent on all subsequent requests.

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <optional>
#include <chrono>
#include <cstdint>

namespace zeroclaw {
namespace security {

/// Maximum failed pairing attempts before lockout
static constexpr uint32_t MAX_PAIR_ATTEMPTS = 5;
/// Lockout duration after too many failed pairing attempts (5 minutes)
static constexpr uint64_t PAIR_LOCKOUT_SECS = 300;
/// Maximum tracked client entries to bound memory usage
static constexpr size_t MAX_TRACKED_CLIENTS = 1024;

/// Manages pairing state for the gateway.
/// Bearer tokens are stored as SHA-256 hashes.
class PairingGuard {
public:
    /// Create a new pairing guard.
    PairingGuard(bool require_pairing, const std::vector<std::string>& existing_tokens);

    /// The one-time pairing code (only set when no tokens exist yet)
    std::optional<std::string> pairing_code() const;

    /// Whether pairing is required at all
    bool require_pairing() const { return require_pairing_; }

    /// Attempt to pair with the given code. Returns a bearer token on success.
    /// Returns empty string on failure.
    /// Sets lockout_seconds > 0 if locked out due to brute force.
    struct PairResult {
        std::optional<std::string> token;
        uint64_t lockout_seconds = 0;
        bool locked_out = false;
    };
    PairResult try_pair(const std::string& code, const std::string& client_id);

    /// Check if a bearer token is valid (compares against stored hashes)
    bool is_authenticated(const std::string& token) const;

    /// Returns true if the gateway is already paired (has at least one token)
    bool is_paired() const;

    /// Get all paired token hashes (for persisting to config)
    std::vector<std::string> tokens() const;

private:
    bool require_pairing_;
    mutable std::mutex code_mutex_;
    std::optional<std::string> pairing_code_;
    mutable std::mutex tokens_mutex_;
    std::unordered_set<std::string> paired_tokens_;

    struct FailedEntry {
        uint32_t count = 0;
        std::optional<std::chrono::steady_clock::time_point> locked_at;
    };
    mutable std::mutex attempts_mutex_;
    std::unordered_map<std::string, FailedEntry> failed_attempts_;
};

// ── Utility functions ────────────────────────────────────────────

/// SHA-256 hash a bearer token for storage (lowercase hex)
std::string hash_token(const std::string& token);

/// Check if a stored value looks like a SHA-256 hash
bool is_token_hash(const std::string& value);

/// Constant-time string comparison to prevent timing attacks
bool constant_time_eq(const std::string& a, const std::string& b);

/// Check if a host string represents a non-localhost bind address
bool is_public_bind(const std::string& host);

/// Generate a cryptographic 6-digit pairing code
std::string generate_pairing_code();

/// Generate a cryptographic bearer token with 256-bit entropy
std::string generate_token();

} // namespace security
} // namespace zeroclaw
