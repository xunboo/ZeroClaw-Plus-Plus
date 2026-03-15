#pragma once

#include <chrono>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>

#ifdef USE_JSON
#include <nlohmann/json.hpp>
#endif

namespace zeroclaw::cost {

enum class UsagePeriod {
    Session,
    Day,
    Month
};

struct TokenUsage {
    std::string model;
    std::uint64_t input_tokens;
    std::uint64_t output_tokens;
    std::uint64_t total_tokens;
    double cost_usd;
    std::chrono::system_clock::time_point timestamp;

    TokenUsage() = default;

    TokenUsage(std::string model_name,
               std::uint64_t input,
               std::uint64_t output,
               double input_price_per_million,
               double output_price_per_million)
        : model(std::move(model_name))
        , input_tokens(input)
        , output_tokens(output)
        , timestamp(std::chrono::system_clock::now())
    {
        double sanitized_input_price = sanitize_price(input_price_per_million);
        double sanitized_output_price = sanitize_price(output_price_per_million);
        total_tokens = saturating_add(input_tokens, output_tokens);
        double input_cost = (static_cast<double>(input_tokens) / 1'000'000.0) * sanitized_input_price;
        double output_cost = (static_cast<double>(output_tokens) / 1'000'000.0) * sanitized_output_price;
        cost_usd = input_cost + output_cost;
    }

    double cost() const { return cost_usd; }

#ifdef USE_JSON
    nlohmann::json to_json() const {
        return nlohmann::json{
            {"model", model},
            {"input_tokens", input_tokens},
            {"output_tokens", output_tokens},
            {"total_tokens", total_tokens},
            {"cost_usd", cost_usd},
            {"timestamp", std::chrono::duration_cast<std::chrono::seconds>(timestamp.time_since_epoch()).count()}
        };
    }

    static TokenUsage from_json(const nlohmann::json& j) {
        TokenUsage usage;
        usage.model = j.at("model").get<std::string>();
        usage.input_tokens = j.at("input_tokens").get<std::uint64_t>();
        usage.output_tokens = j.at("output_tokens").get<std::uint64_t>();
        usage.total_tokens = j.at("total_tokens").get<std::uint64_t>();
        usage.cost_usd = j.at("cost_usd").get<double>();
        auto ts = j.at("timestamp").get<std::int64_t>();
        usage.timestamp = std::chrono::system_clock::time_point(std::chrono::seconds(ts));
        return usage;
    }
#endif

private:
    static double sanitize_price(double value) {
        return (std::isfinite(value) && value > 0.0) ? value : 0.0;
    }

    static std::uint64_t saturating_add(std::uint64_t a, std::uint64_t b) {
        std::uint64_t result = a + b;
        return (result < a) ? std::numeric_limits<std::uint64_t>::max() : result;
    }
};

struct CostRecord {
    std::string id;
    TokenUsage usage;
    std::string session_id;

    CostRecord() = default;

    CostRecord(std::string sid, TokenUsage u)
        : usage(std::move(u))
        , session_id(std::move(sid))
    {
        id = generate_uuid();
    }

#ifdef USE_JSON
    nlohmann::json to_json() const {
        return nlohmann::json{
            {"id", id},
            {"usage", usage.to_json()},
            {"session_id", session_id}
        };
    }

    static CostRecord from_json(const nlohmann::json& j) {
        CostRecord record;
        record.id = j.at("id").get<std::string>();
        record.usage = TokenUsage::from_json(j.at("usage"));
        record.session_id = j.at("session_id").get<std::string>();
        return record;
    }
#endif

private:
    static std::string generate_uuid() {
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
};

struct BudgetCheck {
    enum class Status { Allowed, Warning, Exceeded };

    Status status = Status::Allowed;
    double current_usd = 0.0;
    double limit_usd = 0.0;
    UsagePeriod period = UsagePeriod::Session;

    static BudgetCheck allowed() {
        return BudgetCheck{Status::Allowed, 0.0, 0.0, UsagePeriod::Session};
    }

    static BudgetCheck warning(double current, double limit, UsagePeriod p) {
        return BudgetCheck{Status::Warning, current, limit, p};
    }

    static BudgetCheck exceeded(double current, double limit, UsagePeriod p) {
        return BudgetCheck{Status::Exceeded, current, limit, p};
    }

    bool is_allowed() const { return status == Status::Allowed; }
    bool is_warning() const { return status == Status::Warning; }
    bool is_exceeded() const { return status == Status::Exceeded; }
};

struct ModelStats {
    std::string model;
    double cost_usd = 0.0;
    std::uint64_t total_tokens = 0;
    std::size_t request_count = 0;

#ifdef USE_JSON
    nlohmann::json to_json() const {
        return nlohmann::json{
            {"model", model},
            {"cost_usd", cost_usd},
            {"total_tokens", total_tokens},
            {"request_count", request_count}
        };
    }

    static ModelStats from_json(const nlohmann::json& j) {
        ModelStats stats;
        stats.model = j.at("model").get<std::string>();
        stats.cost_usd = j.at("cost_usd").get<double>();
        stats.total_tokens = j.at("total_tokens").get<std::uint64_t>();
        stats.request_count = j.at("request_count").get<std::size_t>();
        return stats;
    }
#endif
};

struct CostSummary {
    double session_cost_usd = 0.0;
    double daily_cost_usd = 0.0;
    double monthly_cost_usd = 0.0;
    std::uint64_t total_tokens = 0;
    std::size_t request_count = 0;
    std::unordered_map<std::string, ModelStats> by_model;

#ifdef USE_JSON
    nlohmann::json to_json() const {
        nlohmann::json models_json = nlohmann::json::object();
        for (const auto& [name, stats] : by_model) {
            models_json[name] = stats.to_json();
        }
        return nlohmann::json{
            {"session_cost_usd", session_cost_usd},
            {"daily_cost_usd", daily_cost_usd},
            {"monthly_cost_usd", monthly_cost_usd},
            {"total_tokens", total_tokens},
            {"request_count", request_count},
            {"by_model", models_json}
        };
    }
#endif
};

struct CostConfig {
    bool enabled = true;
    double daily_limit_usd = 100.0;
    double monthly_limit_usd = 1000.0;
    std::uint32_t warn_at_percent = 80;
};

}
