#pragma once

#include "discover.hpp"
#include "introspect.hpp"
#include "registry.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <iostream>

namespace zeroclaw::hardware {

enum class HardwareTransport {
    Native,
    Serial,
    Probe
};

struct HardwareConfig {
    bool enabled = false;
    HardwareTransport transport = HardwareTransport::Serial;
    std::optional<std::string> serial_port;
};

struct DiscoveredDevice {
    std::string name;
    std::optional<std::string> detail;
    std::optional<std::string> device_path;
    HardwareTransport transport;
};

inline std::vector<DiscoveredDevice> discover_hardware() {
    std::vector<DiscoveredDevice> result;
    
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    auto usb_devices = list_usb_devices();
    
    for (const auto& d : usb_devices) {
        DiscoveredDevice dev;
        if (d.board_name) {
            dev.name = *d.board_name;
        } else {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%04x:%04x", d.vid, d.pid);
            dev.name = buf;
        }
        dev.detail = d.product_string;
        dev.device_path = std::nullopt;
        
        if (d.architecture && *d.architecture == "native") {
            dev.transport = HardwareTransport::Native;
        } else {
            dev.transport = HardwareTransport::Serial;
        }
        
        result.push_back(std::move(dev));
    }
#endif
    
    return result;
}

inline size_t recommended_wizard_default(const std::vector<DiscoveredDevice>& devices) {
    if (devices.empty()) {
        return 3;
    }
    return 1;
}

inline HardwareConfig config_from_wizard_choice(size_t choice, const std::vector<DiscoveredDevice>& devices) {
    HardwareConfig config;
    
    switch (choice) {
        case 0:
            config.enabled = true;
            config.transport = HardwareTransport::Native;
            break;
        case 1: {
            config.enabled = true;
            config.transport = HardwareTransport::Serial;
            auto it = std::find_if(devices.begin(), devices.end(),
                [](const DiscoveredDevice& d) { return d.transport == HardwareTransport::Serial; });
            if (it != devices.end()) {
                config.serial_port = it->device_path;
            }
            break;
        }
        case 2:
            config.enabled = true;
            config.transport = HardwareTransport::Probe;
            break;
        default:
            break;
    }
    
    return config;
}

inline void run_discover() {
    auto devices = list_usb_devices();
    
    if (devices.empty()) {
        std::cout << "No USB devices found." << std::endl;
        std::cout << std::endl;
        std::cout << "Connect a board (e.g. Nucleo-F401RE) via USB and try again." << std::endl;
        return;
    }
    
    std::cout << "USB devices:" << std::endl;
    std::cout << std::endl;
    
    for (const auto& d : devices) {
        auto board = d.board_name.value_or("(unknown)");
        auto arch = d.architecture.value_or("-");
        auto product = d.product_string.value_or("-");
        std::printf("  %04x:%04x  %s  %s  %s\n", d.vid, d.pid, board.c_str(), arch.c_str(), product.c_str());
    }
    
    std::cout << std::endl;
    std::cout << "Known boards: nucleo-f401re, nucleo-f411re, arduino-uno, arduino-mega, cp2102" << std::endl;
}

inline void run_introspect(const std::string& path) {
    auto result = introspect_device(path);
    
    std::cout << "Device at " << result.path << ":" << std::endl;
    std::cout << std::endl;
    
    if (result.vid && result.pid) {
        std::printf("  VID:PID     %04x:%04x\n", *result.vid, *result.pid);
    } else {
        std::cout << "  VID:PID     (could not correlate with USB device)" << std::endl;
    }
    
    if (result.board_name) {
        std::cout << "  Board       " << *result.board_name << std::endl;
    }
    
    if (result.architecture) {
        std::cout << "  Architecture " << *result.architecture << std::endl;
    }
    
    std::cout << "  Memory map  " << result.memory_map_note << std::endl;
}

inline void run_info(const std::string& chip) {
    std::cout << "Chip info via USB requires the 'probe' feature." << std::endl;
    std::cout << std::endl;
    std::cout << "Build with: cargo build --features hardware,probe" << std::endl;
    std::cout << std::endl;
    std::cout << "Then run: zeroclaw hardware info --chip " << chip << std::endl;
    std::cout << std::endl;
    std::cout << "This uses probe-rs to attach to the Nucleo's ST-Link over USB" << std::endl;
    std::cout << "and read chip info (memory map, etc.) — no firmware on target needed." << std::endl;
}

}
