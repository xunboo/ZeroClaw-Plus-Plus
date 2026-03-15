#pragma once

#include "profiles.hpp"
#include "oauth.hpp"
#include <string>
#include <optional>
#include <map>
#include <memory>
#include <functional>
#include <chrono>

namespace zeroclaw {
    namespace auth {

        constexpr const char* OPENAI_OAUTH_CLIENT_ID = "app_EMoamEEZ73f0CkXaXp7hrann";
        constexpr const char* OPENAI_OAUTH_AUTHORIZE_URL = "https://auth.openai.com/oauth/authorize";
        constexpr const char* OPENAI_OAUTH_TOKEN_URL = "https://auth.openai.com/oauth/token";
        constexpr const char* OPENAI_OAUTH_DEVICE_CODE_URL = "https://auth.openai.com/oauth/device/code";
        constexpr const char* OPENAI_OAUTH_REDIRECT_URI = "http://localhost:1455/auth/callback";

        constexpr const char* GOOGLE_OAUTH_AUTHORIZE_URL = "https://accounts.google.com/o/oauth2/v2/auth";
        constexpr const char* GOOGLE_OAUTH_TOKEN_URL = "https://oauth2.googleapis.com/token";
        constexpr const char* GOOGLE_OAUTH_DEVICE_CODE_URL = "https://oauth2.googleapis.com/device/code";
        constexpr const char* GEMINI_OAUTH_REDIRECT_URI = "http://localhost:1456/auth/callback";
        constexpr const char* GEMINI_OAUTH_SCOPES = "openid profile email https://www.googleapis.com/auth/cloud-platform";

        struct DeviceCodeStart {
            std::string device_code;
            std::string user_code;
            std::string verification_uri;
            std::optional<std::string> verification_uri_complete;
            uint64_t expires_in;
            uint64_t interval;
            std::optional<std::string> message;
        };

        class HttpClient {
        public:
            virtual ~HttpClient() = default;
            virtual std::string post_form(const std::string& url, const std::map<std::string, std::string>& form_data) = 0;
            virtual std::pair<int, std::string> post_form_with_status(const std::string& url, const std::map<std::string, std::string>& form_data) = 0;
        };

        std::unique_ptr<HttpClient> create_http_client();

        namespace openai {
            std::string build_authorize_url(const PkceState& pkce);
            TokenSet exchange_code_for_tokens(HttpClient& client, const std::string& code, const PkceState& pkce);
            TokenSet refresh_access_token(HttpClient& client, const std::string& refresh_token);
            DeviceCodeStart start_device_code_flow(HttpClient& client);
            TokenSet poll_device_code_tokens(HttpClient& client, const DeviceCodeStart& device);
            std::string receive_loopback_code(const std::string& expected_state, std::chrono::seconds timeout);
            std::string parse_code_from_redirect(const std::string& input, const std::optional<std::string>& expected_state);
            std::optional<std::string> extract_account_id_from_jwt(const std::string& token);
        }

        namespace gemini {
            std::optional<std::string> get_oauth_client_id();
            std::optional<std::string> get_oauth_client_secret();
            std::string build_authorize_url(const PkceState& pkce);
            TokenSet exchange_code_for_tokens(HttpClient& client, const std::string& code, const PkceState& pkce);
            TokenSet refresh_access_token(HttpClient& client, const std::string& refresh_token);
            DeviceCodeStart start_device_code_flow(HttpClient& client);
            TokenSet poll_device_code_tokens(HttpClient& client, const DeviceCodeStart& device);
            std::string receive_loopback_code(const std::string& expected_state, std::chrono::seconds timeout);
            std::string parse_code_from_redirect(const std::string& input, const std::optional<std::string>& expected_state);
            std::optional<std::string> extract_account_email_from_id_token(const std::string& id_token);
        }

    }
}
