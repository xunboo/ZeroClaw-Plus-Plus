#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace zeroclaw::hardware {

struct BoardInfo {
    uint16_t vid;
    uint16_t pid;
    const char* name;
    const char* architecture;
};

inline constexpr BoardInfo known_boards[] = {
    {0x0483, 0x374b, "nucleo-f401re", "ARM Cortex-M4"},
    {0x0483, 0x3748, "nucleo-f411re", "ARM Cortex-M4"},
    {0x2341, 0x0043, "arduino-uno", "AVR ATmega328P"},
    {0x2341, 0x0078, "arduino-uno", "Arduino Uno Q / ATmega328P"},
    {0x2341, 0x0042, "arduino-mega", "AVR ATmega2560"},
    {0x10c4, 0xea60, "cp2102", "USB-UART bridge"},
    {0x10c4, 0xea70, "cp2102n", "USB-UART bridge"},
    {0x1a86, 0x7523, "esp32", "ESP32 (CH340)"},
    {0x1a86, 0x55d4, "esp32", "ESP32 (CH340)"},
};

inline const BoardInfo* lookup_board(uint16_t vid, uint16_t pid) {
    for (const auto& board : known_boards) {
        if (board.vid == vid && board.pid == pid) {
            return &board;
        }
    }
    return nullptr;
}

inline std::vector<BoardInfo> get_known_boards() {
    return {std::begin(known_boards), std::end(known_boards)};
}

}
