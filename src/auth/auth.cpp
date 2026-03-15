#include "auth.hpp"
#include <algorithm>
#include <cctype>
#include <map>
#include <mutex>
#include <shared_mutex>

namespace zeroclaw {
    namespace auth {

        namespace {
            std::string to_lower(const std::string& s) {
                std::string result = s;
                std::transform(result.begin(), result.end(), result.begin(),
                              [](unsigned char c) { return std::tolower(c); });
                return result;
            }

            std::string trim(const std::string& s) {
                size_t start = s.find_first_not_of(" \t\r\n");
                if (start == std::string::npos) return "";
                size_t end = s.find_last_not_of(" \t\r\n");
                return s.substr(start, end - start + 1);
            }

            std::string resolve_requested_profile_id(const std::string& provider, const std::string& requested) {
                if (requested.find(':') != std::string::npos) {
                    return requested;
                }
                return profile_id(provider, requested);
            }

            std::optional<uint64_t> refresh_backoff_remaining(const std::string& profile_id, std::map<std::string, std::chrono::steady_clock::time_point>& backoffs) {
                auto it = backoffs.find(profile_id);
                if (it == backoffs.end()) return std::nullopt;

                auto now = std::chrono::steady_clock::now();
                if (it->second <= now) {
                    backoffs.erase(it);
                    return std::nullopt;
                }

                auto remaining = std::chrono::duration_cast<std::chrono::seconds>(it->second - now);
                return std::max<uint64_t>(1, remaining.count());
            }

            void set_refresh_backoff(const std::string& profile_id, std::chrono::seconds duration, std::map<std::string, std::chrono::steady_clock::time_point>& backoffs) {
                backoffs[profile_id] = std::chrono::steady_clock::now() + duration;
            }

            void clear_refresh_backoff(const std::string& profile_id, std::map<std::string, std::chrono::steady_clock::time_point>& backoffs) {
                backoffs.erase(profile_id);
            }
        }

        std::string normalize_provider(const std::string& provider) {
            std::string normalized = to_lower(trim(provider));

            if (normalized == "openai-codex" || normalized == "openai_codex" || normalized == "codex") {
                return OPENAI_CODEX_PROVIDER;
            }
            if (normalized == "anthropic" || normalized == "claude" || normalized == "claude-code") {
                return ANTHROPIC_PROVIDER;
            }
            if (normalized == "gemini" || normalized == "google" || normalized == "vertex") {
                return GEMINI_PROVIDER;
            }
            if (!normalized.empty()) {
                return normalized;
            }

            throw std::runtime_error("Provider name cannot be empty");
        }

        std::string default_profile_id(const std::string& provider) {
            return profile_id(provider, DEFAULT_PROFILE_NAME);
        }

        std::optional<std::string> select_profile_id(const AuthProfilesData& data, const std::string& provider, const std::optional<std::string>& profile_override) {
            if (profile_override.has_value()) {
                std::string requested = resolve_requested_profile_id(provider, profile_override.value());
                if (data.profiles.count(requested)) {
                    return requested;
                }
                return std::nullopt;
            }

            auto active_it = data.active_profiles.find(provider);
            if (active_it != data.active_profiles.end()) {
                if (data.profiles.count(active_it->second)) {
                    return active_it->second;
                }
            }

            std::string default_id = default_profile_id(provider);
            if (data.profiles.count(default_id)) {
                return default_id;
            }

            for (const auto& [id, profile] : data.profiles) {
                if (profile.provider == provider) {
                    return id;
                }
            }

            return std::nullopt;
        }

        class AuthService::Impl {
        public:
            AuthProfilesStore store;
            std::unique_ptr<HttpClient> client;
            std::map<std::string, std::chrono::steady_clock::time_point> refresh_backoffs;
            std::mutex backoff_mutex;
            std::shared_mutex profile_mutex;

            Impl(const std::string& state_dir, bool encrypt_secrets)
                : store(state_dir, encrypt_secrets)
                , client(create_http_client()) {}
        };

        AuthService::AuthService(const std::string& state_dir, bool encrypt_secrets)
            : impl_(std::make_unique<Impl>(state_dir, encrypt_secrets)) {}

        AuthService::~AuthService() = default;

        AuthProfilesData AuthService::load_profiles() {
            return impl_->store.load();
        }

        AuthProfile AuthService::store_openai_tokens(const std::string& profile_name, const TokenSet& token_set, const std::optional<std::string>& account_id, bool set_active) {
            auto profile = AuthProfile::new_oauth(OPENAI_CODEX_PROVIDER, profile_name, token_set);
            profile.account_id = account_id;
            impl_->store.upsert_profile(profile, set_active);
            return profile;
        }

        AuthProfile AuthService::store_gemini_tokens(const std::string& profile_name, const TokenSet& token_set, const std::optional<std::string>& account_id, bool set_active) {
            auto profile = AuthProfile::new_oauth(GEMINI_PROVIDER, profile_name, token_set);
            profile.account_id = account_id;
            impl_->store.upsert_profile(profile, set_active);
            return profile;
        }

        AuthProfile AuthService::store_provider_token(const std::string& provider, const std::string& profile_name, const std::string& token, const std::map<std::string, std::string>& metadata, bool set_active) {
            auto profile = AuthProfile::new_token(provider, profile_name, token);
            for (const auto& [k, v] : metadata) {
                profile.metadata[k] = v;
            }
            impl_->store.upsert_profile(profile, set_active);
            return profile;
        }

        std::string AuthService::set_active_profile(const std::string& provider, const std::string& requested_profile) {
            std::string normalized = normalize_provider(provider);
            auto data = impl_->store.load();
            std::string pid = resolve_requested_profile_id(normalized, requested_profile);

            auto it = data.profiles.find(pid);
            if (it == data.profiles.end()) {
                throw std::runtime_error("Auth profile not found: " + pid);
            }

            if (it->second.provider != normalized) {
                throw std::runtime_error("Profile " + pid + " belongs to provider " + it->second.provider + ", not " + normalized);
            }

            impl_->store.set_active_profile(normalized, pid);
            return pid;
        }

        bool AuthService::remove_profile(const std::string& provider, const std::string& requested_profile) {
            std::string normalized = normalize_provider(provider);
            std::string pid = resolve_requested_profile_id(normalized, requested_profile);
            return impl_->store.remove_profile(pid);
        }

        std::optional<AuthProfile> AuthService::get_profile(const std::string& provider, const std::optional<std::string>& profile_override) {
            std::string normalized = normalize_provider(provider);
            auto data = impl_->store.load();
            auto pid = select_profile_id(data, normalized, profile_override);
            if (!pid.has_value()) {
                return std::nullopt;
            }
            auto it = data.profiles.find(pid.value());
            if (it == data.profiles.end()) {
                return std::nullopt;
            }
            return it->second;
        }

        std::optional<std::string> AuthService::get_provider_bearer_token(const std::string& provider, const std::optional<std::string>& profile_override) {
            auto profile = get_profile(provider, profile_override);
            if (!profile.has_value()) {
                return std::nullopt;
            }

            std::optional<std::string> credential;
            if (profile->kind == AuthProfileKind::Token) {
                credential = profile->token;
            } else if (profile->token_set.has_value()) {
                credential = profile->token_set->access_token;
            }

            if (credential.has_value() && !trim(credential.value()).empty()) {
                return credential;
            }
            return std::nullopt;
        }

        std::optional<std::string> AuthService::get_valid_openai_access_token(const std::optional<std::string>& profile_override) {
            std::lock_guard<std::shared_mutex> lock(impl_->profile_mutex);

            auto data = impl_->store.load();
            auto pid = select_profile_id(data, OPENAI_CODEX_PROVIDER, profile_override);
            if (!pid.has_value()) return std::nullopt;

            auto it = data.profiles.find(pid.value());
            if (it == data.profiles.end()) return std::nullopt;

            auto& profile = it->second;
            if (!profile.token_set.has_value()) {
                throw std::runtime_error("OpenAI Codex auth profile is not OAuth-based: " + pid.value());
            }

            auto& token_set = profile.token_set.value();
            if (!token_set.is_expiring_within(std::chrono::seconds(OPENAI_REFRESH_SKEW_SECS))) {
                return token_set.access_token;
            }

            auto refresh_token = token_set.refresh_token;
            if (!refresh_token.has_value()) {
                return token_set.access_token;
            }

            data = impl_->store.load();
            auto latest_it = data.profiles.find(pid.value());
            if (latest_it == data.profiles.end()) return std::nullopt;

            auto& latest_profile = latest_it->second;
            if (!latest_profile.token_set.has_value()) {
                throw std::runtime_error("OpenAI Codex auth profile is missing token set: " + pid.value());
            }

            auto& latest_tokens = latest_profile.token_set.value();
            if (!latest_tokens.is_expiring_within(std::chrono::seconds(OPENAI_REFRESH_SKEW_SECS))) {
                return latest_tokens.access_token;
            }

            std::string rt = latest_tokens.refresh_token.has_value() ? latest_tokens.refresh_token.value() : refresh_token.value();

            {
                std::lock_guard<std::mutex> backoff_lock(impl_->backoff_mutex);
                auto remaining = refresh_backoff_remaining(pid.value(), impl_->refresh_backoffs);
                if (remaining.has_value()) {
                    throw std::runtime_error("OpenAI token refresh is in backoff for " + std::to_string(remaining.value()) + "s due to previous failures");
                }
            }

            TokenSet refreshed;
            try {
                refreshed = openai::refresh_access_token(*impl_->client, rt);
                std::lock_guard<std::mutex> backoff_lock(impl_->backoff_mutex);
                clear_refresh_backoff(pid.value(), impl_->refresh_backoffs);
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> backoff_lock(impl_->backoff_mutex);
                set_refresh_backoff(pid.value(), std::chrono::seconds(OPENAI_REFRESH_FAILURE_BACKOFF_SECS), impl_->refresh_backoffs);
                throw;
            }

            if (!refreshed.refresh_token.has_value()) {
                refreshed.refresh_token = latest_tokens.refresh_token;
            }

            auto account_id = openai::extract_account_id_from_jwt(refreshed.access_token);
            if (!account_id.has_value()) {
                account_id = latest_profile.account_id;
            }

            auto updated = impl_->store.update_profile(pid.value(), [&](AuthProfile& p) {
                p.kind = AuthProfileKind::OAuth;
                p.token_set = refreshed;
                p.account_id = account_id;
            });

            return updated.token_set.has_value() ? std::optional<std::string>(updated.token_set->access_token) : std::nullopt;
        }

        std::optional<std::string> AuthService::get_valid_gemini_access_token(const std::optional<std::string>& profile_override) {
            std::lock_guard<std::shared_mutex> lock(impl_->profile_mutex);

            auto data = impl_->store.load();
            auto pid = select_profile_id(data, GEMINI_PROVIDER, profile_override);
            if (!pid.has_value()) return std::nullopt;

            auto it = data.profiles.find(pid.value());
            if (it == data.profiles.end()) return std::nullopt;

            auto& profile = it->second;
            if (!profile.token_set.has_value()) {
                throw std::runtime_error("Gemini auth profile is not OAuth-based: " + pid.value());
            }

            auto& token_set = profile.token_set.value();
            if (!token_set.is_expiring_within(std::chrono::seconds(OPENAI_REFRESH_SKEW_SECS))) {
                return token_set.access_token;
            }

            auto refresh_token = token_set.refresh_token;
            if (!refresh_token.has_value()) {
                return token_set.access_token;
            }

            data = impl_->store.load();
            auto latest_it = data.profiles.find(pid.value());
            if (latest_it == data.profiles.end()) return std::nullopt;

            auto& latest_profile = latest_it->second;
            if (!latest_profile.token_set.has_value()) {
                throw std::runtime_error("Gemini auth profile is missing token set: " + pid.value());
            }

            auto& latest_tokens = latest_profile.token_set.value();
            if (!latest_tokens.is_expiring_within(std::chrono::seconds(OPENAI_REFRESH_SKEW_SECS))) {
                return latest_tokens.access_token;
            }

            std::string rt = latest_tokens.refresh_token.has_value() ? latest_tokens.refresh_token.value() : refresh_token.value();

            {
                std::lock_guard<std::mutex> backoff_lock(impl_->backoff_mutex);
                auto remaining = refresh_backoff_remaining(pid.value(), impl_->refresh_backoffs);
                if (remaining.has_value()) {
                    throw std::runtime_error("Gemini token refresh is in backoff for " + std::to_string(remaining.value()) + "s due to previous failures");
                }
            }

            TokenSet refreshed;
            try {
                refreshed = gemini::refresh_access_token(*impl_->client, rt);
                std::lock_guard<std::mutex> backoff_lock(impl_->backoff_mutex);
                clear_refresh_backoff(pid.value(), impl_->refresh_backoffs);
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> backoff_lock(impl_->backoff_mutex);
                set_refresh_backoff(pid.value(), std::chrono::seconds(OPENAI_REFRESH_FAILURE_BACKOFF_SECS), impl_->refresh_backoffs);
                throw;
            }

            if (!refreshed.refresh_token.has_value()) {
                refreshed.refresh_token = latest_tokens.refresh_token;
            }

            std::optional<std::string> account_id;
            if (refreshed.id_token.has_value()) {
                account_id = gemini::extract_account_email_from_id_token(refreshed.id_token.value());
            }
            if (!account_id.has_value()) {
                account_id = latest_profile.account_id;
            }

            auto updated = impl_->store.update_profile(pid.value(), [&](AuthProfile& p) {
                p.kind = AuthProfileKind::OAuth;
                p.token_set = refreshed;
                p.account_id = account_id;
            });

            return updated.token_set.has_value() ? std::optional<std::string>(updated.token_set->access_token) : std::nullopt;
        }

        std::optional<AuthProfile> AuthService::get_gemini_profile(const std::optional<std::string>& profile_override) {
            return get_profile(GEMINI_PROVIDER, profile_override);
        }

    }
}
