// Atlas Scientific EZO command/state machine, vendored for ezo_types.
//
// Derived from ESPHome's built-in `ezo` component. Vendored so the state
// machine can be fixed rather than worked around: the stock component's
// `case 254: return;` waits forever on a "still processing" reply, so a
// firmware-locked circuit (which answers 0xFE to every read) pins the front
// command permanently and the driver wedges. This copy adds a bounded
// per-command deadline, checks the I2C write result, counts consecutive
// failed reads, and publishes NaN once a circuit stops responding so
// downstream consumers see *unavailable* instead of a stale value.
//
// Per the ESPHome License, the C++ sources (.c/.cpp/.h/.hpp/.tcc/.ino) of
// this component are licensed under the GNU General Public License v3. See
// the LICENSE file in this directory for the full text.
//
// Copyright (c) 2019 ESPHome

#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include <deque>

namespace esphome {
namespace ezo_types {

enum EzoCommandType : uint8_t {
  EZO_READ = 0,
  EZO_LED,
  EZO_DEVICE_INFORMATION,
  EZO_SLOPE,
  EZO_CALIBRATION,
  EZO_SLEEP,
  EZO_I2C,
  EZO_T,
  EZO_CUSTOM
};

enum EzoCalibrationType : uint8_t { EZO_CAL_LOW = 0, EZO_CAL_MID = 1, EZO_CAL_HIGH = 2 };

class EzoCommand {
 public:
  std::string command;
  uint16_t delay_ms = 0;
  bool command_sent = false;
  EzoCommandType command_type;
};

/// Base driver for Atlas Scientific EZO circuits in I2C mode.
///
/// Command flow matches Atlas's own Ezo_I2c_lib: send the ASCII command, wait
/// the documented processing delay, read the response once. A short bounded
/// grace is allowed for a 254 ("still processing") reply, after which the
/// command is dropped — a busy or locked circuit costs skipped datapoints,
/// never a wedged queue.
class EZOSensorBase : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void loop() override;
  void update() override;

  // I2C
  void set_address(uint8_t address);

  // Device Information
  void get_device_information();
  template<typename F> void add_device_infomation_callback(F &&callback) {
    this->device_infomation_callback_.add(std::forward<F>(callback));
  }

  // Sleep
  void set_sleep();

  // R
  void get_state();

  // Slope
  void get_slope();
  template<typename F> void add_slope_callback(F &&callback) { this->slope_callback_.add(std::forward<F>(callback)); }

  // T
  void get_t();
  void set_t(float value);
  void set_tempcomp_value(float temp);  // For backwards compatibility
  template<typename F> void add_t_callback(F &&callback) { this->t_callback_.add(std::forward<F>(callback)); }

  // Calibration
  void get_calibration();
  void set_calibration_point_low(float value);
  void set_calibration_point_mid(float value);
  void set_calibration_point_high(float value);
  void set_calibration_generic(float value);
  void clear_calibration();
  template<typename F> void add_calibration_callback(F &&callback) {
    this->calibration_callback_.add(std::forward<F>(callback));
  }

  // LED
  void get_led_state();
  void set_led_state(bool on);
  template<typename F> void add_led_state_callback(F &&callback) { this->led_callback_.add(std::forward<F>(callback)); }

  // Custom
  void send_custom(const std::string &to_send);
  template<typename F> void add_custom_callback(F &&callback) { this->custom_callback_.add(std::forward<F>(callback)); }

 protected:
  // How long past a command's processing delay to keep accepting 254 ("still
  // processing") before dropping the command. Atlas's reference code fails a
  // cycle after a single attempt; this grace is a bounded superset of that.
  static constexpr uint32_t COMMAND_TIMEOUT_MS = 2000;
  // Consecutive failed READs before the sensor value is marked unavailable.
  static constexpr uint8_t MAX_READ_FAILURES = 3;

  std::deque<std::unique_ptr<EzoCommand>> commands_;
  int new_address_;

  void add_command_(const char *command, EzoCommandType command_type, uint16_t delay_ms = 300);

  void set_calibration_point_(EzoCalibrationType type, float value);

  /// Pop the front command, tracking READ success/failure for NaN publishing.
  void finish_command_(bool success);

  /// Handle the payload of a successful READ. Default: truncate at the first
  /// comma, parse a float, publish (stock ezo behavior). ECSensor overrides
  /// this to parse the full multi-output CSV.
  virtual void handle_read_payload_(const std::string &payload);

  /// Publish NaN after MAX_READ_FAILURES consecutive failed reads. Subclasses
  /// with sub-sensors (EC's TDS/SAL/SG) override to mark those too.
  virtual void publish_read_failure_();

  CallbackManager<void(std::string)> device_infomation_callback_{};
  CallbackManager<void(std::string)> calibration_callback_{};
  CallbackManager<void(std::string)> slope_callback_{};
  CallbackManager<void(std::string)> t_callback_{};
  CallbackManager<void(std::string)> custom_callback_{};
  CallbackManager<void(bool)> led_callback_{};

  uint32_t start_time_ = 0;
  uint8_t consecutive_read_failures_{0};
};

}  // namespace ezo_types
}  // namespace esphome
