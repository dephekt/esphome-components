#include "mlx90640.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace mlx90640 {

static const char *const TAG = "mlx90640";

bool MLX90640Component::init_device_() {
  ESP_LOGCONFIG(TAG, "Initializing MLX90640 thermal camera...");

  // Route the Melexis driver's I2C through this component's ESPHome bus
  // (no Arduino Wire). Must happen before the first MLX90640_* call.
  MLX90640_SetI2CBus(this->bus_);

  // Dump EEPROM parameters
  uint16_t eeMLX90640[832];
  int status = MLX90640_DumpEE(this->address_, eeMLX90640);
  if (status != 0) {
    ESP_LOGE(TAG, "Failed to dump MLX90640 EEPROM: %d", status);
    // Legacy behavior: log and let the rest of setup() (palette/ROI sync,
    // web server) proceed rather than marking the component failed.
    return true;
  }

  // Extract calibration parameters
  status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640_params_);
  if (status != 0) {
    ESP_LOGE(TAG, "Failed to extract MLX90640 parameters: %d", status);
    return true;
  }

  // Configure refresh rate from user setting
  uint16_t refresh_rate_code = parse_refresh_rate_(refresh_rate_);
  status = MLX90640_SetRefreshRate(this->address_, refresh_rate_code);
  if (status != 0) {
    ESP_LOGW(TAG, "Failed to set MLX90640 refresh rate: %d", status);
  } else {
    ESP_LOGCONFIG(TAG, "MLX90640 refresh rate set to %s", refresh_rate_.c_str());
  }

  // Configure resolution from user setting
  int resolution_bits = parse_resolution_(resolution_);
  status = setup_thermal_resolution_(resolution_bits);
  if (status != 0) {
    ESP_LOGW(TAG, "Failed to set MLX90640 resolution: %d", status);
  } else {
    ESP_LOGCONFIG(TAG, "MLX90640 resolution set to %s", resolution_.c_str());
  }

  // Configure pattern mode from user setting
  status = setup_thermal_pattern_(pattern_);
  if (status != 0) {
    ESP_LOGW(TAG, "Failed to set MLX90640 pattern mode: %d", status);
  } else {
    ESP_LOGCONFIG(TAG, "MLX90640 pattern mode set to %s", pattern_.c_str());
  }

  // Prime both checkerboard halves once, here in setup() where blocking is free,
  // so the first frame the loop publishes is complete rather than half zero-init.
  prime_frame_();

  initialized_ = true;
  ESP_LOGCONFIG(TAG, "MLX90640 thermal camera initialized successfully");
  return true;
}

// Reads one subpage into pixels_ IFF the sensor already has a fresh subpage
// waiting. Returns 1 = a subpage was read and integrated, 0 = no fresh data yet
// (the caller must not block), -1 = I2C/frame error. Polling the data-ready bit
// up front means MLX90640_GetFrameData's internal `while (dataReady == 0)` spin
// exits immediately, so this never blocks the loop waiting on a conversion.
int MLX90640Component::poll_subpage_() {
  uint16_t status = 0;
  if (MLX90640_I2CRead(this->address_, MLX90640_STATUS_REG, 1, &status) != 0)
    return -1;
  if (!MLX90640_GET_DATA_READY(status))
    return 0;  // no fresh subpage yet

  int subpage = MLX90640_GetFrameData(this->address_, mlx90640Frame_);
  if (subpage < 0) {
    ESP_LOGD(TAG, "MLX90640 GetFrame error: %d", subpage);
    return -1;
  }

  // Determine the reflected temperature for this frame.
  float ta = MLX90640_GetTa(mlx90640Frame_, &mlx90640_params_);
  float tr;
  if (reflected_temperature_auto_ || std::isnan(reflected_temperature_)) {
    tr = ta - ta_shift_;
  } else {
    tr = reflected_temperature_;
  }

  // CalculateTo writes only the current subpage's 384 pixels into pixels_; the
  // opposite checkerboard half carries over from a previous cycle. Display
  // interpolation happens lazily in generate_jpg_jpegenc_ (base).
  MLX90640_CalculateTo(mlx90640Frame_, &mlx90640_params_, emissivity_, tr, pixels_);

  // Fix bad pixels using neighboring-pixel interpolation (mode matches the
  // configured readout pattern — see setup_thermal_pattern_).
  MLX90640_BadPixelsCorrection(mlx90640_params_.brokenPixels, pixels_, bad_pixel_mode_, &mlx90640_params_);
  MLX90640_BadPixelsCorrection(mlx90640_params_.outlierPixels, pixels_, bad_pixel_mode_, &mlx90640_params_);

  subpages_seen_ |= (subpage == 0) ? 0x01 : 0x02;
  return 1;
}

// Blocking read of both subpages, called once from init_device_() where blocking
// is free. Guarantees the first frame the loop publishes is a complete 32x24
// image instead of one half of real data and one half of zero-init 0°C pixels
// (which would otherwise pass the validity filter and skew min/avg/median).
void MLX90640Component::prime_frame_() {
  uint32_t start = millis();
  while (subpages_seen_ != 0x03 && (millis() - start < 1000)) {
    if (poll_subpage_() == 0)
      delay(2);  // no fresh subpage yet; yield briefly and retry
  }
  frame_primed_ = (subpages_seen_ == 0x03);
  if (frame_primed_) {
    ESP_LOGCONFIG(TAG, "MLX90640 primed both subpages");
  } else {
    ESP_LOGW(TAG, "MLX90640 frame priming incomplete (subpages=0x%02X)", subpages_seen_);
  }
}

bool MLX90640Component::read_frame_() {
  // Rolling, non-blocking frame read: pull however many subpages are already
  // waiting (0, 1, or — when not single_frame — up to 2) without ever blocking
  // on the next conversion. The opposite checkerboard half carries over from the
  // previous cycle. single_frame caps this at one subpage per tick (less I2C bus
  // load, slightly more motion smear); the default reads both when both are ready.
  int max_reads = single_frame_ ? 1 : 2;
  int reads = 0;
  for (int i = 0; i < max_reads; i++) {
    int r = poll_subpage_();
    if (r <= 0)
      break;  // error, or no fresh subpage ready right now — do not block
    reads++;
  }

  if (reads == 0)
    return false;  // nothing new this cycle; keep last full frame, skip stats

  // Only report a frame ready once both halves hold real data. setup() primes
  // this; finish it here if priming timed out, so stats/alarm/JPEG never see a
  // half zero-init frame.
  if (!frame_primed_)
    frame_primed_ = (subpages_seen_ == 0x03);
  return frame_primed_;
}

void MLX90640Component::dump_device_config_() {
  ESP_LOGCONFIG(TAG, "  Refresh Rate: %s", refresh_rate_.c_str());
  ESP_LOGCONFIG(TAG, "  Resolution: %s", resolution_.c_str());
  ESP_LOGCONFIG(TAG, "  Pattern: %s", pattern_.c_str());
  ESP_LOGCONFIG(TAG, "  Single Frame: %s", single_frame_ ? "true" : "false");
}

// Configuration helper functions
uint16_t MLX90640Component::parse_refresh_rate_(const std::string &rate_str) {
  if (rate_str == "0.5Hz")
    return 0;
  if (rate_str == "1Hz")
    return 1;
  if (rate_str == "2Hz")
    return 2;
  if (rate_str == "4Hz")
    return 3;
  if (rate_str == "8Hz")
    return 4;
  if (rate_str == "16Hz")
    return 5;
  if (rate_str == "32Hz")
    return 6;
  if (rate_str == "64Hz")
    return 7;
  ESP_LOGW(TAG, "Unknown refresh rate: %s, defaulting to 16Hz", rate_str.c_str());
  return 5;  // Default to 16Hz
}

int MLX90640Component::parse_resolution_(const std::string &res_str) {
  if (res_str == "16-bit")
    return 16;
  if (res_str == "17-bit")
    return 17;
  if (res_str == "18-bit")
    return 18;
  if (res_str == "19-bit")
    return 19;
  ESP_LOGW(TAG, "Unknown resolution: %s, defaulting to 18-bit", res_str.c_str());
  return 18;  // Default to 18-bit
}

int MLX90640Component::setup_thermal_resolution_(int bits) {
  uint8_t resolution = 0;
  switch (bits) {
    case 16:
      resolution = 0;
      break;
    case 17:
      resolution = 1;
      break;
    case 18:
      resolution = 2;
      break;
    case 19:
      resolution = 3;
      break;
    default:
      ESP_LOGW(TAG, "Invalid resolution bits: %d, using 18-bit", bits);
      resolution = 2;
  }
  return MLX90640_SetResolution(this->address_, resolution);
}

int MLX90640Component::setup_thermal_pattern_(const std::string &pattern) {
  // bad_pixel_mode_ must match the readout pattern: MLX90640_BadPixelsCorrection
  // interpolates from different neighbors in chess (1) vs interleaved (0) mode.
  if (pattern == "chess") {
    bad_pixel_mode_ = 1;
    return MLX90640_SetChessMode(this->address_);
  } else if (pattern == "interleaved") {
    bad_pixel_mode_ = 0;
    return MLX90640_SetInterleavedMode(this->address_);
  } else {
    ESP_LOGW(TAG, "Unknown pattern mode: %s, using chess", pattern.c_str());
    bad_pixel_mode_ = 1;
    return MLX90640_SetChessMode(this->address_);
  }
}

// State synchronization - sync the base ROI/update_interval controls, then the
// device-specific thermography number controls.
void MLX90640Component::sync_roi_state_from_controls() {
  thermal_camera_core::ThermalCameraBase::sync_roi_state_from_controls();

  // ThermalNumber::setup() only publish_state()s the restored value; it never
  // calls control(). Pick up the restored emissivity / reflected-temperature
  // here so a value restored from flash (or the YAML initial) actually drives
  // MLX90640_CalculateTo, instead of the UI showing one value while the sensor
  // keeps using the compile-time default.
  if (emissivity_control_ != nullptr && !std::isnan(emissivity_control_->state)) {
    emissivity_ = emissivity_control_->state;
    ESP_LOGD(TAG, "Synced emissivity: %.3f", emissivity_);
  }
  if (reflected_temperature_control_ != nullptr && !std::isnan(reflected_temperature_control_->state)) {
    reflected_temperature_ = reflected_temperature_control_->state;
    ESP_LOGD(TAG, "Synced reflected temperature: %.1f°C", reflected_temperature_);
  }
}

// Fall-through for device-specific numeric controls (emissivity, reflected temperature).
void MLX90640Component::on_extra_number_control(int type, float value) {
  switch (type) {
    case EMISSIVITY:
      update_emissivity(value);
      ESP_LOGD(TAG, "Emissivity changed to %.3f", value);
      break;

    case REFLECTED_TEMPERATURE:
      update_reflected_temperature(value);
      ESP_LOGD(TAG, "Reflected temperature changed to %.1f°C", value);
      break;

    default:
      ESP_LOGE(TAG, "Unknown control type");
      break;
  }
}

// Fall-through for device-specific switch controls (reflected-temperature auto mode).
void MLX90640Component::on_extra_switch_control(int type, bool value) {
  if (type == REFLECTED_TEMPERATURE_AUTO) {
    update_reflected_temperature_auto(value);
    ESP_LOGD(TAG, "Reflected temperature auto changed to %s", value ? "true" : "false");
  } else {
    ESP_LOGE(TAG, "Unknown control type");
  }
}

}  // namespace mlx90640
}  // namespace esphome
