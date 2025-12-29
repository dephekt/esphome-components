#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/template/number/template_number.h"

namespace esphome {
namespace scd4x_alerts {

class SCD4xAlerts : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_co2_sensor(sensor::Sensor *co2_sensor) { co2_sensor_ = co2_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }
  void set_vpd_sensor(sensor::Sensor *vpd_sensor) { vpd_sensor_ = vpd_sensor; }

  // Threshold setters
  void set_co2_high_threshold(float threshold) { co2_high_threshold_ = threshold; }
  void set_co2_low_threshold(float threshold) { co2_low_threshold_ = threshold; }
  void set_temp_high_threshold(float threshold) { temp_high_threshold_ = threshold; }
  void set_temp_low_threshold(float threshold) { temp_low_threshold_ = threshold; }
  void set_humidity_high_threshold(float threshold) { humidity_high_threshold_ = threshold; }
  void set_humidity_low_threshold(float threshold) { humidity_low_threshold_ = threshold; }
  void set_vpd_high_threshold(float threshold) { vpd_high_threshold_ = threshold; }
  void set_vpd_low_threshold(float threshold) { vpd_low_threshold_ = threshold; }

  // Alert sensor setters
  void set_co2_high_alert_sensor(binary_sensor::BinarySensor *sensor) { co2_high_alert_sensor_ = sensor; }
  void set_co2_low_alert_sensor(binary_sensor::BinarySensor *sensor) { co2_low_alert_sensor_ = sensor; }
  void set_temp_high_alert_sensor(binary_sensor::BinarySensor *sensor) { temp_high_alert_sensor_ = sensor; }
  void set_temp_low_alert_sensor(binary_sensor::BinarySensor *sensor) { temp_low_alert_sensor_ = sensor; }
  void set_humidity_high_alert_sensor(binary_sensor::BinarySensor *sensor) { humidity_high_alert_sensor_ = sensor; }
  void set_humidity_low_alert_sensor(binary_sensor::BinarySensor *sensor) { humidity_low_alert_sensor_ = sensor; }
  void set_vpd_high_alert_sensor(binary_sensor::BinarySensor *sensor) { vpd_high_alert_sensor_ = sensor; }
  void set_vpd_low_alert_sensor(binary_sensor::BinarySensor *sensor) { vpd_low_alert_sensor_ = sensor; }

  // Number entity setters
  void set_co2_high_threshold_number(template_::TemplateNumber *number) { co2_high_threshold_number_ = number; }
  void set_co2_low_threshold_number(template_::TemplateNumber *number) { co2_low_threshold_number_ = number; }
  void set_temp_high_threshold_number(template_::TemplateNumber *number) { temp_high_threshold_number_ = number; }
  void set_temp_low_threshold_number(template_::TemplateNumber *number) { temp_low_threshold_number_ = number; }
  void set_humidity_high_threshold_number(template_::TemplateNumber *number) {
    humidity_high_threshold_number_ = number;
  }
  void set_humidity_low_threshold_number(template_::TemplateNumber *number) { humidity_low_threshold_number_ = number; }
  void set_vpd_high_threshold_number(template_::TemplateNumber *number) { vpd_high_threshold_number_ = number; }
  void set_vpd_low_threshold_number(template_::TemplateNumber *number) { vpd_low_threshold_number_ = number; }

 protected:
  void update_alerts_();
  void update_co2_alerts_();
  void update_temperature_alerts_();
  void update_humidity_alerts_();
  void update_vpd_alerts_();

  bool check_alert_with_delay_(bool condition, bool &last_state, uint32_t &last_change_time, uint32_t delay_ms);

  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *vpd_sensor_{nullptr};

  // Threshold values
  float co2_high_threshold_{1500.0f};
  float co2_low_threshold_{800.0f};
  float temp_high_threshold_{30.0f};
  float temp_low_threshold_{18.0f};
  float humidity_high_threshold_{70.0f};
  float humidity_low_threshold_{40.0f};
  float vpd_high_threshold_{1.5f};
  float vpd_low_threshold_{0.4f};

  // Alert binary sensors
  binary_sensor::BinarySensor *co2_high_alert_sensor_{nullptr};
  binary_sensor::BinarySensor *co2_low_alert_sensor_{nullptr};
  binary_sensor::BinarySensor *temp_high_alert_sensor_{nullptr};
  binary_sensor::BinarySensor *temp_low_alert_sensor_{nullptr};
  binary_sensor::BinarySensor *humidity_high_alert_sensor_{nullptr};
  binary_sensor::BinarySensor *humidity_low_alert_sensor_{nullptr};
  binary_sensor::BinarySensor *vpd_high_alert_sensor_{nullptr};
  binary_sensor::BinarySensor *vpd_low_alert_sensor_{nullptr};

  // Threshold number entities
  template_::TemplateNumber *co2_high_threshold_number_{nullptr};
  template_::TemplateNumber *co2_low_threshold_number_{nullptr};
  template_::TemplateNumber *temp_high_threshold_number_{nullptr};
  template_::TemplateNumber *temp_low_threshold_number_{nullptr};
  template_::TemplateNumber *humidity_high_threshold_number_{nullptr};
  template_::TemplateNumber *humidity_low_threshold_number_{nullptr};
  template_::TemplateNumber *vpd_high_threshold_number_{nullptr};
  template_::TemplateNumber *vpd_low_threshold_number_{nullptr};

  // Alert state tracking for delays
  bool co2_high_last_state_{false};
  bool co2_low_last_state_{false};
  bool temp_high_last_state_{false};
  bool temp_low_last_state_{false};
  bool humidity_high_last_state_{false};
  bool humidity_low_last_state_{false};
  bool vpd_high_last_state_{false};
  bool vpd_low_last_state_{false};

  // Alert timing for delays
  uint32_t co2_high_last_change_{0};
  uint32_t co2_low_last_change_{0};
  uint32_t temp_high_last_change_{0};
  uint32_t temp_low_last_change_{0};
  uint32_t humidity_high_last_change_{0};
  uint32_t humidity_low_last_change_{0};
  uint32_t vpd_high_last_change_{0};
  uint32_t vpd_low_last_change_{0};

  // Update timing
  uint32_t last_update_time_{0};
  static const uint32_t UPDATE_INTERVAL = 10000;  // 10 seconds

  // Alert delay constants
  static const uint32_t CO2_ALERT_DELAY = 30000;       // 30 seconds
  static const uint32_t TEMP_ALERT_DELAY = 60000;      // 1 minute
  static const uint32_t HUMIDITY_ALERT_DELAY = 60000;  // 1 minute
  static const uint32_t VPD_ALERT_DELAY = 60000;       // 1 minute
};

}  // namespace scd4x_alerts
}  // namespace esphome