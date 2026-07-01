#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/thermal_camera_core/thermal_camera_core.h"
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"
#include <cmath>

namespace esphome {
namespace mlx90640 {

// Bare Melexis MLX90640 sensor. Device I/O, calibration, and the two
// thermography-specific parameters (emissivity, reflected temperature) live
// here; everything else (palettes, ROI, web JPEG, common controls) is
// inherited from ThermalCameraBase.
class MLX90640Component : public thermal_camera_core::ThermalCameraBase {
 public:
  // The bare sensor's usable range is narrower than the base default (300);
  // keep the historical -40..85°C validity band.
  MLX90640Component() { this->temp_valid_max_ = 85.0f; }

  // Configuration setters
  void set_refresh_rate(const std::string &rate) { refresh_rate_ = rate; }
  void set_resolution(const std::string &resolution) { resolution_ = resolution; }
  void set_pattern(const std::string &pattern) { pattern_ = pattern; }
  void set_single_frame(bool single_frame) { single_frame_ = single_frame; }

  // Thermography parameter setters (static config)
  void set_emissivity(float e) { emissivity_ = e; }
  void set_ta_shift(float s) { ta_shift_ = s; }
  void set_reflected_temperature(float t) {
    reflected_temperature_ = t;
    reflected_temperature_auto_ = false;
  }

  // Thermography parameter runtime update methods (used by controls)
  void update_emissivity(float e) { emissivity_ = e; }
  void update_reflected_temperature(float t) { reflected_temperature_ = t; }
  void update_reflected_temperature_auto(bool a) { reflected_temperature_auto_ = a; }

  // Auto-generated control entity setters (device-specific controls)
  void set_emissivity_control(number::Number *control) { emissivity_control_ = control; }
  void set_reflected_temperature_control(number::Number *control) { reflected_temperature_control_ = control; }
  void set_reflected_temperature_auto_control(switch_::Switch *control) {
    reflected_temperature_auto_control_ = control;
  }

  // Extends the base ROI/update_interval sync with the emissivity and
  // reflected-temperature number controls (ThermalNumber::setup() restores the
  // value but never pushes it into the parent).
  void sync_roi_state_from_controls() override;

 protected:
  // Virtual seams from ThermalCameraBase
  bool read_frame_() override;
  bool init_device_() override;
  const char *display_name() const override { return "MLX90640"; }
  void dump_device_config_() override;
  void on_extra_number_control(int type, float value) override;
  void on_extra_switch_control(int type, bool value) override;

  // Configuration helpers
  uint16_t parse_refresh_rate_(const std::string &rate_str);
  int parse_resolution_(const std::string &res_str);
  int setup_thermal_resolution_(int bits);
  int setup_thermal_pattern_(const std::string &pattern);

  // Configuration
  std::string refresh_rate_{"16Hz"};
  std::string resolution_{"18-bit"};
  std::string pattern_{"chess"};
  bool single_frame_{false};

  // Thermography parameters
  float emissivity_{0.95f};
  float reflected_temperature_{NAN};       // manual reflected-temperature value
  bool reflected_temperature_auto_{true};  // true → tr = Ta − ta_shift_
  float ta_shift_{8.0f};

  // Hardware state
  paramsMLX90640 mlx90640_params_;
  uint16_t mlx90640Frame_[834];  // MLX90640 frame buffer (class member to prevent stack overflow)

  // Auto-generated control entities (optional, device-specific)
  number::Number *emissivity_control_{nullptr};
  number::Number *reflected_temperature_control_{nullptr};
  switch_::Switch *reflected_temperature_auto_control_{nullptr};
};

// Device-specific control types, dispatched to via
// ThermalCameraBase::on_extra_number_control()/on_extra_switch_control().
enum MLX90640ControlType {
  EMISSIVITY = thermal_camera_core::THERMAL_CONTROL_TYPE_EXTRA_START,
  REFLECTED_TEMPERATURE,
  REFLECTED_TEMPERATURE_AUTO,
};

}  // namespace mlx90640
}  // namespace esphome
