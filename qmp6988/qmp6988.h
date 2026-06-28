// QMP6988 barometric pressure sensor component for ESPHome.
//
// Derived from ESPHome's built-in `qmp6988` component, with hardened
// initialization, runtime self-heal, and an output plausibility guard added.
//
// Per the ESPHome License, the C++ sources (.c/.cpp/.h/.hpp/.tcc/.ino) of this
// component are licensed under the GNU General Public License v3. See the
// LICENSE file in this directory for the full text.
//
// Copyright (c) 2019 ESPHome

#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::qmp6988 {

/* oversampling */
enum QMP6988Oversampling : uint8_t {
  QMP6988_OVERSAMPLING_SKIPPED = 0x00,
  QMP6988_OVERSAMPLING_1X = 0x01,
  QMP6988_OVERSAMPLING_2X = 0x02,
  QMP6988_OVERSAMPLING_4X = 0x03,
  QMP6988_OVERSAMPLING_8X = 0x04,
  QMP6988_OVERSAMPLING_16X = 0x05,
  QMP6988_OVERSAMPLING_32X = 0x06,
  QMP6988_OVERSAMPLING_64X = 0x07,
};

/* filter */
enum QMP6988IIRFilter : uint8_t {
  QMP6988_IIR_FILTER_OFF = 0x00,
  QMP6988_IIR_FILTER_2X = 0x01,
  QMP6988_IIR_FILTER_4X = 0x02,
  QMP6988_IIR_FILTER_8X = 0x03,
  QMP6988_IIR_FILTER_16X = 0x04,
  QMP6988_IIR_FILTER_32X = 0x05,
};

using qmp6988_cali_data_t = struct Qmp6988CaliData {
  int32_t COE_a0;
  int16_t COE_a1;
  int16_t COE_a2;
  int32_t COE_b00;
  int16_t COE_bt1;
  int16_t COE_bt2;
  int16_t COE_bp1;
  int16_t COE_b11;
  int16_t COE_bp2;
  int16_t COE_b12;
  int16_t COE_b21;
  int16_t COE_bp3;
};

using qmp6988_ik_data_t = struct Qmp6988IkData {
  int32_t a0, b00;
  int32_t a1, a2;
  int64_t bt1, bt2, bp1, b11, bp2, b12, b21, bp3;
};

using qmp6988_data_t = struct Qmp6988Data {
  uint8_t chip_id;
  float temperature;
  float pressure;
  qmp6988_cali_data_t qmp6988_cali;
  qmp6988_ik_data_t ik;
};

class QMP6988Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
  void set_pressure_sensor(sensor::Sensor *pressure_sensor) { this->pressure_sensor_ = pressure_sensor; }

  void setup() override;
  void dump_config() override;
  void update() override;

  void set_iir_filter(QMP6988IIRFilter iirfilter) { this->iir_filter_ = iirfilter; }
  void set_temperature_oversampling(QMP6988Oversampling oversampling_t) {
    this->temperature_oversampling_ = oversampling_t;
  }
  void set_pressure_oversampling(QMP6988Oversampling oversampling_p) { this->pressure_oversampling_ = oversampling_p; }

 protected:
  qmp6988_data_t qmp6988_data_;
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *pressure_sensor_{nullptr};

  QMP6988Oversampling temperature_oversampling_{QMP6988_OVERSAMPLING_8X};
  QMP6988Oversampling pressure_oversampling_{QMP6988_OVERSAMPLING_8X};
  QMP6988IIRFilter iir_filter_{QMP6988_IIR_FILTER_OFF};

  bool recovering_{false};           // a recovery sequence is in flight
  bool recovery_calibrated_{false};  // calibration result, carried to finish_recovery_
  uint8_t configure_attempts_{0};    // CTRLMEAS retry counter

  bool get_calibration_data_();
  bool device_check_();
  // Non-blocking (re)initialization: software reset, reload calibration, and program
  // CTRLMEAS, sequenced over the scheduler so no step blocks the main loop. Used by
  // setup() and update()'s self-heal. No-op while a sequence is already in flight.
  void begin_recovery_();
  void recovery_clear_reset_();
  void recovery_configure_();
  void recovery_write_ctrlmeas_();
  void recovery_verify_ctrlmeas_();
  void finish_recovery_(bool configured);
  uint8_t target_ctrlmeas_() const;
  // Physical-plausibility gate used to reject garbage from a non-converting sensor.
  static bool values_plausible_(float temperature_c, float pressure_hpa);
  // Returns false on an I2C read error; on success the compensated values are stored.
  bool calculate_pressure_();

  int32_t get_compensated_pressure_(qmp6988_ik_data_t *ik, int32_t dp, int16_t tx);
  int16_t get_compensated_temperature_(qmp6988_ik_data_t *ik, int32_t dt);
};

}  // namespace esphome::qmp6988
