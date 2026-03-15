#pragma once

#include "types.hpp"

#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef USE_JSON
#include <nlohmann/json.hpp>
#endif

namespace zeroclaw::cost {

struct DateParts {
    int year;
    int month;
    int day;

    static DateParts from_time_t(std::time_t t) {
        std::tm* tm = std::localtime(&t);
        return {tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday};
    }

    static DateParts now() {
        return from_time_t(std::time(nullptr));
    }

    bool operator==(const DateParts& other) const {
        return year == other.year && month == other.month && day == other.day;
    }

    bool operator!=(const DateParts& other) const {
        return !(*this == other);
    }
};

class CostStorage;

class CostTracker {
public:
    CostTracker(CostConfig config, const std::filesystem::path& workspace_dir);

    const std::string& session_id() const { return session_id_; }

    BudgetCheck check_budget(double estimated_cost_usd);
    void record_usage(const TokenUsage& usage);
    CostSummary get_summary();
    double get_daily_cost(int year, int month, int day);
    double get_monthly_cost(int year, int month);

private:
    CostConfig config_;
    std::unique_ptr<CostStorage> storage_;
    std::string session_id_;
    std::vector<CostRecord> session_costs_;
    std::mutex storage_mutex_;
    std::mutex session_mutex_;

    static std::string generate_uuid();
    static std::filesystem::path resolve_storage_path(const std::filesystem::path& workspace_dir);
    static std::unordered_map<std::string, ModelStats> build_session_model_stats(
        const std::vector<CostRecord>& session_costs);
};

class CostStorage {
public:
    explicit CostStorage(const std::filesystem::path& path);

    void add_record(const CostRecord& record);
    std::pair<double, double> get_aggregated_costs();
    double get_cost_for_date(int year, int month, int day);
    double get_cost_for_month(int year, int month);

private:
    std::filesystem::path path_;
    double daily_cost_usd_;
    double monthly_cost_usd_;
    DateParts cached_day_;
    int cached_year_;
    int cached_month_;

    template<typename F>
    void for_each_record(F&& on_record);

    void rebuild_aggregates(const DateParts& day, int year, int month);
    void ensure_period_cache_current();
};

inline CostTracker::CostTracker(CostConfig config, const std::filesystem::path& workspace_dir)
    : config_(std::move(config))
    , session_id_(generate_uuid())
{
    auto storage_path = resolve_storage_path(workspace_dir);
    storage_ = std::make_unique<CostStorage>(storage_path);
}

inline BudgetCheck CostTracker::check_budget(double estimated_cost_usd) {
    if (!config_.enabled) {
        return BudgetCheck::allowed();
    }

    if (!std::isfinite(estimated_cost_usd) || estimated_cost_usd < 0.0) {
        throw std::invalid_argument("Estimated cost must be a finite, non-negative value");
    }

    auto [daily_cost, monthly_cost] = [&]() {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        return storage_->get_aggregated_costs();
    }();

    double projected_daily = daily_cost + estimated_cost_usd;
    if (projected_daily > config_.daily_limit_usd) {
        return BudgetCheck::exceeded(daily_cost, config_.daily_limit_usd, UsagePeriod::Day);
    }

    double projected_monthly = monthly_cost + estimated_cost_usd;
    if (projected_monthly > config_.monthly_limit_usd) {
        return BudgetCheck::exceeded(monthly_cost, config_.monthly_limit_usd, UsagePeriod::Month);
    }

    double warn_threshold = static_cast<double>(std::min(config_.warn_at_percent, 100u)) / 100.0;
    double daily_warn_threshold = config_.daily_limit_usd * warn_threshold;
    double monthly_warn_threshold = config_.monthly_limit_usd * warn_threshold;

    if (projected_daily >= daily_warn_threshold) {
        return BudgetCheck::warning(daily_cost, config_.daily_limit_usd, UsagePeriod::Day);
    }

    if (projected_monthly >= monthly_warn_threshold) {
        return BudgetCheck::warning(monthly_cost, config_.monthly_limit_usd, UsagePeriod::Month);
    }

    return BudgetCheck::allowed();
}

inline void CostTracker::record_usage(const TokenUsage& usage) {
    if (!config_.enabled) {
        return;
    }

    if (!std::isfinite(usage.cost_usd) || usage.cost_usd < 0.0) {
        throw std::invalid_argument("Token usage cost must be a finite, non-negative value");
    }

    CostRecord record(session_id_, usage);

    {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        storage_->add_record(record);
    }

    {
        std::lock_guard<std::mutex> lock(session_mutex_);
        session_costs_.push_back(std::move(record));
    }
}

inline CostSummary CostTracker::get_summary() {
    auto [daily_cost, monthly_cost] = [&]() {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        return storage_->get_aggregated_costs();
    }();

    std::lock_guard<std::mutex> lock(session_mutex_);
    
    double session_cost = 0.0;
    std::uint64_t total_tokens = 0;
    for (const auto& record : session_costs_) {
        session_cost += record.usage.cost_usd;
        total_tokens += record.usage.total_tokens;
    }

    auto by_model = build_session_model_stats(session_costs_);

    CostSummary summary;
    summary.session_cost_usd = session_cost;
    summary.daily_cost_usd = daily_cost;
    summary.monthly_cost_usd = monthly_cost;
    summary.total_tokens = total_tokens;
    summary.request_count = session_costs_.size();
    summary.by_model = std::move(by_model);

    return summary;
}

inline double CostTracker::get_daily_cost(int year, int month, int day) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    return storage_->get_cost_for_date(year, month, day);
}

inline double CostTracker::get_monthly_cost(int year, int month) {
    std::lock_guard<std::mutex> lock(storage_mutex_);
    return storage_->get_cost_for_month(year, month);
}

inline std::string CostTracker::generate_uuid() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; ++i) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; ++i) ss << dis(gen);
    ss << "-4";
    for (int i = 0; i < 3; ++i) ss << dis(gen);
    ss << "-";
    ss << dis2(gen);
    for (int i = 0; i < 3; ++i) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; ++i) ss << dis(gen);
    return ss.str();
}

inline std::filesystem::path CostTracker::resolve_storage_path(const std::filesystem::path& workspace_dir) {
    auto storage_path = workspace_dir / "state" / "costs.jsonl";
    auto legacy_path = workspace_dir / ".zeroclaw" / "costs.db";

    if (!std::filesystem::exists(storage_path) && std::filesystem::exists(legacy_path)) {
        if (storage_path.has_parent_path()) {
            std::filesystem::create_directories(storage_path.parent_path());
        }

        std::error_code ec;
        std::filesystem::rename(legacy_path, storage_path, ec);
        if (ec) {
            std::filesystem::copy_file(legacy_path, storage_path);
        }
    }

    return storage_path;
}

inline std::unordered_map<std::string, ModelStats> CostTracker::build_session_model_stats(
    const std::vector<CostRecord>& session_costs) {
    std::unordered_map<std::string, ModelStats> by_model;

    for (const auto& record : session_costs) {
        auto it = by_model.find(record.usage.model);
        if (it == by_model.end()) {
            ModelStats stats;
            stats.model = record.usage.model;
            stats.cost_usd = record.usage.cost_usd;
            stats.total_tokens = record.usage.total_tokens;
            stats.request_count = 1;
            by_model[record.usage.model] = stats;
        } else {
            it->second.cost_usd += record.usage.cost_usd;
            it->second.total_tokens += record.usage.total_tokens;
            it->second.request_count += 1;
        }
    }

    return by_model;
}

inline CostStorage::CostStorage(const std::filesystem::path& path)
    : path_(path)
    , daily_cost_usd_(0.0)
    , monthly_cost_usd_(0.0)
{
    if (path_.has_parent_path()) {
        std::filesystem::create_directories(path_.parent_path());
    }

    auto now = DateParts::now();
    cached_day_ = now;
    cached_year_ = now.year;
    cached_month_ = now.month;

    rebuild_aggregates(cached_day_, cached_year_, cached_month_);
}

inline void CostStorage::add_record(const CostRecord& record) {
    std::ofstream file(path_, std::ios::app);
    if (!file) {
        throw std::runtime_error("Failed to open cost storage at " + path_.string());
    }

#ifdef USE_JSON
    file << record.to_json().dump() << "\n";
#else
    auto timestamp = std::chrono::system_clock::to_time_t(record.usage.timestamp);
    file << record.id << "|" << record.session_id << "|" 
         << record.usage.model << "|" << record.usage.input_tokens << "|"
         << record.usage.output_tokens << "|" << record.usage.total_tokens << "|"
         << record.usage.cost_usd << "|" << timestamp << "\n";
#endif
    file.flush();

    ensure_period_cache_current();

    auto record_date = DateParts::from_time_t(
        std::chrono::system_clock::to_time_t(record.usage.timestamp));

    if (record_date == cached_day_) {
        daily_cost_usd_ += record.usage.cost_usd;
    }
    if (record_date.year == cached_year_ && record_date.month == cached_month_) {
        monthly_cost_usd_ += record.usage.cost_usd;
    }
}

inline std::pair<double, double> CostStorage::get_aggregated_costs() {
    ensure_period_cache_current();
    return {daily_cost_usd_, monthly_cost_usd_};
}

inline double CostStorage::get_cost_for_date(int year, int month, int day) {
    double cost = 0.0;

    for_each_record([&](const CostRecord& record) {
        auto record_date = DateParts::from_time_t(
            std::chrono::system_clock::to_time_t(record.usage.timestamp));

        if (record_date.year == year && record_date.month == month && record_date.day == day) {
            cost += record.usage.cost_usd;
        }
    });

    return cost;
}

inline double CostStorage::get_cost_for_month(int year, int month) {
    double cost = 0.0;

    for_each_record([&](const CostRecord& record) {
        auto record_date = DateParts::from_time_t(
            std::chrono::system_clock::to_time_t(record.usage.timestamp));

        if (record_date.year == year && record_date.month == month) {
            cost += record.usage.cost_usd;
        }
    });

    return cost;
}

template<typename F>
inline void CostStorage::for_each_record(F&& on_record) {
    if (!std::filesystem::exists(path_)) {
        return;
    }

    std::ifstream file(path_);
    if (!file) {
        throw std::runtime_error("Failed to read cost storage from " + path_.string());
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue;
        }

        try {
#ifdef USE_JSON
            auto j = nlohmann::json::parse(line);
            auto record = CostRecord::from_json(j);
            on_record(record);
#else
            CostRecord record;
            std::istringstream iss(line);
            std::string token;
            
            std::getline(iss, record.id, '|');
            std::getline(iss, record.session_id, '|');
            std::getline(iss, record.usage.model, '|');
            
            std::getline(iss, token, '|');
            record.usage.input_tokens = std::stoull(token);
            
            std::getline(iss, token, '|');
            record.usage.output_tokens = std::stoull(token);
            
            std::getline(iss, token, '|');
            record.usage.total_tokens = std::stoull(token);
            
            std::getline(iss, token, '|');
            record.usage.cost_usd = std::stod(token);
            
            std::getline(iss, token);
            auto timestamp = std::chrono::system_clock::from_time_t(std::stoll(token));
            record.usage.timestamp = timestamp;
            
            on_record(record);
#endif
        } catch (const std::exception&) {
        }
    }
}

inline void CostStorage::rebuild_aggregates(const DateParts& day, int year, int month) {
    double daily_cost = 0.0;
    double monthly_cost = 0.0;

    for_each_record([&](const CostRecord& record) {
        auto record_date = DateParts::from_time_t(
            std::chrono::system_clock::to_time_t(record.usage.timestamp));

        if (record_date == day) {
            daily_cost += record.usage.cost_usd;
        }

        if (record_date.year == year && record_date.month == month) {
            monthly_cost += record.usage.cost_usd;
        }
    });

    daily_cost_usd_ = daily_cost;
    monthly_cost_usd_ = monthly_cost;
    cached_day_ = day;
    cached_year_ = year;
    cached_month_ = month;
}

inline void CostStorage::ensure_period_cache_current() {
    auto now = DateParts::now();

    if (now != cached_day_ || now.year != cached_year_ || now.month != cached_month_) {
        rebuild_aggregates(now, now.year, now.month);
    }
}

}
