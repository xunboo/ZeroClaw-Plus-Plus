#pragma once

/// Peripherals module — hardware peripheral management for embedded targets.
/// Supports Arduino, STM32 Nucleo, Raspberry Pi, and generic serial peripherals.

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include "nlohmann/json.hpp"

namespace zeroclaw {
namespace peripherals {

// ── Peripheral Traits ────────────────────────────────────────────

/// Peripheral connection state
enum class PeripheralState {
    Disconnected,
    Connected,
    Busy,
    Error
};

/// Abstract peripheral interface
class Peripheral {
public:
    virtual ~Peripheral() = default;
    virtual std::string name() const = 0;
    virtual std::string device_type() const = 0;
    virtual PeripheralState state() const = 0;
    virtual bool connect() = 0;
    virtual bool disconnect() = 0;
    virtual bool flash(const std::string& firmware_path) = 0;
    virtual std::optional<std::string> read_output() = 0;
    virtual bool send_command(const std::string& command) = 0;
};

/// Peripheral capability description
struct PeripheralCapability {
    std::string name;
    std::string description;
    nlohmann::json parameters;
};

// ── Serial Peripheral ────────────────────────────────────────────

/// Serial port configuration
struct SerialConfig {
    std::string port;
    uint32_t baud_rate = 115200;
    uint8_t data_bits = 8;
    uint8_t stop_bits = 1;
    std::string parity = "none";
    uint32_t timeout_ms = 1000;
};

/// Serial port peripheral
class SerialPeripheral : public Peripheral {
public:
    explicit SerialPeripheral(const SerialConfig& config);
    std::string name() const override { return "serial:" + config_.port; }
    std::string device_type() const override { return "serial"; }
    PeripheralState state() const override { return state_; }
    bool connect() override;
    bool disconnect() override;
    bool flash(const std::string& firmware_path) override;
    std::optional<std::string> read_output() override;
    bool send_command(const std::string& command) override;

private:
    SerialConfig config_;
    PeripheralState state_ = PeripheralState::Disconnected;
};

// ── Arduino ──────────────────────────────────────────────────────

/// Arduino board type
enum class ArduinoBoardType {
    Uno,
    Mega,
    Nano,
    Due,
    MKR,
    ESP32,
    Other
};

/// Arduino peripheral — flash and communicate with Arduino boards
class ArduinoPeripheral : public Peripheral {
public:
    ArduinoPeripheral(ArduinoBoardType board_type, const std::string& port);
    std::string name() const override { return "arduino:" + port_; }
    std::string device_type() const override { return "arduino"; }
    PeripheralState state() const override { return state_; }
    bool connect() override;
    bool disconnect() override;
    bool flash(const std::string& firmware_path) override;
    std::optional<std::string> read_output() override;
    bool send_command(const std::string& command) override;

    /// Compile a sketch before flashing
    bool compile(const std::string& sketch_path, const std::string& output_dir);

private:
    ArduinoBoardType board_type_;
    std::string port_;
    PeripheralState state_ = PeripheralState::Disconnected;
};

// ── STM32 Nucleo ─────────────────────────────────────────────────

/// STM32 Nucleo board peripheral
class NucleoPeripheral : public Peripheral {
public:
    NucleoPeripheral(const std::string& board_name, const std::string& port);
    std::string name() const override { return "nucleo:" + board_name_; }
    std::string device_type() const override { return "nucleo"; }
    PeripheralState state() const override { return state_; }
    bool connect() override;
    bool disconnect() override;
    bool flash(const std::string& firmware_path) override;
    std::optional<std::string> read_output() override;
    bool send_command(const std::string& command) override;

private:
    std::string board_name_;
    std::string port_;
    PeripheralState state_ = PeripheralState::Disconnected;
};

// ── Raspberry Pi ─────────────────────────────────────────────────

/// Raspberry Pi peripheral — GPIO, SSH, and file transfer
class RpiPeripheral : public Peripheral {
public:
    RpiPeripheral(const std::string& hostname, const std::string& user,
                  const std::optional<std::string>& key_path = std::nullopt);
    std::string name() const override { return "rpi:" + hostname_; }
    std::string device_type() const override { return "rpi"; }
    PeripheralState state() const override { return state_; }
    bool connect() override;
    bool disconnect() override;
    bool flash(const std::string& firmware_path) override;
    std::optional<std::string> read_output() override;
    bool send_command(const std::string& command) override;

    /// Execute a remote command via SSH
    std::optional<std::string> ssh_exec(const std::string& command);
    /// Transfer a file via SCP
    bool scp_upload(const std::string& local_path, const std::string& remote_path);

private:
    std::string hostname_;
    std::string user_;
    std::optional<std::string> key_path_;
    PeripheralState state_ = PeripheralState::Disconnected;
};

// ── UNO-Q (custom bridge board) ──────────────────────────────────

/// UNO-Q bridge peripheral for custom hardware
class UnoQBridge : public Peripheral {
public:
    explicit UnoQBridge(const std::string& port);
    std::string name() const override { return "uno_q:" + port_; }
    std::string device_type() const override { return "uno_q"; }
    PeripheralState state() const override { return state_; }
    bool connect() override;
    bool disconnect() override;
    bool flash(const std::string& firmware_path) override;
    std::optional<std::string> read_output() override;
    bool send_command(const std::string& command) override;
    bool setup();

private:
    std::string port_;
    PeripheralState state_ = PeripheralState::Disconnected;
};

// ── Capabilities Tool ────────────────────────────────────────────

/// List capabilities of connected peripherals
std::vector<PeripheralCapability> list_capabilities(const std::vector<std::shared_ptr<Peripheral>>& peripherals);

/// Detect available peripherals by scanning serial ports
std::vector<std::shared_ptr<Peripheral>> detect_peripherals();

} // namespace peripherals
} // namespace zeroclaw
