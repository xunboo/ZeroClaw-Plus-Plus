#pragma once

/// Encrypted secret store — defense-in-depth for API keys and tokens.
///
/// Secrets are encrypted using XOR cipher with a random key stored
/// in the config directory. In a production build, this should use
/// ChaCha20-Poly1305 AEAD (the Rust version uses the chacha20poly1305 crate).
///
/// This C++ implementation provides:
///   - enc2: prefix for ChaCha20 format (placeholder, falls back to XOR for now)
///   - enc: prefix for legacy XOR cipher
///   - Plaintext passthrough for unencrypted values
///   - Migration detection from enc: to enc2: format

#include <string>
#include <vector>
#include <filesystem>

namespace zeroclaw {
namespace security {

/// Key length in bytes (256-bit)
static constexpr size_t SECRET_KEY_LEN = 32;

/// ChaCha20-Poly1305 nonce length
static constexpr size_t SECRET_NONCE_LEN = 12;

/// Manages encrypted storage of secrets (API keys, tokens, etc.)
class SecretStore {
public:
    /// Create a new secret store rooted at the given directory.
    SecretStore(const std::filesystem::path& zeroclaw_dir, bool enabled);

    /// Encrypt a plaintext secret. Returns prefixed ciphertext.
    /// If encryption is disabled, returns the plaintext as-is.
    std::string encrypt(const std::string& plaintext) const;

    /// Decrypt a secret.
    /// - enc2: prefix → ChaCha20 format (currently XOR-based placeholder)
    /// - enc: prefix → legacy XOR cipher
    /// - No prefix → returned as-is (plaintext config)
    std::string decrypt(const std::string& value) const;

    /// Decrypt and return migrated enc2: value if input used legacy enc: format.
    /// Returns (plaintext, migrated_value) where migrated_value is set if migration occurred.
    std::pair<std::string, std::string> decrypt_and_migrate(const std::string& value) const;

    /// Check if a value uses the legacy enc: format that should be migrated.
    static bool needs_migration(const std::string& value);

    /// Check if a value is already encrypted (current or legacy format).
    static bool is_encrypted(const std::string& value);

    /// Check if a value uses the secure enc2: format.
    static bool is_secure_encrypted(const std::string& value);

private:
    std::string decrypt_enc2(const std::string& hex_str) const;
    std::string decrypt_legacy_xor(const std::string& hex_str) const;
    std::vector<uint8_t> load_or_create_key() const;

    std::filesystem::path key_path_;
    bool enabled_;
};

// ── Low-level helpers ────────────────────────────────────────────

/// XOR cipher with repeating key (same function for encrypt and decrypt)
std::vector<uint8_t> xor_cipher(const std::vector<uint8_t>& data, const std::vector<uint8_t>& key);

/// Hex-encode bytes to a lowercase hex string
std::string hex_encode(const std::vector<uint8_t>& data);

/// Hex-decode a hex string to bytes (throws on invalid input)
std::vector<uint8_t> hex_decode(const std::string& hex);

/// Generate a random 256-bit key
std::vector<uint8_t> generate_random_key();

} // namespace security
} // namespace zeroclaw
