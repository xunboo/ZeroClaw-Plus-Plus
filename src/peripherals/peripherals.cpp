#include "peripherals.hpp"

namespace zeroclaw {
namespace peripherals {

// ── SerialPeripheral ─────────────────────────────────────────────

SerialPeripheral::SerialPeripheral(const SerialConfig& config) : config_(config) {}
bool SerialPeripheral::connect() { state_ = PeripheralState::Connected; return true; }
bool SerialPeripheral::disconnect() { state_ = PeripheralState::Disconnected; return true; }
bool SerialPeripheral::flash(const std::string&) { return false; }
std::optional<std::string> SerialPeripheral::read_output() { return std::nullopt; }
bool SerialPeripheral::send_command(const std::string&) { return state_ == PeripheralState::Connected; }

// ── ArduinoPeripheral ────────────────────────────────────────────

ArduinoPeripheral::ArduinoPeripheral(ArduinoBoardType board_type, const std::string& port)
    : board_type_(board_type), port_(port) {}
bool ArduinoPeripheral::connect() { state_ = PeripheralState::Connected; return true; }
bool ArduinoPeripheral::disconnect() { state_ = PeripheralState::Disconnected; return true; }
bool ArduinoPeripheral::flash(const std::string&) { return state_ == PeripheralState::Connected; }
std::optional<std::string> ArduinoPeripheral::read_output() { return std::nullopt; }
bool ArduinoPeripheral::send_command(const std::string&) { return state_ == PeripheralState::Connected; }
bool ArduinoPeripheral::compile(const std::string&, const std::string&) { return true; }

// ── NucleoPeripheral ─────────────────────────────────────────────

NucleoPeripheral::NucleoPeripheral(const std::string& board_name, const std::string& port)
    : board_name_(board_name), port_(port) {}
bool NucleoPeripheral::connect() { state_ = PeripheralState::Connected; return true; }
bool NucleoPeripheral::disconnect() { state_ = PeripheralState::Disconnected; return true; }
bool NucleoPeripheral::flash(const std::string&) { return state_ == PeripheralState::Connected; }
std::optional<std::string> NucleoPeripheral::read_output() { return std::nullopt; }
bool NucleoPeripheral::send_command(const std::string&) { return state_ == PeripheralState::Connected; }

// ── RpiPeripheral ────────────────────────────────────────────────

RpiPeripheral::RpiPeripheral(const std::string& hostname, const std::string& user,
                              const std::optional<std::string>& key_path)
    : hostname_(hostname), user_(user), key_path_(key_path) {}
bool RpiPeripheral::connect() { state_ = PeripheralState::Connected; return true; }
bool RpiPeripheral::disconnect() { state_ = PeripheralState::Disconnected; return true; }
bool RpiPeripheral::flash(const std::string&) { return false; }
std::optional<std::string> RpiPeripheral::read_output() { return std::nullopt; }
bool RpiPeripheral::send_command(const std::string& cmd) { return ssh_exec(cmd).has_value(); }
std::optional<std::string> RpiPeripheral::ssh_exec(const std::string&) { return std::nullopt; }
bool RpiPeripheral::scp_upload(const std::string&, const std::string&) { return false; }

// ── UnoQBridge ───────────────────────────────────────────────────

UnoQBridge::UnoQBridge(const std::string& port) : port_(port) {}
bool UnoQBridge::connect() { state_ = PeripheralState::Connected; return true; }
bool UnoQBridge::disconnect() { state_ = PeripheralState::Disconnected; return true; }
bool UnoQBridge::flash(const std::string&) { return false; }
std::optional<std::string> UnoQBridge::read_output() { return std::nullopt; }
bool UnoQBridge::send_command(const std::string&) { return state_ == PeripheralState::Connected; }
bool UnoQBridge::setup() { return true; }

// ── Capabilities ─────────────────────────────────────────────────

std::vector<PeripheralCapability> list_capabilities(
    const std::vector<std::shared_ptr<Peripheral>>& peripherals) {
    std::vector<PeripheralCapability> caps;
    for (const auto& p : peripherals) {
        caps.push_back({p->name(), "Device: " + p->device_type(), {}});
    }
    return caps;
}

std::vector<std::shared_ptr<Peripheral>> detect_peripherals() {
    // Would scan /dev/ttyUSB*, /dev/ttyACM*, COM* for serial devices
    return {};
}

} // namespace peripherals
} // namespace zeroclaw
