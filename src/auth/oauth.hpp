#pragma once

#include <string>
#include <optional>
#include <map>
#include <vector>

namespace zeroclaw {
    namespace auth {

        struct PkceState {
            std::string code_verifier;
            std::string code_challenge;
            std::string state;
        };

        PkceState generate_pkce_state();
        std::string random_base64url(size_t byte_len);
        std::string url_encode(const std::string& input);
        std::string url_decode(const std::string& input);
        std::map<std::string, std::string> parse_query_params(const std::string& input);

        namespace anthropic {
            enum class AnthropicAuthKind {
                ApiKey,
                Authorization
            };

            std::string auth_kind_to_metadata_value(AnthropicAuthKind kind);
            std::optional<AnthropicAuthKind> auth_kind_from_metadata_value(const std::string& value);
            AnthropicAuthKind detect_auth_kind(const std::string& token, const std::optional<std::string>& explicit_kind);
        }

    }
}
