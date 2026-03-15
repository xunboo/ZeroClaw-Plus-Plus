#pragma once

#include "traits.hpp"

namespace zeroclaw::memory {

class NoneMemory : public Memory {
public:
    NoneMemory() = default;
    
    std::string name() const override {
        return "none";
    }
    
    void store(const std::string&,
               const std::string&,
               const MemoryCategory&,
               const std::optional<std::string>& = std::nullopt) override {
    }
    
    std::vector<MemoryEntry> recall(const std::string&,
                                    size_t,
                                    const std::optional<std::string>& = std::nullopt) override {
        return {};
    }
    
    std::optional<MemoryEntry> get(const std::string&) override {
        return std::nullopt;
    }
    
    std::vector<MemoryEntry> list(const std::optional<MemoryCategory>& = std::nullopt,
                                  const std::optional<std::string>& = std::nullopt) override {
        return {};
    }
    
    bool forget(const std::string&) override {
        return false;
    }
    
    size_t count() override {
        return 0;
    }
    
    bool health_check() override {
        return true;
    }
};

}
