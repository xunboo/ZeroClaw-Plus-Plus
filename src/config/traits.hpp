#pragma once

#include <string>

namespace zeroclaw {
namespace config {

class ChannelConfig {
public:
    static const char* name();
    static const char* desc();
};

class ConfigHandle {
public:
    virtual const char* name() const = 0;
    virtual const char* desc() const = 0;
    virtual ~ConfigHandle() = default;
};

}
}
