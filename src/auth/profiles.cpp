#include "profiles.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <filesystem>

namespace zeroclaw {
    namespace auth {

        namespace {
            constexpr uint32_t CURRENT_SCHEMA_VERSION = 1;
            constexpr const char* PROFILES_FILENAME = "auth-profiles.json";
            constexpr const char* LOCK_FILENAME = "auth-profiles.lock";
            constexpr uint64_t LOCK_WAIT_MS = 50;
            constexpr uint64_t LOCK_TIMEOUT_MS = 10000;

            std::string time_point_to_rfc3339(const std::chrono::system_clock::time_point& tp) {
                auto time = std::chrono::system_clock::to_time_t(tp);
                std::tm tm = *std::gmtime(&time);
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()) % 1000;
                std::ostringstream oss;
                oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
                oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
                return oss.str();
            }

            std::chrono::system_clock::time_point rfc3339_to_time_point(const std::string& s) {
                std::tm tm = {};
                std::istringstream iss(s);
                iss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
                if (iss.fail()) {
                    return std::chrono::system_clock::now();
                }
                auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
                return tp;
            }

            std::string auth_profile_kind_to_string(AuthProfileKind kind) {
                switch (kind) {
                    case AuthProfileKind::OAuth: return "oauth";
                    case AuthProfileKind::Token: return "token";
                }
                return "token";
            }

            AuthProfileKind parse_profile_kind(const std::string& value) {
                if (value == "oauth") return AuthProfileKind::OAuth;
                return AuthProfileKind::Token;
            }
        }

        bool TokenSet::is_expiring_within(std::chrono::seconds skew) const {
            if (!expires_at.has_value()) return false;
            auto now_plus_skew = std::chrono::system_clock::now() + skew;
            return expires_at.value() <= now_plus_skew;
        }

        AuthProfile AuthProfile::new_oauth(const std::string& provider, const std::string& profile_name, const TokenSet& token_set) {
            auto now = std::chrono::system_clock::now();
            AuthProfile profile;
            profile.id = zeroclaw::auth::profile_id(provider, profile_name);
            profile.provider = provider;
            profile.profile_name = profile_name;
            profile.kind = AuthProfileKind::OAuth;
            profile.token_set = token_set;
            profile.created_at = now;
            profile.updated_at = now;
            return profile;
        }

        AuthProfile AuthProfile::new_token(const std::string& provider, const std::string& profile_name, const std::string& token) {
            auto now = std::chrono::system_clock::now();
            AuthProfile profile;
            profile.id = zeroclaw::auth::profile_id(provider, profile_name);
            profile.provider = provider;
            profile.profile_name = profile_name;
            profile.kind = AuthProfileKind::Token;
            profile.token = token;
            profile.created_at = now;
            profile.updated_at = now;
            return profile;
        }

        AuthProfilesData AuthProfilesData::create_default() {
            AuthProfilesData data;
            data.updated_at = std::chrono::system_clock::now();
            return data;
        }

        SecretStore::SecretStore(const std::string& state_dir, bool encrypt_secrets)
            : state_dir_(state_dir), encrypt_secrets_(encrypt_secrets) {}

        std::string SecretStore::encrypt(const std::string& plaintext) {
            if (!encrypt_secrets_) return plaintext;
            return "enc2:" + plaintext;
        }

        std::pair<std::string, std::optional<std::string>> SecretStore::decrypt_and_migrate(const std::string& ciphertext) {
            if (ciphertext.empty()) return {"", std::nullopt};
            if (ciphertext.rfind("enc2:", 0) == 0) {
                return {ciphertext.substr(5), std::nullopt};
            }
            return {ciphertext, ciphertext};
        }

        class AuthProfilesStore::Impl {
        public:
            std::filesystem::path path_;
            std::filesystem::path lock_path_;
            SecretStore secret_store_;

            Impl(const std::string& state_dir, bool encrypt_secrets)
                : path_(std::filesystem::path(state_dir) / PROFILES_FILENAME)
                , lock_path_(std::filesystem::path(state_dir) / LOCK_FILENAME)
                , secret_store_(state_dir, encrypt_secrets) {}

            std::string acquire_lock() {
                auto parent = lock_path_.parent_path();
                if (!std::filesystem::exists(parent)) {
                    std::filesystem::create_directories(parent);
                }

                uint64_t waited = 0;
                while (waited < LOCK_TIMEOUT_MS) {
                    std::ofstream lock_file(lock_path_, std::ios::out | std::ios::trunc);
                    if (lock_file.is_open()) {
                        lock_file << "pid=" << std::this_thread::get_id();
                        return lock_path_.string();
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(LOCK_WAIT_MS));
                    waited += LOCK_WAIT_MS;
                }
                throw std::runtime_error("Timed out waiting for auth profile lock");
            }

            void release_lock(const std::string& lock_path) {
                std::error_code ec;
                std::filesystem::remove(lock_path, ec);
            }

            AuthProfilesData load_locked() {
                if (!std::filesystem::exists(path_)) {
                    return AuthProfilesData::create_default();
                }

                std::ifstream file(path_);
                if (!file.is_open()) {
                    return AuthProfilesData::create_default();
                }

                nlohmann::json j;
                file >> j;

                AuthProfilesData data;
                data.schema_version = j.value("schema_version", CURRENT_SCHEMA_VERSION);
                data.updated_at = rfc3339_to_time_point(j.value("updated_at", time_point_to_rfc3339(std::chrono::system_clock::now())));

                if (j.contains("active_profiles")) {
                    for (auto& [k, v] : j["active_profiles"].items()) {
                        data.active_profiles[k] = v.get<std::string>();
                    }
                }

                auto get_opt_str = [](const nlohmann::json& obj, const std::string& key) -> std::optional<std::string> {
                    if (obj.contains(key) && obj[key].is_string()) {
                        return obj[key].get<std::string>();
                    }
                    if (obj.contains(key) && !obj[key].is_null()) {
                        return obj[key].get<std::string>(); // fallback if convertible
                    }
                    return std::nullopt;
                };

                if (j.contains("profiles")) {
                    for (auto& [id, pj] : j["profiles"].items()) {
                        AuthProfile profile;
                        profile.id = id;
                        profile.provider = pj.value("provider", "");
                        profile.profile_name = pj.value("profile_name", "");
                        profile.kind = parse_profile_kind(pj.value("kind", "token"));
                        profile.account_id = get_opt_str(pj, "account_id");
                        profile.workspace_id = get_opt_str(pj, "workspace_id");
                        profile.created_at = rfc3339_to_time_point(pj.value("created_at", time_point_to_rfc3339(std::chrono::system_clock::now())));
                        profile.updated_at = rfc3339_to_time_point(pj.value("updated_at", time_point_to_rfc3339(std::chrono::system_clock::now())));

                        for (auto& [mk, mv] : pj.value("metadata", nlohmann::json::object()).items()) {
                            profile.metadata[mk] = mv.get<std::string>();
                        }

                        if (profile.kind == AuthProfileKind::OAuth) {
                            TokenSet ts;
                            auto [access, _] = secret_store_.decrypt_and_migrate(pj.value("access_token", ""));
                            ts.access_token = access;
                            auto [refresh, __] = secret_store_.decrypt_and_migrate(pj.value("refresh_token", ""));
                            ts.refresh_token = refresh.empty() ? std::nullopt : std::optional<std::string>(refresh);
                            auto [id_tok, ___] = secret_store_.decrypt_and_migrate(pj.value("id_token", ""));
                            ts.id_token = id_tok.empty() ? std::nullopt : std::optional<std::string>(id_tok);
                            if (pj.contains("expires_at")) {
                                ts.expires_at = rfc3339_to_time_point(pj["expires_at"].get<std::string>());
                            }
                            ts.token_type = get_opt_str(pj, "token_type");
                            ts.scope = get_opt_str(pj, "scope");
                            profile.token_set = ts;
                        } else {
                            auto [tok, ____] = secret_store_.decrypt_and_migrate(pj.value("token", ""));
                            profile.token = tok.empty() ? std::nullopt : std::optional<std::string>(tok);
                        }

                        data.profiles[id] = profile;
                    }
                }

                return data;
            }

            void save_locked(const AuthProfilesData& data) {
                nlohmann::json j;
                j["schema_version"] = data.schema_version;
                j["updated_at"] = time_point_to_rfc3339(data.updated_at);
                j["active_profiles"] = data.active_profiles;

                nlohmann::json profiles_json = nlohmann::json::object();
                for (const auto& [id, profile] : data.profiles) {
                    nlohmann::json pj;
                    pj["provider"] = profile.provider;
                    pj["profile_name"] = profile.profile_name;
                    pj["kind"] = auth_profile_kind_to_string(profile.kind);
                    pj["account_id"] = profile.account_id.has_value() ? nlohmann::json(profile.account_id.value()) : nlohmann::json(nullptr);
                    pj["workspace_id"] = profile.workspace_id.has_value() ? nlohmann::json(profile.workspace_id.value()) : nlohmann::json(nullptr);
                    pj["created_at"] = time_point_to_rfc3339(profile.created_at);
                    pj["updated_at"] = time_point_to_rfc3339(profile.updated_at);
                    pj["metadata"] = profile.metadata;

                    if (profile.kind == AuthProfileKind::OAuth && profile.token_set.has_value()) {
                        const auto& ts = profile.token_set.value();
                        pj["access_token"] = secret_store_.encrypt(ts.access_token);
                        if (ts.refresh_token.has_value()) {
                            pj["refresh_token"] = secret_store_.encrypt(ts.refresh_token.value());
                        }
                        if (ts.id_token.has_value()) {
                            pj["id_token"] = secret_store_.encrypt(ts.id_token.value());
                        }
                        if (ts.expires_at.has_value()) {
                            pj["expires_at"] = time_point_to_rfc3339(ts.expires_at.value());
                        }
                        pj["token_type"] = ts.token_type.has_value() ? nlohmann::json(ts.token_type.value()) : nlohmann::json(nullptr);
                        pj["scope"] = ts.scope.has_value() ? nlohmann::json(ts.scope.value()) : nlohmann::json(nullptr);
                    } else if (profile.token.has_value()) {
                        pj["token"] = secret_store_.encrypt(profile.token.value());
                    }

                    profiles_json[id] = pj;
                }
                j["profiles"] = profiles_json;

                auto parent = path_.parent_path();
                if (!std::filesystem::exists(parent)) {
                    std::filesystem::create_directories(parent);
                }

                auto tmp_path = path_.string() + ".tmp." + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));
                {
                    std::ofstream tmp_file(tmp_path);
                    tmp_file << j.dump(2);
                }
                std::filesystem::rename(tmp_path, path_);
            }
        };

        AuthProfilesStore::AuthProfilesStore(const std::string& state_dir, bool encrypt_secrets)
            : impl_(std::make_unique<Impl>(state_dir, encrypt_secrets)) {}

        AuthProfilesStore::~AuthProfilesStore() = default;

        const std::string& AuthProfilesStore::path() const {
            static std::string p = impl_->path_.string();
            return p;
        }

        AuthProfilesData AuthProfilesStore::load() {
            auto lock = impl_->acquire_lock();
            auto result = impl_->load_locked();
            impl_->release_lock(lock);
            return result;
        }

        void AuthProfilesStore::upsert_profile(AuthProfile& profile, bool set_active) {
            auto lock = impl_->acquire_lock();
            auto data = impl_->load_locked();
            profile.updated_at = std::chrono::system_clock::now();
            if (data.profiles.count(profile.id)) {
                profile.created_at = data.profiles.at(profile.id).created_at;
            }
            if (set_active) {
                data.active_profiles[profile.provider] = profile.id;
            }
            data.profiles[profile.id] = profile;
            data.updated_at = std::chrono::system_clock::now();
            impl_->save_locked(data);
            impl_->release_lock(lock);
        }

        bool AuthProfilesStore::remove_profile(const std::string& profile_id) {
            auto lock = impl_->acquire_lock();
            auto data = impl_->load_locked();
            if (!data.profiles.erase(profile_id)) {
                impl_->release_lock(lock);
                return false;
            }
            for (auto it = data.active_profiles.begin(); it != data.active_profiles.end(); ) {
                if (it->second == profile_id) {
                    it = data.active_profiles.erase(it);
                } else {
                    ++it;
                }
            }
            data.updated_at = std::chrono::system_clock::now();
            impl_->save_locked(data);
            impl_->release_lock(lock);
            return true;
        }

        void AuthProfilesStore::set_active_profile(const std::string& provider, const std::string& profile_id) {
            auto lock = impl_->acquire_lock();
            auto data = impl_->load_locked();
            if (!data.profiles.count(profile_id)) {
                impl_->release_lock(lock);
                throw std::runtime_error("Auth profile not found: " + profile_id);
            }
            data.active_profiles[provider] = profile_id;
            data.updated_at = std::chrono::system_clock::now();
            impl_->save_locked(data);
            impl_->release_lock(lock);
        }

        void AuthProfilesStore::clear_active_profile(const std::string& provider) {
            auto lock = impl_->acquire_lock();
            auto data = impl_->load_locked();
            data.active_profiles.erase(provider);
            data.updated_at = std::chrono::system_clock::now();
            impl_->save_locked(data);
            impl_->release_lock(lock);
        }

        AuthProfile AuthProfilesStore::update_profile(const std::string& profile_id, std::function<void(AuthProfile&)> updater) {
            auto lock = impl_->acquire_lock();
            auto data = impl_->load_locked();
            auto it = data.profiles.find(profile_id);
            if (it == data.profiles.end()) {
                impl_->release_lock(lock);
                throw std::runtime_error("Auth profile not found: " + profile_id);
            }
            updater(it->second);
            it->second.updated_at = std::chrono::system_clock::now();
            auto result = it->second;
            data.updated_at = std::chrono::system_clock::now();
            impl_->save_locked(data);
            impl_->release_lock(lock);
            return result;
        }

        std::string profile_id(const std::string& provider, const std::string& profile_name) {
            return provider + ":" + profile_name;
        }

    }
}
