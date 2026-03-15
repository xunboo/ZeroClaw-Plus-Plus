#include "secrets.hpp"
#include <fstream>
#include <random>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

namespace zeroclaw {
namespace security {

// ── Low-level helpers ────────────────────────────────────────────

std::vector<uint8_t> xor_cipher(const std::vector<uint8_t>& data, const std::vector<uint8_t>& key) {
    if (key.empty()) return data;
    std::vector<uint8_t> result(data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        result[i] = data[i] ^ key[i % key.size()];
    }
    return result;
}

std::string hex_encode(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    for (uint8_t b : data) {
        oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
    }
    return oss.str();
}

std::vector<uint8_t> hex_decode(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        throw std::runtime_error("Hex string has odd length");
    }
    std::vector<uint8_t> result;
    result.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        std::string byte_str = hex.substr(i, 2);
        char* end;
        unsigned long val = std::strtoul(byte_str.c_str(), &end, 16);
        if (*end != '\0') {
            throw std::runtime_error("Invalid hex at position " + std::to_string(i));
        }
        result.push_back(static_cast<uint8_t>(val));
    }
    return result;
}

std::vector<uint8_t> generate_random_key() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint16_t> dist(0, 255);
    std::vector<uint8_t> key(SECRET_KEY_LEN);
    for (auto& b : key) b = static_cast<uint8_t>(dist(gen));
    return key;
}

// ── SecretStore ──────────────────────────────────────────────────

SecretStore::SecretStore(const std::filesystem::path& zeroclaw_dir, bool enabled)
    : key_path_(zeroclaw_dir / ".secret_key"), enabled_(enabled) {}

std::string SecretStore::encrypt(const std::string& plaintext) const {
    if (!enabled_ || plaintext.empty()) return plaintext;

    auto key = load_or_create_key();

    // Generate random nonce
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint16_t> dist(0, 255);
    std::vector<uint8_t> nonce(SECRET_NONCE_LEN);
    for (auto& b : nonce) b = static_cast<uint8_t>(dist(gen));

    // XOR-based encryption (ChaCha20 placeholder)
    std::vector<uint8_t> plaintext_bytes(plaintext.begin(), plaintext.end());
    auto ciphertext = xor_cipher(plaintext_bytes, key);

    // Prepend nonce to ciphertext
    std::vector<uint8_t> blob;
    blob.reserve(nonce.size() + ciphertext.size());
    blob.insert(blob.end(), nonce.begin(), nonce.end());
    blob.insert(blob.end(), ciphertext.begin(), ciphertext.end());

    return "enc2:" + hex_encode(blob);
}

std::string SecretStore::decrypt(const std::string& value) const {
    if (value.substr(0, 5) == "enc2:") {
        return decrypt_enc2(value.substr(5));
    } else if (value.substr(0, 4) == "enc:") {
        return decrypt_legacy_xor(value.substr(4));
    }
    return value; // plaintext passthrough
}

std::pair<std::string, std::string> SecretStore::decrypt_and_migrate(const std::string& value) const {
    if (value.substr(0, 5) == "enc2:") {
        auto plaintext = decrypt_enc2(value.substr(5));
        return {plaintext, ""}; // No migration needed
    } else if (value.substr(0, 4) == "enc:") {
        auto plaintext = decrypt_legacy_xor(value.substr(4));
        auto migrated = encrypt(plaintext);
        return {plaintext, migrated};
    }
    return {value, ""}; // Plaintext, no migration
}

bool SecretStore::needs_migration(const std::string& value) {
    return value.substr(0, 4) == "enc:";
}

bool SecretStore::is_encrypted(const std::string& value) {
    return value.substr(0, 5) == "enc2:" || value.substr(0, 4) == "enc:";
}

bool SecretStore::is_secure_encrypted(const std::string& value) {
    return value.substr(0, 5) == "enc2:";
}

std::string SecretStore::decrypt_enc2(const std::string& hex_str) const {
    auto blob = hex_decode(hex_str);
    if (blob.size() <= SECRET_NONCE_LEN) {
        throw std::runtime_error("Encrypted value too short (missing nonce)");
    }

    // Skip nonce, decrypt ciphertext
    std::vector<uint8_t> ciphertext(blob.begin() + SECRET_NONCE_LEN, blob.end());
    auto key = load_or_create_key();
    auto plaintext_bytes = xor_cipher(ciphertext, key);

    return std::string(plaintext_bytes.begin(), plaintext_bytes.end());
}

std::string SecretStore::decrypt_legacy_xor(const std::string& hex_str) const {
    auto ciphertext = hex_decode(hex_str);
    auto key = load_or_create_key();
    auto plaintext_bytes = xor_cipher(ciphertext, key);
    return std::string(plaintext_bytes.begin(), plaintext_bytes.end());
}

std::vector<uint8_t> SecretStore::load_or_create_key() const {
    if (std::filesystem::exists(key_path_)) {
        std::ifstream file(key_path_);
        std::string hex_key((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        // Trim
        size_t s = hex_key.find_first_not_of(" \t\n\r");
        size_t e = hex_key.find_last_not_of(" \t\n\r");
        if (s != std::string::npos) hex_key = hex_key.substr(s, e - s + 1);
        return hex_decode(hex_key);
    }

    // Create new key
    auto key = generate_random_key();
    if (auto parent = key_path_.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    std::ofstream file(key_path_);
    file << hex_encode(key);
    file.close();

    // Set restrictive permissions
#ifdef _WIN32
    // On Windows, use icacls to restrict permissions to current user only
    const char* username = std::getenv("USERNAME");
    if (username && std::strlen(username) > 0) {
        std::string trimmed(username);
        size_t s = trimmed.find_first_not_of(" \t");
        size_t e = trimmed.find_last_not_of(" \t");
        if (s != std::string::npos) {
            trimmed = trimmed.substr(s, e - s + 1);
            std::string grant_arg = trimmed + ":F";
            std::string cmd = "icacls \"" + key_path_.string() + "\" /inheritance:r /grant:r \"" + grant_arg + "\"";
            std::system(cmd.c_str());
        }
    }
#else
    std::filesystem::permissions(key_path_,
        std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace);
#endif

    return key;
}

} // namespace security
} // namespace zeroclaw
