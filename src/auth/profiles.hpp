#pragma once

#include <string>
#include <optional>
#include <map>
#include <chrono>
#include <memory>
#include <functional>
#include <ctime>

namespace zeroclaw {
    namespace auth {

        enum class AuthProfileKind {
            OAuth,
            Token
        };

        struct TokenSet {
            std::string access_token;
            std::optional<std::string> refresh_token;
            std::optional<std::string> id_token;
            std::optional<std::chrono::system_clock::time_point> expires_at;
            std::optional<std::string> token_type;
            std::optional<std::string> scope;

            bool is_expiring_within(std::chrono::seconds skew) const;
        };

        struct AuthProfile {
            std::string id;
            std::string provider;
            std::string profile_name;
            AuthProfileKind kind;
            std::optional<std::string> account_id;
            std::optional<std::string> workspace_id;
            std::optional<TokenSet> token_set;
            std::optional<std::string> token;
            std::map<std::string, std::string> metadata;
            std::chrono::system_clock::time_point created_at;
            std::chrono::system_clock::time_point updated_at;

            static AuthProfile new_oauth(const std::string& provider, const std::string& profile_name, const TokenSet& token_set);
            static AuthProfile new_token(const std::string& provider, const std::string& profile_name, const std::string& token);
        };

        struct AuthProfilesData {
            uint32_t schema_version = 1;
            std::chrono::system_clock::time_point updated_at;
            std::map<std::string, std::string> active_profiles;
            std::map<std::string, AuthProfile> profiles;

            static AuthProfilesData create_default();
        };

        class SecretStore {
        public:
            SecretStore(const std::string& state_dir, bool encrypt_secrets);
            std::string encrypt(const std::string& plaintext);
            std::pair<std::string, std::optional<std::string>> decrypt_and_migrate(const std::string& ciphertext);
        private:
            std::string state_dir_;
            bool encrypt_secrets_;
        };

        class AuthProfilesStore {
        public:
            AuthProfilesStore(const std::string& state_dir, bool encrypt_secrets);
            ~AuthProfilesStore();

            const std::string& path() const;

            AuthProfilesData load();
            void upsert_profile(AuthProfile& profile, bool set_active);
            bool remove_profile(const std::string& profile_id);
            void set_active_profile(const std::string& provider, const std::string& profile_id);
            void clear_active_profile(const std::string& provider);
            AuthProfile update_profile(const std::string& profile_id, std::function<void(AuthProfile&)> updater);

        private:
            class Impl;
            std::unique_ptr<Impl> impl_;
        };

        std::string profile_id(const std::string& provider, const std::string& profile_name);

    }
}
