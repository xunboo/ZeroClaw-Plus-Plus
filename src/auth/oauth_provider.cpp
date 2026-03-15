#include "oauth_provider.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cctype>

namespace zeroclaw {
    namespace auth {

        namespace {

            std::optional<std::string> get_env_var(const std::string& name) {
                const char* value = std::getenv(name.c_str());
                if (value && strlen(value) > 0) {
                    return std::string(value);
                }
                return std::nullopt;
            }

            std::string base64url_decode(const std::string& input) {
                std::string result;
                std::string padded = input;

                while (padded.size() % 4 != 0) {
                    padded += '=';
                }

                for (char& c : padded) {
                    if (c == '-') c = '+';
                    else if (c == '_') c = '/';
                }

                static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                std::vector<int> T(256, -1);
                for (int i = 0; i < 64; i++) T[chars[i]] = i;

                int val = 0, valb = -8;
                for (unsigned char c : padded) {
                    if (T[c] == -1) break;
                    val = (val << 6) + T[c];
                    valb += 6;
                    if (valb >= 0) {
                        result.push_back(static_cast<char>((val >> valb) & 0xFF));
                        valb -= 8;
                    }
                }

                return result;
            }

            TokenSet parse_token_response(const nlohmann::json& j) {
                TokenSet ts;
                ts.access_token = j.value("access_token", "");

                if (j.contains("refresh_token") && !j["refresh_token"].is_null()) {
                    ts.refresh_token = j["refresh_token"].get<std::string>();
                }
                if (j.contains("id_token") && !j["id_token"].is_null()) {
                    ts.id_token = j["id_token"].get<std::string>();
                }
                if (j.contains("expires_in") && !j["expires_in"].is_null()) {
                    auto secs = j["expires_in"].get<int64_t>();
                    if (secs > 0) {
                        ts.expires_at = std::chrono::system_clock::now() + std::chrono::seconds(secs);
                    }
                }
                if (j.contains("token_type") && !j["token_type"].is_null()) {
                    ts.token_type = j["token_type"].get<std::string>();
                } else {
                    ts.token_type = "Bearer";
                }
                if (j.contains("scope") && !j["scope"].is_null()) {
                    ts.scope = j["scope"].get<std::string>();
                }

                return ts;
            }

            std::string build_form_body(const std::map<std::string, std::string>& form_data) {
                std::ostringstream oss;
                bool first = true;
                for (const auto& [k, v] : form_data) {
                    if (!first) oss << "&";
                    oss << url_encode(k) << "=" << url_encode(v);
                    first = false;
                }
                return oss.str();
            }
        }

        namespace openai {

            std::string build_authorize_url(const PkceState& pkce) {
                std::map<std::string, std::string> params = {
                    {"response_type", "code"},
                    {"client_id", OPENAI_OAUTH_CLIENT_ID},
                    {"redirect_uri", OPENAI_OAUTH_REDIRECT_URI},
                    {"scope", "openid profile email offline_access"},
                    {"code_challenge", pkce.code_challenge},
                    {"code_challenge_method", "S256"},
                    {"state", pkce.state},
                    {"codex_cli_simplified_flow", "true"},
                    {"id_token_add_organizations", "true"}
                };

                std::ostringstream oss;
                oss << OPENAI_OAUTH_AUTHORIZE_URL << "?";
                bool first = true;
                for (const auto& [k, v] : params) {
                    if (!first) oss << "&";
                    oss << url_encode(k) << "=" << url_encode(v);
                    first = false;
                }
                return oss.str();
            }

            TokenSet exchange_code_for_tokens(HttpClient& client, const std::string& code, const PkceState& pkce) {
                std::map<std::string, std::string> form = {
                    {"grant_type", "authorization_code"},
                    {"code", code},
                    {"client_id", OPENAI_OAUTH_CLIENT_ID},
                    {"redirect_uri", OPENAI_OAUTH_REDIRECT_URI},
                    {"code_verifier", pkce.code_verifier}
                };

                auto [status, body] = client.post_form_with_status(OPENAI_OAUTH_TOKEN_URL, form);
                if (status < 200 || status >= 300) {
                    throw std::runtime_error("OpenAI OAuth token request failed (" + std::to_string(status) + "): " + body);
                }

                auto j = nlohmann::json::parse(body);
                return parse_token_response(j);
            }

            TokenSet refresh_access_token(HttpClient& client, const std::string& refresh_token) {
                std::map<std::string, std::string> form = {
                    {"grant_type", "refresh_token"},
                    {"refresh_token", refresh_token},
                    {"client_id", OPENAI_OAUTH_CLIENT_ID}
                };

                auto [status, body] = client.post_form_with_status(OPENAI_OAUTH_TOKEN_URL, form);
                if (status < 200 || status >= 300) {
                    throw std::runtime_error("OpenAI OAuth refresh failed (" + std::to_string(status) + "): " + body);
                }

                auto j = nlohmann::json::parse(body);
                return parse_token_response(j);
            }

            DeviceCodeStart start_device_code_flow(HttpClient& client) {
                std::map<std::string, std::string> form = {
                    {"client_id", OPENAI_OAUTH_CLIENT_ID},
                    {"scope", "openid profile email offline_access"}
                };

                auto [status, body] = client.post_form_with_status(OPENAI_OAUTH_DEVICE_CODE_URL, form);
                if (status < 200 || status >= 300) {
                    throw std::runtime_error("OpenAI device-code start failed (" + std::to_string(status) + "): " + body);
                }

                auto j = nlohmann::json::parse(body);
                DeviceCodeStart device;
                device.device_code = j.value("device_code", "");
                device.user_code = j.value("user_code", "");
                device.verification_uri = j.value("verification_uri", "");
                if (j.contains("verification_uri_complete") && !j["verification_uri_complete"].is_null()) {
                    device.verification_uri_complete = j["verification_uri_complete"].get<std::string>();
                }
                device.expires_in = j.value("expires_in", uint64_t(1800));
                device.interval = j.value("interval", uint64_t(5));
                if (device.interval < 1) device.interval = 1;
                if (j.contains("message") && !j["message"].is_null()) {
                    device.message = j["message"].get<std::string>();
                }

                return device;
            }

            TokenSet poll_device_code_tokens(HttpClient& client, const DeviceCodeStart& device) {
                auto start = std::chrono::steady_clock::now();
                uint64_t interval_secs = device.interval;
                if (interval_secs < 1) interval_secs = 1;

                while (true) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start);
                    if (elapsed.count() > static_cast<int64_t>(device.expires_in)) {
                        throw std::runtime_error("Device-code flow timed out before authorization completed");
                    }

                    std::this_thread::sleep_for(std::chrono::seconds(interval_secs));

                    std::map<std::string, std::string> form = {
                        {"grant_type", "urn:ietf:params:oauth:grant-type:device_code"},
                        {"device_code", device.device_code},
                        {"client_id", OPENAI_OAUTH_CLIENT_ID}
                    };

                    auto [status, body] = client.post_form_with_status(OPENAI_OAUTH_TOKEN_URL, form);
                    if (status >= 200 && status < 300) {
                        auto j = nlohmann::json::parse(body);
                        return parse_token_response(j);
                    }

                    try {
                        auto j = nlohmann::json::parse(body);
                        std::string error = j.value("error", "");

                        if (error == "authorization_pending") {
                            continue;
                        } else if (error == "slow_down") {
                            interval_secs += 5;
                            continue;
                        } else if (error == "access_denied") {
                            throw std::runtime_error("OpenAI device-code authorization was denied");
                        } else if (error == "expired_token") {
                            throw std::runtime_error("OpenAI device-code expired");
                        } else {
                            std::string desc = j.value("error_description", error);
                            throw std::runtime_error("OpenAI device-code polling failed: " + desc);
                        }
                    } catch (const nlohmann::json::exception&) {
                        throw std::runtime_error("OpenAI device-code polling failed (" + std::to_string(status) + "): " + body);
                    }
                }
            }

            std::string receive_loopback_code(const std::string& expected_state, std::chrono::seconds timeout) {
                throw std::runtime_error("receive_loopback_code not implemented - requires TCP listener");
            }

            std::string parse_code_from_redirect(const std::string& input, const std::optional<std::string>& expected_state) {
                std::string trimmed = input;
                size_t start = trimmed.find_first_not_of(" \t\r\n");
                size_t end = trimmed.find_last_not_of(" \t\r\n");
                if (start != std::string::npos) {
                    trimmed = trimmed.substr(start, end - start + 1);
                }

                if (trimmed.empty()) {
                    throw std::runtime_error("No OAuth code provided");
                }

                std::string query = trimmed;
                size_t qpos = trimmed.find('?');
                if (qpos != std::string::npos) {
                    query = trimmed.substr(qpos + 1);
                }

                auto params = parse_query_params(query);
                bool is_callback_payload = trimmed.find('?') != std::string::npos ||
                    params.count("code") || params.count("state") || params.count("error");

                if (params.count("error")) {
                    std::string err = params["error"];
                    std::string desc = params.count("error_description") ? params["error_description"] : "OAuth authorization failed";
                    throw std::runtime_error("OpenAI OAuth error: " + err + " (" + desc + ")");
                }

                if (expected_state.has_value()) {
                    if (params.count("state")) {
                        if (params["state"] != expected_state.value()) {
                            throw std::runtime_error("OAuth state mismatch");
                        }
                    } else if (is_callback_payload) {
                        throw std::runtime_error("Missing OAuth state in callback");
                    }
                }

                if (params.count("code")) {
                    return params["code"];
                }

                if (!is_callback_payload) {
                    return trimmed;
                }

                throw std::runtime_error("Missing OAuth code in callback");
            }

            std::optional<std::string> extract_account_id_from_jwt(const std::string& token) {
                size_t first_dot = token.find('.');
                if (first_dot == std::string::npos) return std::nullopt;
                size_t second_dot = token.find('.', first_dot + 1);
                if (second_dot == std::string::npos) return std::nullopt;

                std::string payload_b64 = token.substr(first_dot + 1, second_dot - first_dot - 1);
                std::string payload = base64url_decode(payload_b64);

                try {
                    auto j = nlohmann::json::parse(payload);
                    std::vector<std::string> keys = {"account_id", "accountId", "acct", "sub", "https://api.openai.com/account_id"};
                    for (const auto& key : keys) {
                        if (j.contains(key) && j[key].is_string()) {
                            std::string value = j[key].get<std::string>();
                            if (!value.empty()) {
                                size_t s = value.find_first_not_of(" \t\r\n");
                                size_t e = value.find_last_not_of(" \t\r\n");
                                if (s != std::string::npos && e >= s) {
                                    return value.substr(s, e - s + 1);
                                }
                            }
                        }
                    }
                } catch (const nlohmann::json::exception&) {
                }

                return std::nullopt;
            }
        }

        namespace gemini {

            std::optional<std::string> get_oauth_client_id() {
                return get_env_var("GEMINI_OAUTH_CLIENT_ID");
            }

            std::optional<std::string> get_oauth_client_secret() {
                return get_env_var("GEMINI_OAUTH_CLIENT_SECRET");
            }

            std::string build_authorize_url(const PkceState& pkce) {
                auto client_id = get_oauth_client_id();
                if (!client_id.has_value()) {
                    throw std::runtime_error("GEMINI_OAUTH_CLIENT_ID environment variable is required");
                }

                std::map<std::string, std::string> params = {
                    {"response_type", "code"},
                    {"client_id", client_id.value()},
                    {"redirect_uri", GEMINI_OAUTH_REDIRECT_URI},
                    {"scope", GEMINI_OAUTH_SCOPES},
                    {"code_challenge", pkce.code_challenge},
                    {"code_challenge_method", "S256"},
                    {"state", pkce.state},
                    {"access_type", "offline"},
                    {"prompt", "consent"}
                };

                std::ostringstream oss;
                oss << GOOGLE_OAUTH_AUTHORIZE_URL << "?";
                bool first = true;
                for (const auto& [k, v] : params) {
                    if (!first) oss << "&";
                    oss << url_encode(k) << "=" << url_encode(v);
                    first = false;
                }
                return oss.str();
            }

            TokenSet exchange_code_for_tokens(HttpClient& client, const std::string& code, const PkceState& pkce) {
                auto client_id = get_oauth_client_id();
                auto client_secret = get_oauth_client_secret();
                if (!client_id.has_value() || !client_secret.has_value()) {
                    throw std::runtime_error("GEMINI_OAUTH_CLIENT_ID and GEMINI_OAUTH_CLIENT_SECRET environment variables are required");
                }

                std::map<std::string, std::string> form = {
                    {"grant_type", "authorization_code"},
                    {"code", code},
                    {"redirect_uri", GEMINI_OAUTH_REDIRECT_URI},
                    {"client_id", client_id.value()},
                    {"client_secret", client_secret.value()},
                    {"code_verifier", pkce.code_verifier}
                };

                auto [status, body] = client.post_form_with_status(GOOGLE_OAUTH_TOKEN_URL, form);
                if (status < 200 || status >= 300) {
                    try {
                        auto j = nlohmann::json::parse(body);
                        std::string err = j.value("error", "");
                        std::string desc = j.value("error_description", "");
                        throw std::runtime_error("Google OAuth error: " + err + " - " + desc);
                    } catch (const nlohmann::json::exception&) {
                        throw std::runtime_error("Google OAuth token exchange failed (" + std::to_string(status) + "): " + body);
                    }
                }

                auto j = nlohmann::json::parse(body);
                return parse_token_response(j);
            }

            TokenSet refresh_access_token(HttpClient& client, const std::string& refresh_token) {
                auto client_id = get_oauth_client_id();
                auto client_secret = get_oauth_client_secret();
                if (!client_id.has_value() || !client_secret.has_value()) {
                    throw std::runtime_error("GEMINI_OAUTH_CLIENT_ID and GEMINI_OAUTH_CLIENT_SECRET environment variables are required");
                }

                std::map<std::string, std::string> form = {
                    {"grant_type", "refresh_token"},
                    {"refresh_token", refresh_token},
                    {"client_id", client_id.value()},
                    {"client_secret", client_secret.value()}
                };

                auto [status, body] = client.post_form_with_status(GOOGLE_OAUTH_TOKEN_URL, form);
                if (status < 200 || status >= 300) {
                    try {
                        auto j = nlohmann::json::parse(body);
                        std::string err = j.value("error", "");
                        std::string desc = j.value("error_description", "");
                        throw std::runtime_error("Google OAuth refresh error: " + err + " - " + desc);
                    } catch (const nlohmann::json::exception&) {
                        throw std::runtime_error("Google OAuth refresh failed (" + std::to_string(status) + "): " + body);
                    }
                }

                auto j = nlohmann::json::parse(body);
                return parse_token_response(j);
            }

            DeviceCodeStart start_device_code_flow(HttpClient& client) {
                auto client_id = get_oauth_client_id();
                if (!client_id.has_value()) {
                    throw std::runtime_error("GEMINI_OAUTH_CLIENT_ID environment variable is required");
                }

                std::map<std::string, std::string> form = {
                    {"client_id", client_id.value()},
                    {"scope", GEMINI_OAUTH_SCOPES}
                };

                auto [status, body] = client.post_form_with_status(GOOGLE_OAUTH_DEVICE_CODE_URL, form);
                if (status < 200 || status >= 300) {
                    try {
                        auto j = nlohmann::json::parse(body);
                        std::string err = j.value("error", "");
                        std::string desc = j.value("error_description", "");
                        throw std::runtime_error("Google device code error: " + err + " - " + desc);
                    } catch (const nlohmann::json::exception&) {
                        throw std::runtime_error("Google device code request failed (" + std::to_string(status) + "): " + body);
                    }
                }

                auto j = nlohmann::json::parse(body);
                DeviceCodeStart device;
                device.device_code = j.value("device_code", "");
                device.user_code = j.value("user_code", "");
                device.verification_uri = j.value("verification_url", "");
                device.verification_uri_complete = device.verification_uri + "?user_code=" + device.user_code;
                device.expires_in = j.value("expires_in", uint64_t(1800));
                device.interval = j.value("interval", uint64_t(5));
                if (device.interval < 5) device.interval = 5;

                return device;
            }

            TokenSet poll_device_code_tokens(HttpClient& client, const DeviceCodeStart& device) {
                auto client_id = get_oauth_client_id();
                auto client_secret = get_oauth_client_secret();
                if (!client_id.has_value() || !client_secret.has_value()) {
                    throw std::runtime_error("GEMINI_OAUTH_CLIENT_ID and GEMINI_OAUTH_CLIENT_SECRET environment variables are required");
                }

                auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(device.expires_in);
                uint64_t interval_secs = device.interval;
                if (interval_secs < 5) interval_secs = 5;

                while (true) {
                    if (std::chrono::steady_clock::now() > deadline) {
                        throw std::runtime_error("Device code expired before authorization was completed");
                    }

                    std::this_thread::sleep_for(std::chrono::seconds(interval_secs));

                    std::map<std::string, std::string> form = {
                        {"client_id", client_id.value()},
                        {"client_secret", client_secret.value()},
                        {"device_code", device.device_code},
                        {"grant_type", "urn:ietf:params:oauth:grant-type:device_code"}
                    };

                    auto [status, body] = client.post_form_with_status(GOOGLE_OAUTH_TOKEN_URL, form);

                    if (status >= 200 && status < 300) {
                        auto j = nlohmann::json::parse(body);
                        return parse_token_response(j);
                    }

                    try {
                        auto j = nlohmann::json::parse(body);
                        std::string error = j.value("error", "");

                        if (error == "authorization_pending") {
                            continue;
                        } else if (error == "slow_down") {
                            std::this_thread::sleep_for(std::chrono::seconds(5));
                            continue;
                        } else if (error == "access_denied") {
                            throw std::runtime_error("User denied authorization");
                        } else if (error == "expired_token") {
                            throw std::runtime_error("Device code expired");
                        } else {
                            std::string desc = j.value("error_description", "");
                            throw std::runtime_error("Google OAuth error: " + error + " - " + desc);
                        }
                    } catch (const nlohmann::json::exception&) {
                    }
                }
            }

            std::string receive_loopback_code(const std::string& expected_state, std::chrono::seconds timeout) {
                throw std::runtime_error("receive_loopback_code not implemented - requires TCP listener");
            }

            std::string parse_code_from_redirect(const std::string& input, const std::optional<std::string>& expected_state) {
                std::string trimmed = input;
                size_t start = trimmed.find_first_not_of(" \t\r\n");
                size_t end = trimmed.find_last_not_of(" \t\r\n");
                if (start != std::string::npos) {
                    trimmed = trimmed.substr(start, end - start + 1);
                }

                if (trimmed.empty()) {
                    throw std::runtime_error("No OAuth code provided");
                }

                std::string query = trimmed;
                size_t qpos = trimmed.find('?');
                if (qpos != std::string::npos) {
                    query = trimmed.substr(qpos + 1);
                }

                auto params = parse_query_params(query);

                if (params.count("code")) {
                    if (expected_state.has_value() && params.count("state")) {
                        if (params["state"] != expected_state.value()) {
                            throw std::runtime_error("OAuth state mismatch: expected " + expected_state.value() + ", got " + params["state"]);
                        }
                    }
                    return params["code"];
                }

                if (trimmed.size() > 10 && trimmed.find(' ') == std::string::npos && trimmed.find('&') == std::string::npos) {
                    return trimmed;
                }

                throw std::runtime_error("Could not parse OAuth code from input");
            }

            std::optional<std::string> extract_account_email_from_id_token(const std::string& id_token) {
                size_t first_dot = id_token.find('.');
                if (first_dot == std::string::npos) return std::nullopt;
                size_t second_dot = id_token.find('.', first_dot + 1);
                if (second_dot == std::string::npos) return std::nullopt;

                std::string payload_b64 = id_token.substr(first_dot + 1, second_dot - first_dot - 1);
                std::string payload = base64url_decode(payload_b64);

                try {
                    auto j = nlohmann::json::parse(payload);
                    if (j.contains("email") && j["email"].is_string()) {
                        return j["email"].get<std::string>();
                    }
                } catch (const nlohmann::json::exception&) {
                }

                return std::nullopt;
            }
        }

    }
}
