#pragma once

#include <M5Unified.h>
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace m5cores3_power {

class M5CoreS3Power : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  // Battery sensors
  void set_battery_sensor(sensor::Sensor *battery_sensor) { battery_sensor_ = battery_sensor; }
  void set_battery_present_sensor(binary_sensor::BinarySensor *battery_present_sensor) {
    battery_present_sensor_ = battery_present_sensor;
  }
  void set_battery_charging_sensor(binary_sensor::BinarySensor *battery_charging_sensor) {
    battery_charging_sensor_ = battery_charging_sensor;
  }

  // Service to trigger diagnostic logging
  void log_axp2101_diagnostics();

 protected:
  sensor::Sensor *battery_sensor_{nullptr};
  binary_sensor::BinarySensor *battery_present_sensor_{nullptr};
  binary_sensor::BinarySensor *battery_charging_sensor_{nullptr};
  uint32_t last_battery_update_{0};
};

// Action class for diagnostic logging
template<typename... Ts> class LogAxp2101DiagnosticsAction : public Action<Ts...> {
 public:
  LogAxp2101DiagnosticsAction() = default;

  void set_component(M5CoreS3Power *component) { this->component_ = component; }

  void play(Ts... x) override {
    if (this->component_) {
      this->component_->log_axp2101_diagnostics();
    }
  }

 protected:
  M5CoreS3Power *component_{nullptr};
};

}  // namespace m5cores3_power
}  // namespace esphome