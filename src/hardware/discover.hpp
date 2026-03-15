#pragma once

#include "registry.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace zeroclaw::hardware {

struct UsbDeviceInfo {
    std::string bus_id;
    uint8_t device_address;
    uint16_t vid;
    uint16_t pid;
    std::optional<std::string> product_string;
    std::optional<std::string> board_name;
    std::optional<std::string> architecture;
};

#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)

inline std::vector<UsbDeviceInfo> list_usb_devices() {
    std::vector<UsbDeviceInfo> devices;
    return devices;
}

#else

inline std::vector<UsbDeviceInfo> list_usb_devices() {
    return {};
}

#endif

}
