#include "oauth.hpp"
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace zeroclaw {
    namespace auth {

        namespace {
            std::string base64url_encode(const unsigned char* data, size_t len) {
                static const char* chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
                std::string result;
                result.reserve((len * 4 + 2) / 3);

                for (size_t i = 0; i < len; i += 3) {
                    uint32_t n = (static_cast<uint32_t>(data[i]) << 16);
                    if (i + 1 < len) n |= (static_cast<uint32_t>(data[i + 1]) << 8);
                    if (i + 2 < len) n |= static_cast<uint32_t>(data[i + 2]);

                    result.push_back(chars[(n >> 18) & 0x3F]);
                    result.push_back(chars[(n >> 12) & 0x3F]);
                    if (i + 1 < len) result.push_back(chars[(n >> 6) & 0x3F]);
                    if (i + 2 < len) result.push_back(chars[n & 0x3F]);
                }

                return result;
            }

            std::string base64url_encode_no_pad(const unsigned char* data, size_t len) {
                return base64url_encode(data, len);
            }
        }

        std::string random_base64url(size_t byte_len) {
            std::vector<unsigned char> bytes(byte_len);
            RAND_bytes(bytes.data(), static_cast<int>(byte_len));
            return base64url_encode_no_pad(bytes.data(), byte_len);
        }

        PkceState generate_pkce_state() {
            PkceState pkce;
            pkce.code_verifier = random_base64url(64);

            unsigned char hash[SHA256_DIGEST_LENGTH];
            SHA256(reinterpret_cast<const unsigned char*>(pkce.code_verifier.data()),
                   pkce.code_verifier.size(),
                   hash);

            pkce.code_challenge = base64url_encode_no_pad(hash, SHA256_DIGEST_LENGTH);
            pkce.state = random_base64url(24);

            return pkce;
        }

        std::string url_encode(const std::string& input) {
            std::ostringstream result;
            for (unsigned char c : input) {
                if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                    result << c;
                } else {
                    result << '%' << std::uppercase << std::setw(2) << std::setfill('0')
                           << std::hex << static_cast<int>(c);
                }
            }
            return result.str();
        }

        std::string url_decode(const std::string& input) {
            std::string result;
            result.reserve(input.size());

            for (size_t i = 0; i < input.size(); ++i) {
                if (input[i] == '%' && i + 2 < input.size()) {
                    std::string hex = input.substr(i + 1, 2);
                    try {
                        unsigned char c = static_cast<unsigned char>(std::stoul(hex, nullptr, 16));
                        result.push_back(c);
                        i += 2;
                    } catch (...) {
                        result.push_back(input[i]);
                    }
                } else if (input[i] == '+') {
                    result.push_back(' ');
                } else {
                    result.push_back(input[i]);
                }
            }

            return result;
        }

        std::map<std::string, std::string> parse_query_params(const std::string& input) {
            std::map<std::string, std::string> result;

            size_t start = 0;
            while (start < input.size()) {
                size_t amp_pos = input.find('&', start);
                std::string pair = (amp_pos == std::string::npos)
                    ? input.substr(start)
                    : input.substr(start, amp_pos - start);

                if (!pair.empty()) {
                    size_t eq_pos = pair.find('=');
                    if (eq_pos != std::string::npos) {
                        std::string key = url_decode(pair.substr(0, eq_pos));
                        std::string value = url_decode(pair.substr(eq_pos + 1));
                        result[key] = value;
                    } else {
                        result[url_decode(pair)] = "";
                    }
                }

                if (amp_pos == std::string::npos) break;
                start = amp_pos + 1;
            }

            return result;
        }

        namespace anthropic {

            std::string auth_kind_to_metadata_value(AnthropicAuthKind kind) {
                switch (kind) {
                    case AnthropicAuthKind::ApiKey: return "api-key";
                    case AnthropicAuthKind::Authorization: return "authorization";
                }
                return "api-key";
            }

            std::optional<AnthropicAuthKind> auth_kind_from_metadata_value(const std::string& value) {
                std::string lower = value;
                std::transform(lower.begin(), lower.end(), lower.begin(),
                              [](unsigned char c) { return std::tolower(c); });

                std::string trimmed = lower;
                size_t start = trimmed.find_first_not_of(" \t\r\n");
                size_t end = trimmed.find_last_not_of(" \t\r\n");
                if (start != std::string::npos) {
                    trimmed = trimmed.substr(start, end - start + 1);
                }

                if (trimmed == "api-key" || trimmed == "x-api-key" || trimmed == "apikey") {
                    return AnthropicAuthKind::ApiKey;
                }
                if (trimmed == "authorization" || trimmed == "bearer" || trimmed == "auth-token" || trimmed == "oauth") {
                    return AnthropicAuthKind::Authorization;
                }

                return std::nullopt;
            }

            AnthropicAuthKind detect_auth_kind(const std::string& token, const std::optional<std::string>& explicit_kind) {
                if (explicit_kind.has_value()) {
                    auto kind = auth_kind_from_metadata_value(explicit_kind.value());
                    if (kind.has_value()) {
                        return kind.value();
                    }
                }

                std::string trimmed = token;
                size_t start = trimmed.find_first_not_of(" \t\r\n");
                size_t end = trimmed.find_last_not_of(" \t\r\n");
                if (start != std::string::npos) {
                    trimmed = trimmed.substr(start, end - start + 1);
                }

                size_t dot_count = std::count(trimmed.begin(), trimmed.end(), '.');
                if (dot_count >= 2) {
                    return AnthropicAuthKind::Authorization;
                }

                if (trimmed.rfind("sk-ant-api", 0) == 0) {
                    return AnthropicAuthKind::ApiKey;
                }

                return AnthropicAuthKind::ApiKey;
            }

        }

    }
}
