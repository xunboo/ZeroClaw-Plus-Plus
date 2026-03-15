#pragma once

#include "profiles.hpp"
#include "oauth_provider.hpp"
#include <string>
#include <optional>
#include <map>
#include <memory>
#include <mutex>
#include <chrono>

namespace zeroclaw {
    namespace auth {

        constexpr const char* OPENAI_CODEX_PROVIDER = "openai-codex";
        constexpr const char* ANTHROPIC_PROVIDER = "anthropic";
        constexpr const char* GEMINI_PROVIDER = "gemini";
        constexpr const char* DEFAULT_PROFILE_NAME = "default";
        constexpr uint64_t OPENAI_REFRESH_SKEW_SECS = 90;
        constexpr uint64_t OPENAI_REFRESH_FAILURE_BACKOFF_SECS = 10;

        std::string normalize_provider(const std::string& provider);
        std::string default_profile_id(const std::string& provider);
        std::optional<std::string> select_profile_id(const AuthProfilesData& data, const std::string& provider, const std::optional<std::string>& profile_override);

        class AuthService {
        public:
            AuthService(const std::string& state_dir, bool encrypt_secrets);
            ~AuthService();

            AuthProfilesData load_profiles();

            AuthProfile store_openai_tokens(const std::string& profile_name, const TokenSet& token_set, const std::optional<std::string>& account_id, bool set_active);
            AuthProfile store_gemini_tokens(const std::string& profile_name, const TokenSet& token_set, const std::optional<std::string>& account_id, bool set_active);
            AuthProfile store_provider_token(const std::string& provider, const std::string& profile_name, const std::string& token, const std::map<std::string, std::string>& metadata, bool set_active);

            std::string set_active_profile(const std::string& provider, const std::string& requested_profile);
            bool remove_profile(const std::string& provider, const std::string& requested_profile);

            std::optional<AuthProfile> get_profile(const std::string& provider, const std::optional<std::string>& profile_override);
            std::optional<std::string> get_provider_bearer_token(const std::string& provider, const std::optional<std::string>& profile_override);

            std::optional<std::string> get_valid_openai_access_token(const std::optional<std::string>& profile_override);
            std::optional<std::string> get_valid_gemini_access_token(const std::optional<std::string>& profile_override);

            std::optional<AuthProfile> get_gemini_profile(const std::optional<std::string>& profile_override);

        private:
            class Impl;
            std::unique_ptr<Impl> impl_;
        };

    }
}
