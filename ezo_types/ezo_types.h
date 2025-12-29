#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/ezo/ezo.h"
#include <string>
#include <vector>

namespace esphome {
namespace ezo_types {

static const char *const TAG = "ezo_types";

class PHSensor : public ezo::EZOSensor {
 public:
  void setup() override;
  void dump_config() override;

  // Additional sensors for pH circuit
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_reset_reason_sensor(text_sensor::TextSensor *reset_reason_sensor) {
    reset_reason_sensor_ = reset_reason_sensor;
  }

 protected:
  void handle_custom_response_(const std::string &response);
  void parse_status_response_(const std::string &response);
  void parse_calibration_response_(const std::string &response);

  sensor::Sensor *voltage_sensor_{nullptr};
  text_sensor::TextSensor *reset_reason_sensor_{nullptr};
};

class ECSensor : public ezo::EZOSensor {
 public:
  void setup() override;
  void dump_config() override;

  // Additional sensors for EC circuit
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_reset_reason_sensor(text_sensor::TextSensor *reset_reason_sensor) {
    reset_reason_sensor_ = reset_reason_sensor;
  }
  void set_tds_sensor(sensor::Sensor *tds_sensor) { tds_sensor_ = tds_sensor; }
  void set_salinity_sensor(sensor::Sensor *salinity_sensor) { salinity_sensor_ = salinity_sensor; }
  void set_relative_density_sensor(sensor::Sensor *relative_density_sensor) {
    relative_density_sensor_ = relative_density_sensor;
  }

  // Output parameter control
  void set_output_ec_enabled(bool enabled) { output_ec_enabled_ = enabled; }
  void set_output_tds_enabled(bool enabled) { output_tds_enabled_ = enabled; }
  void set_output_salinity_enabled(bool enabled) { output_salinity_enabled_ = enabled; }
  void set_output_relative_density_enabled(bool enabled) { output_rd_enabled_ = enabled; }

  // Cell constant callbacks
  void add_cell_constant_callback(std::function<void(float)> &&callback) {
    cell_constant_callback_.add(std::move(callback));
  }
  void add_output_params_callback(std::function<void(bool, bool, bool, bool)> &&callback) {
    output_params_callback_.add(std::move(callback));
  }

 protected:
  void handle_custom_response_(const std::string &response);
  void parse_status_response_(const std::string &response);
  void parse_calibration_response_(const std::string &response);
  void parse_cell_constant_response_(const std::string &response);
  void parse_output_params_response_(const std::string &response);
  void parse_multi_value_response_(const std::string &response);

  sensor::Sensor *voltage_sensor_{nullptr};
  text_sensor::TextSensor *reset_reason_sensor_{nullptr};
  sensor::Sensor *tds_sensor_{nullptr};
  sensor::Sensor *salinity_sensor_{nullptr};
  sensor::Sensor *relative_density_sensor_{nullptr};

  bool output_ec_enabled_{true};
  bool output_tds_enabled_{false};
  bool output_salinity_enabled_{false};
  bool output_rd_enabled_{false};

  CallbackManager<void(float)> cell_constant_callback_{};
  CallbackManager<void(bool, bool, bool, bool)> output_params_callback_{};
};

class RTDSensor : public ezo::EZOSensor {
 public:
  void setup() override;
  void dump_config() override;

  // Additional sensors for RTD circuit
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_reset_reason_sensor(text_sensor::TextSensor *reset_reason_sensor) {
    reset_reason_sensor_ = reset_reason_sensor;
  }

  // Temperature scale callbacks
  void add_temp_scale_callback(std::function<void(std::string)> &&callback) {
    temp_scale_callback_.add(std::move(callback));
  }
  void add_datalogger_callback(std::function<void(bool, int)> &&callback) {
    datalogger_callback_.add(std::move(callback));
  }

 protected:
  void handle_custom_response_(const std::string &response);
  void parse_status_response_(const std::string &response);
  void parse_calibration_response_(const std::string &response);
  void parse_temp_scale_response_(const std::string &response);
  void parse_datalogger_response_(const std::string &response);

  sensor::Sensor *voltage_sensor_{nullptr};
  text_sensor::TextSensor *reset_reason_sensor_{nullptr};

  CallbackManager<void(std::string)> temp_scale_callback_{};
  CallbackManager<void(bool, int)> datalogger_callback_{};
};

class ORPSensor : public ezo::EZOSensor {
 public:
  void setup() override;
  void dump_config() override;

  // Additional sensors for ORP circuit
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { voltage_sensor_ = voltage_sensor; }
  void set_reset_reason_sensor(text_sensor::TextSensor *reset_reason_sensor) {
    reset_reason_sensor_ = reset_reason_sensor;
  }

  // Extended scale callback
  void add_extended_scale_callback(std::function<void(bool)> &&callback) {
    extended_scale_callback_.add(std::move(callback));
  }

 protected:
  void handle_custom_response_(const std::string &response);
  void parse_status_response_(const std::string &response);
  void parse_calibration_response_(const std::string &response);
  void parse_extended_scale_response_(const std::string &response);

  sensor::Sensor *voltage_sensor_{nullptr};
  text_sensor::TextSensor *reset_reason_sensor_{nullptr};

  CallbackManager<void(bool)> extended_scale_callback_{};
};

}  // namespace ezo_types
}  // namespace esphome
