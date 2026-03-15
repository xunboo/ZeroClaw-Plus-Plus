#pragma once

#include <string>
#include <variant>
#include <chrono>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

namespace zeroclaw::hooks {
template<typename T>
class HookResult {
public:
    using ContinueType = T;
    using CancelType = std::string;

    static HookResult<T> Continue(T value) {
        return HookResult<T>(std::move(value));
    }

    static HookResult<T> Cancel(std::string reason) {
        return HookResult<T>(std::move(reason), true);
    }

    bool is_cancel() const {
        return std::holds_alternative<CancelType>(data_);
    }

    bool is_continue() const {
        return std::holds_alternative<ContinueType>(data_);
    }

    const T& value() const& {
        return std::get<ContinueType>(data_);
    }

    T&& value() && {
        return std::get<ContinueType>(std::move(data_));
    }

    const std::string& cancel_reason() const& {
        return std::get<CancelType>(data_);
    }

private:
    explicit HookResult(T value) : data_(std::move(value)) {}
    HookResult(std::string reason, bool) : data_(std::move(reason)) {}

    std::variant<ContinueType, CancelType> data_;
};
}
