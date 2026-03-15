#pragma once

#include <string>
#include <variant>
#include <optional>

namespace zeroclaw {
namespace util {

std::string truncate_with_ellipsis(const std::string& s, size_t max_chars);

template<typename T>
class MaybeSet {
public:
    enum class State { Set, Unset, Null };
    
private:
    State state_;
    std::optional<T> value_;

public:
    MaybeSet() : state_(State::Unset), value_(std::nullopt) {}
    
    explicit MaybeSet(T val) : state_(State::Set), value_(std::move(val)) {}
    
    static MaybeSet<T> null() {
        MaybeSet<T> m;
        m.state_ = State::Null;
        return m;
    }
    
    static MaybeSet<T> unset() {
        return MaybeSet<T>();
    }
    
    bool is_set() const { return state_ == State::Set; }
    bool is_unset() const { return state_ == State::Unset; }
    bool is_null() const { return state_ == State::Null; }
    
    const T& value() const { return value_.value(); }
    const T& value_or(const T& default_val) const {
        return is_set() ? value_.value() : default_val;
    }
    
    State state() const { return state_; }
};

}
}
