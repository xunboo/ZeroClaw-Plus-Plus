#pragma once

#include "discover.hpp"
#include "registry.hpp"
#include <cstdint>
#include <optional>
#include <string>

namespace zeroclaw::hardware {

struct IntrospectResult {
    std::string path;
    std::optional<uint16_t> vid;
    std::optional<uint16_t> pid;
    std::optional<std::string> board_name;
    std::optional<std::string> architecture;
    std::string memory_map_note;
};

inline std::string memory_map_for_board(const std::optional<std::string>& board_name) {
    return "Build with --features probe for live memory map via USB";
}

inline IntrospectResult introspect_device(const std::string& path) {
    auto devices = list_usb_devices();
    
    std::optional<UsbDeviceInfo> matched;
    
    if (devices.size() == 1) {
        matched = devices[0];
    } else if (devices.empty()) {
        matched = std::nullopt;
    } else {
        auto it = std::find_if(devices.begin(), devices.end(),
            [](const UsbDeviceInfo& d) { return d.board_name.has_value(); });
        if (it != devices.end()) {
            matched = *it;
        } else {
            matched = devices[0];
        }
    }
    
    std::optional<uint16_t> vid;
    std::optional<uint16_t> pid;
    std::optional<std::string> board_name;
    std::optional<std::string> architecture;
    
    if (matched) {
        vid = matched->vid;
        pid = matched->pid;
        board_name = matched->board_name;
        architecture = matched->architecture;
    }
    
    const BoardInfo* board_info = nullptr;
    if (vid && pid) {
        board_info = lookup_board(*vid, *pid);
    }
    
    if (!architecture && board_info) {
        architecture = std::string(board_info->architecture);
    }
    if (!board_name && board_info) {
        board_name = std::string(board_info->name);
    }
    
    return IntrospectResult{
        path,
        vid,
        pid,
        board_name,
        architecture,
        memory_map_for_board(board_name)
    };
}

}
