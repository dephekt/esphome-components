#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/ezo/ezo.h"
#include <string>
#include <vector>

namespace esphome {
namespace ezo_types {

// Forward declarations
class RTDSensor;
class CellConstantSelect;

class EZOSensor : public ezo::EZOSensor {
 public:
  void setup() override;
  void update() override;
  void loop() override;

  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_reset_reason_sensor(text_sensor::TextSensor *reset_reason_sensor) {
    reset_reason_sensor_ = reset_reason_sensor;
  }
  void set_firmware_version_sensor(text_sensor::TextSensor *firmware_version_sensor) {
    firmware_version_sensor_ = firmware_version_sensor;
  }
  void set_calibration_status_sensor(text_sensor::TextSensor *calibration_status_sensor) {
    calibration_status_sensor_ = calibration_status_sensor;
  }
  void set_power_control_switch(switch_::Switch *power_switch) { power_control_switch_ = power_switch; }
  void set_rtd_sensor(RTDSensor *rtd_sensor) { rtd_sensor_ = rtd_sensor; }
  void set_temp_compensation_switch(switch_::Switch *sw) { temp_compensation_switch_ = sw; }
  void set_calibration_mode_switch(switch_::Switch *sw) { calibration_mode_switch_ = sw; }

  // Diagnostic sensors
  void set_current_command_sensor(text_sensor::TextSensor *current_command_sensor) {
    current_command_sensor_ = current_command_sensor;
  }
  void set_next_command_sensor(text_sensor::TextSensor *next_command_sensor) {
    next_command_sensor_ = next_command_sensor;
  }
  void set_last_command_sensor(text_sensor::TextSensor *last_command_sensor) {
    last_command_sensor_ = last_command_sensor;
  }
  void set_queue_size_sensor(sensor::Sensor *queue_size_sensor) { queue_size_sensor_ = queue_size_sensor; }

  // Calibration methods
  void factory_reset();

  // Switch control methods
  void set_temp_compensation_enabled(bool enabled) { temp_compensation_enabled_ = enabled; }
  void set_calibration_mode_enabled(bool enabled) {
    calibration_mode_enabled_ = enabled;
    // Set update interval: 1s for calibration, 5s for normal operation
    this->set_update_interval(enabled ? 1000 : 5000);
  }

 protected:
  void dump_config_base_(const char *sensor_type);
  void handle_common_responses_(const std::string &response);
  void parse_common_status_response_(const std::string &response);
  virtual void parse_common_calibration_response_(const std::string &response);
  void parse_device_information_response_(const std::string &response);

  virtual void handle_custom_response_(const std::string &response) = 0;
  bool is_circuit_powered_() const;
  virtual void reinitialize();
  void request_calibration_status();
  void update_diagnostic_sensors_();
  void send_compensated_read_();

  sensor::Sensor *voltage_sensor_{nullptr};
  text_sensor::TextSensor *reset_reason_sensor_{nullptr};
  text_sensor::TextSensor *firmware_version_sensor_{nullptr};
  text_sensor::TextSensor *calibration_status_sensor_{nullptr};
  switch_::Switch *power_control_switch_{nullptr};
  RTDSensor *rtd_sensor_{nullptr};
  switch_::Switch *temp_compensation_switch_{nullptr};
  switch_::Switch *calibration_mode_switch_{nullptr};

  // State tracking
  bool last_calibration_mode_state_{false};

  // Diagnostic sensors
  text_sensor::TextSensor *current_command_sensor_{nullptr};
  text_sensor::TextSensor *next_command_sensor_{nullptr};
  text_sensor::TextSensor *last_command_sensor_{nullptr};
  sensor::Sensor *queue_size_sensor_{nullptr};

  // Switch control state
  bool temp_compensation_enabled_{false};
  bool calibration_mode_enabled_{false};
  bool last_power_state_{true};

  // Power-on delay to prevent failed I2C commands
  uint32_t power_on_time_{0};
  bool initialization_pending_{false};
  static constexpr uint32_t POWER_ON_DELAY_MS = 2000;

  // Diagnostic state caching to prevent spam
  std::string last_current_command_{""};
  std::string last_next_command_{""};
  std::string last_completed_command_{""};
  std::string last_last_command_{""};
  size_t last_queue_size_{SIZE_MAX};
};

class PHSensor : public EZOSensor {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void reinitialize() override;

  void set_temperature_compensation_sensor(sensor::Sensor *temperature_compensation_sensor) {
    temperature_compensation_sensor_ = temperature_compensation_sensor;
  }

  void set_acid_slope_quality_sensor(sensor::Sensor *acid_slope_quality_sensor) {
    acid_slope_quality_sensor_ = acid_slope_quality_sensor;
  }
  void set_alkaline_slope_quality_sensor(sensor::Sensor *alkaline_slope_quality_sensor) {
    alkaline_slope_quality_sensor_ = alkaline_slope_quality_sensor;
  }
  void set_asymmetry_potential_sensor(sensor::Sensor *asymmetry_potential_sensor) {
    asymmetry_potential_sensor_ = asymmetry_potential_sensor;
  }

 protected:
  void handle_custom_response_(const std::string &response) override;
  void parse_common_calibration_response_(const std::string &response) override;
  void parse_temperature_compensation_response_(const std::string &response);
  void parse_slope_response_(const std::string &response);

  sensor::Sensor *temperature_compensation_sensor_{nullptr};
  sensor::Sensor *acid_slope_quality_sensor_{nullptr};
  sensor::Sensor *alkaline_slope_quality_sensor_{nullptr};
  sensor::Sensor *asymmetry_potential_sensor_{nullptr};
};

class ECSensor : public EZOSensor {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  void loop() override;

  void add_cell_constant_callback(std::function<void(float)> &&callback) {
    cell_constant_callback_.add(std::move(callback));
  }
  void set_temperature_compensation_sensor(sensor::Sensor *temperature_compensation_sensor) {
    this->temperature_compensation_sensor_ = temperature_compensation_sensor;
  }
  void set_cell_constant_select(CellConstantSelect *cell_constant_select) {
    this->cell_constant_select_ = cell_constant_select;
  }
  void set_tds_sensor(sensor::Sensor *tds_sensor) { tds_sensor_ = tds_sensor; }
  void set_salinity_sensor(sensor::Sensor *salinity_sensor) { salinity_sensor_ = salinity_sensor; }
  void set_relative_density_sensor(sensor::Sensor *relative_density_sensor) {
    relative_density_sensor_ = relative_density_sensor;
  }
  void set_tds_conversion_factor(float factor);
  void set_cell_constant(const std::string &value);
  void request_cell_constant_query();
  // EC-specific calibration methods
  void set_calibration_point_dry();

 protected:
  void handle_custom_response_(const std::string &response) override;
  void parse_common_calibration_response_(const std::string &response) override;
  void parse_cell_constant_response_(const std::string &response);
  void parse_temperature_compensation_response_(const std::string &response);
  void parse_reading_csv_(const std::string &response);

  CallbackManager<void(float)> cell_constant_callback_{};
  sensor::Sensor *temperature_compensation_sensor_{nullptr};
  CellConstantSelect *cell_constant_select_{nullptr};
  sensor::Sensor *tds_sensor_{nullptr};
  sensor::Sensor *salinity_sensor_{nullptr};
  sensor::Sensor *relative_density_sensor_{nullptr};
};

class RTDSensor : public EZOSensor {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void add_temp_scale_callback(std::function<void(std::string)> &&callback) {
    temp_scale_callback_.add(std::move(callback));
  }
  void add_datalogger_callback(std::function<void(bool, int)> &&callback) {
    datalogger_callback_.add(std::move(callback));
  }
  void add_temperature_change_callback(std::function<void(float)> &&callback) {
    temperature_change_callback_.add(std::move(callback));
  }
  void set_datalogger(bool enabled, int interval);

 protected:
  void handle_custom_response_(const std::string &response) override;
  void parse_common_calibration_response_(const std::string &response) override;
  void parse_temp_scale_response_(const std::string &response);
  void parse_datalogger_response_(const std::string &response);
  void check_temperature_change_(float new_temp);

  CallbackManager<void(std::string)> temp_scale_callback_{};
  CallbackManager<void(bool, int)> datalogger_callback_{};
  CallbackManager<void(float)> temperature_change_callback_{};

 public:
  float last_known_temperature_{NAN};

 protected:
  static constexpr float TEMP_CHANGE_THRESHOLD = 0.01f;
};

class ORPSensor : public EZOSensor {
 public:
  void update() override;
  void dump_config() override;

  void add_extended_scale_callback(std::function<void(bool)> &&callback) {
    extended_scale_callback_.add(std::move(callback));
  }
  void set_extended_scale(bool enabled);

 protected:
  void handle_custom_response_(const std::string &response) override;
  void parse_common_calibration_response_(const std::string &response) override;
  void parse_extended_scale_response_(const std::string &response);

  CallbackManager<void(bool)> extended_scale_callback_{};
};

class TDSConversionFactorNumber : public number::Number, public Component {
 public:
  void set_ec_sensor(ECSensor *ec_sensor);

 protected:
  void control(float value) override;

  ECSensor *ec_sensor_{nullptr};
};

}  // namespace ezo_types
}  // namespace esphome
