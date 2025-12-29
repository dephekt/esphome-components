#include "mlx90640.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cstdio>

namespace esphome {
namespace mlx90640 {

static const char *const TAG = "mlx90640";

void MLX90640Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MLX90640...");

  setup_thermal_();

  // Initialize thermal color palette
  set_active_palette_();

  // Sync ROI state from control entities (they have higher setup priority)
  sync_roi_state_from_controls();

#ifdef USE_NETWORK
  // Setup web server if enabled
  if (web_server_enabled_) {
    setup_web_server_();
  }
#endif

  ESP_LOGCONFIG(TAG, "MLX90640 setup complete");
}

void MLX90640Component::loop() {
  uint32_t now = millis();
  if (now - last_update_time_ > update_interval_) {
    update_thermal_data_();
    last_update_time_ = now;
  }
}

void MLX90640Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MLX90640:");
  ESP_LOGCONFIG(TAG, "  Refresh Rate: %s", refresh_rate_.c_str());
  ESP_LOGCONFIG(TAG, "  Resolution: %s", resolution_.c_str());
  ESP_LOGCONFIG(TAG, "  Pattern: %s", pattern_.c_str());
  ESP_LOGCONFIG(TAG, "  Single Frame: %s", single_frame_ ? "true" : "false");
  ESP_LOGCONFIG(TAG, "  Update Interval: %dms", update_interval_);
  if (roi_config_.enabled) {
    ESP_LOGCONFIG(TAG, "  ROI Enabled: Center(%d,%d) Size=%d", roi_config_.center_row, roi_config_.center_col,
                  roi_config_.size);
  }
#ifdef USE_NETWORK
  if (web_server_enabled_) {
    ESP_LOGCONFIG(TAG, "  Web Server: %s (160x120, quality=%d)", web_server_path_.c_str(), web_server_quality_);
  }
#endif
}

void MLX90640Component::setup_thermal_() {
  ESP_LOGCONFIG(TAG, "Initializing MLX90640 thermal camera...");

  // Dump EEPROM parameters
  uint16_t eeMLX90640[832];
  int status = MLX90640_DumpEE(this->address_, eeMLX90640);
  if (status != 0) {
    ESP_LOGE(TAG, "Failed to dump MLX90640 EEPROM: %d", status);
    return;
  }

  // Extract calibration parameters
  status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640_params_);
  if (status != 0) {
    ESP_LOGE(TAG, "Failed to extract MLX90640 parameters: %d", status);
    return;
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

  initialized_ = true;
  ESP_LOGCONFIG(TAG, "MLX90640 thermal camera initialized successfully");
}

void MLX90640Component::update_thermal_data_() {
  if (!initialized_)
    return;

  ESP_LOGD(TAG, "Reading MLX90640 thermal data...");

  // Synchronize frame to ensure fresh data is available
  int sync_status = MLX90640_SynchFrame(this->address_);
  if (sync_status != 0) {
    ESP_LOGW(TAG, "MLX90640 frame synchronization failed: %d", sync_status);
    return;
  }

  // Read frames based on configuration
  bool frame_read = false;
  int max_frames = single_frame_ ? 1 : 2;
  int consecutive_failures = 0;
  int subpages_collected = 0;

  for (byte attempt = 0; attempt < max_frames; attempt++) {
    uint32_t start_time = millis();
    int status = MLX90640_GetFrameData(this->address_, mlx90640Frame_);
    uint32_t read_time = millis() - start_time;

    if (status < 0) {
      ESP_LOGD(TAG, "MLX90640 GetFrame attempt %d error: %d (took %dms)", attempt, status, read_time);
      consecutive_failures++;
      if (consecutive_failures > 3) {
        ESP_LOGW(TAG, "MLX90640 consecutive failures, skipping thermal update");
        return;
      }
      continue;
    }

    // Calculate temperatures using this frame
    float vdd = MLX90640_GetVdd(mlx90640Frame_, &mlx90640_params_);
    float Ta = MLX90640_GetTa(mlx90640Frame_, &mlx90640_params_);
    float tr = Ta - TA_SHIFT;
    float emissivity = 0.95;

    // Calculate pixel temperatures
    MLX90640_CalculateTo(mlx90640Frame_, &mlx90640_params_, emissivity, tr, mlx90640_pixels_);

    // Fix bad pixels using neighboring pixel interpolation
    MLX90640_BadPixelsCorrection(mlx90640_params_.brokenPixels, mlx90640_pixels_, 1, &mlx90640_params_);
    MLX90640_BadPixelsCorrection(mlx90640_params_.outlierPixels, mlx90640_pixels_, 1, &mlx90640_params_);

    // Interpolate thermal data from 24x32 to 48x64 for smoother display
    interpolate_image_(mlx90640_pixels_, 24, 32, interpolated_pixels_, 48, 64);

    frame_read = true;
    subpages_collected++;

    // For single frame mode, we're done after one successful read
    if (single_frame_) {
      ESP_LOGD(TAG, "Single frame mode: collected 1 subpage");
      break;
    }

    // For dual subpage mode, continue collecting until we have both or reach max attempts
    if (subpages_collected >= 2) {
      ESP_LOGD(TAG, "Dual subpage mode: collected %d subpages", subpages_collected);
      break;
    }
  }

  if (!frame_read) {
    ESP_LOGD(TAG, "Failed to read any MLX90640 frames");
    return;
  }

  // Reset failure count on successful read
  consecutive_failures = 0;

  // Process thermal data with better statistics
  float min_temp = mlx90640_pixels_[0];
  float max_temp = mlx90640_pixels_[0];
  float sum_temp = 0;
  int valid_count = 0;

  // Filter out invalid/extreme readings and collect valid data
  for (int i = 0; i < 768; i++) {
    float temp = mlx90640_pixels_[i];
    // Filter out obviously bad readings (typical range -40°C to 85°C for MLX90640)
    if (temp > -40.0 && temp < 85.0) {
      valid_pixels_[valid_count++] = temp;
      if (temp < min_temp)
        min_temp = temp;
      if (temp > max_temp)
        max_temp = temp;
      sum_temp += temp;
    }
  }

  if (valid_count > 0) {
    min_temp_ = min_temp;
    max_temp_ = max_temp;
    avg_temp_ = sum_temp / valid_count;

    // Calculate median temperature - use std::sort for O(n log n) instead of O(n²) bubble sort
    std::sort(valid_pixels_, valid_pixels_ + valid_count);
    median_temp_ = valid_pixels_[valid_count / 2];

    // Update sensors if configured
    if (temp_min_sensor_)
      temp_min_sensor_->publish_state(min_temp_);
    if (temp_max_sensor_)
      temp_max_sensor_->publish_state(max_temp_);
    if (temp_avg_sensor_)
      temp_avg_sensor_->publish_state(avg_temp_);
  } else {
    ESP_LOGW(TAG, "No valid thermal readings - all pixels out of range");
    return;
  }

  // Process ROI temperatures if enabled
  process_roi_temperatures_();

  ESP_LOGD(TAG, "MLX90640 data (%d valid pixels) - Min: %.1f°C, Max: %.1f°C, Avg: %.1f°C, Median: %.1f°C", valid_count,
           min_temp_, max_temp_, avg_temp_, median_temp_);
}

// ROI calculation helper - converts 1-based user coordinates to 0-based array bounds
void MLX90640Component::calculate_roi_bounds_(int center_row, int center_col, int size, int &min_row, int &max_row,
                                              int &min_col, int &max_col) const {
  // Convert 1-based user coordinates to 0-based array indices
  int center_row_idx = center_row - 1;  // Convert 1-24 to 0-23
  int center_col_idx = center_col - 1;  // Convert 1-32 to 0-31

  // Calculate ROI bounds (size = n means (2n+1)x(2n+1) square)
  min_row = std::max(0, center_row_idx - size);
  max_row = std::min(23, center_row_idx + size);  // MLX90640 has 24 rows (0-23)
  min_col = std::max(0, center_col_idx - size);
  max_col = std::min(31, center_col_idx + size);  // MLX90640 has 32 columns (0-31)
}

// Process ROI temperatures from the main mlx90640_pixels_ array
void MLX90640Component::process_roi_temperatures_() {
  if (!roi_config_.enabled || !initialized_) {
    return;
  }

  int min_row, max_row, min_col, max_col;
  calculate_roi_bounds_(roi_config_.center_row, roi_config_.center_col, roi_config_.size, min_row, max_row, min_col,
                        max_col);

  roi_pixel_count_ = 0;
  float min_temp = 1000.0f;   // Initialize to unrealistically high value
  float max_temp = -1000.0f;  // Initialize to unrealistically low value
  float sum_temp = 0.0f;

  // Collect valid ROI pixels
  for (int row = min_row; row <= max_row; row++) {
    for (int col = min_col; col <= max_col; col++) {
      int pixel_idx = row * 32 + col;  // MLX90640 is 32 columns wide
      float temp = mlx90640_pixels_[pixel_idx];

      // Filter out invalid/extreme readings (same range as main processing)
      if (temp > -40.0 && temp < 85.0) {
        valid_pixels_[roi_pixel_count_++] = temp;  // Reuse the existing valid_pixels_ array
        if (temp < min_temp)
          min_temp = temp;
        if (temp > max_temp)
          max_temp = temp;
        sum_temp += temp;
      }
    }
  }

  if (roi_pixel_count_ > 0) {
    roi_min_temp_ = min_temp;
    roi_max_temp_ = max_temp;
    roi_avg_temp_ = sum_temp / roi_pixel_count_;

    // Calculate ROI median temperature
    std::sort(valid_pixels_, valid_pixels_ + roi_pixel_count_);
    roi_median_temp_ = valid_pixels_[roi_pixel_count_ / 2];

    // Update ROI sensors if configured
    if (roi_min_sensor_)
      roi_min_sensor_->publish_state(roi_min_temp_);
    if (roi_max_sensor_)
      roi_max_sensor_->publish_state(roi_max_temp_);
    if (roi_avg_sensor_)
      roi_avg_sensor_->publish_state(roi_avg_temp_);

    ESP_LOGD(TAG, "ROI (%d,%d) size=%d (%d pixels) - Min: %.1f°C, Max: %.1f°C, Avg: %.1f°C, Median: %.1f°C",
             roi_config_.center_row, roi_config_.center_col, roi_config_.size, roi_pixel_count_, roi_min_temp_,
             roi_max_temp_, roi_avg_temp_, roi_median_temp_);
  } else {
    ESP_LOGW(TAG, "ROI has no valid temperature readings");
  }
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
  if (pattern == "chess") {
    return MLX90640_SetChessMode(this->address_);
  } else if (pattern == "interleaved") {
    return MLX90640_SetInterleavedMode(this->address_);
  } else {
    ESP_LOGW(TAG, "Unknown pattern mode: %s, using chess", pattern.c_str());
    return MLX90640_SetChessMode(this->address_);
  }
}

// Thermal interpolation functions for smooth upscaling (copied from grow_env_monitor)
float MLX90640Component::get_point_(float *p, uint8_t rows, uint8_t cols, int8_t x, int8_t y) {
  if (x < 0)
    x = 0;
  if (y < 0)
    y = 0;
  if (x >= cols)
    x = cols - 1;
  if (y >= rows)
    y = rows - 1;
  return p[y * cols + x];
}

void MLX90640Component::set_point_(float *p, uint8_t rows, uint8_t cols, int8_t x, int8_t y, float f) {
  if ((x < 0) || (x >= cols))
    return;
  if ((y < 0) || (y >= rows))
    return;
  p[y * cols + x] = f;
}

void MLX90640Component::get_adjacents_2d_(float *src, float *dest, uint8_t rows, uint8_t cols, int8_t x, int8_t y) {
  // Fill dest with adjacent points in 2D grid format for bicubic interpolation
  for (int dy = -1; dy <= 2; dy++) {
    for (int dx = -1; dx <= 2; dx++) {
      dest[(dy + 1) * 4 + (dx + 1)] = get_point_(src, rows, cols, x + dx, y + dy);
    }
  }
}

float MLX90640Component::cubic_interpolate_(float p[], float x) {
  return p[1] + 0.5 * x *
                    (p[2] - p[0] +
                     x * (2.0 * p[0] - 5.0 * p[1] + 4.0 * p[2] - p[3] + x * (3.0 * (p[1] - p[2]) + p[3] - p[0])));
}

float MLX90640Component::bicubic_interpolate_(float p[], float x, float y) {
  float arr[4];
  arr[0] = cubic_interpolate_(p, x);
  arr[1] = cubic_interpolate_(p + 4, x);
  arr[2] = cubic_interpolate_(p + 8, x);
  arr[3] = cubic_interpolate_(p + 12, x);
  return cubic_interpolate_(arr, y);
}

void MLX90640Component::interpolate_image_(float *src, uint8_t src_rows, uint8_t src_cols, float *dest,
                                           uint8_t dest_rows, uint8_t dest_cols) {
  float mu_x = (float) src_cols / (float) dest_cols;
  float mu_y = (float) src_rows / (float) dest_rows;

  for (uint8_t y_idx = 0; y_idx < dest_rows; y_idx++) {
    for (uint8_t x_idx = 0; x_idx < dest_cols; x_idx++) {
      float x = x_idx * mu_x;
      float y = y_idx * mu_y;

      get_adjacents_2d_(src, adj_2d_, src_rows, src_cols, (int8_t) x, (int8_t) y);
      float val = bicubic_interpolate_(adj_2d_, x - (int) x, y - (int) y);
      set_point_(dest, dest_rows, dest_cols, x_idx, y_idx, val);
    }
  }
}

// Thermal color palettes (moved from grow_env_monitor to save RAM)
const uint16_t MLX90640Component::thermal_palette_rainbow_[256] PROGMEM = {
    0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x000A, 0x002A, 0x002B, 0x004B, 0x006B, 0x008C,
    0x00AC, 0x00CC, 0x00ED, 0x010D, 0x010E, 0x012E, 0x014E, 0x014F, 0x0170, 0x0190, 0x0190, 0x0191, 0x01B1, 0x01B1,
    0x01B1, 0x01B2, 0x01D2, 0x01D2, 0x01F3, 0x01F3, 0x0213, 0x0214, 0x0234, 0x0234, 0x0235, 0x0255, 0x0256, 0x0276,
    0x0277, 0x0277, 0x0297, 0x0297, 0x02B8, 0x02B8, 0x02D9, 0x02D9, 0x02F9, 0x02F9, 0x02FA, 0x02FA, 0x031A, 0x031A,
    0x031A, 0x033B, 0x033B, 0x035B, 0x035B, 0x035B, 0x037B, 0x037B, 0x039B, 0x039B, 0x03BB, 0x03BB, 0x03DB, 0x03DB,
    0x03FB, 0x03FA, 0x041A, 0x0419, 0x0439, 0x0439, 0x0438, 0x0458, 0x0457, 0x0476, 0x0C75, 0x0C94, 0x0C94, 0x0C93,
    0x0C93, 0x0C92, 0x0CB1, 0x14B0, 0x14CF, 0x1CCE, 0x1CED, 0x24EC, 0x2D0B, 0x2D0A, 0x3529, 0x3D28, 0x4547, 0x4D66,
    0x4D66, 0x5585, 0x5D84, 0x5DA4, 0x65A3, 0x6DC3, 0x6DC2, 0x75E2, 0x75E2, 0x7DE2, 0x8601, 0x8601, 0x8E21, 0x8E21,
    0x9620, 0x9640, 0x9E40, 0xA640, 0xA660, 0xAE60, 0xAE60, 0xAE60, 0xB660, 0xBE80, 0xBE80, 0xC680, 0xC6A0, 0xC6A0,
    0xCEA0, 0xCEA0, 0xD6A0, 0xD6A0, 0xDEA0, 0xDEA0, 0xDEA0, 0xE6A0, 0xE6A0, 0xE6A0, 0xE680, 0xEE80, 0xEE80, 0xEE80,
    0xEE80, 0xEE80, 0xEE80, 0xEE81, 0xEE61, 0xF661, 0xF641, 0xF641, 0xF641, 0xF641, 0xF621, 0xF621, 0xF621, 0xFE01,
    0xFDE1, 0xFDE1, 0xFDC1, 0xFDC2, 0xFDA2, 0xFD82, 0xFD62, 0xFD42, 0xFD42, 0xFD22, 0xFD22, 0xFD02, 0xFD02, 0xFCE2,
    0xFCE2, 0xFCC3, 0xFCA3, 0xFC63, 0xFC43, 0xFC23, 0xFC03, 0xFBE4, 0xFBA4, 0xFB64, 0xFB44, 0xFB24, 0xFB04, 0xFAE5,
    0xFAC5, 0xFA85, 0xFA65, 0xFA25, 0xF9E6, 0xF9C6, 0xF9A6, 0xF986, 0xF966, 0xF927, 0xF907, 0xF907, 0xF8E7, 0xF8E7,
    0xF8E7, 0xF8E7, 0xF8C7, 0xF8C8, 0xF8C8, 0xF8C8, 0xF8C8, 0xF8C9, 0xF0C9, 0xF0C9, 0xF0C9, 0xF0CA, 0xF10A, 0xF10A,
    0xF12A, 0xF14B, 0xF16B, 0xF18B, 0xF9AB, 0xF9CC, 0xFA0C, 0xFA4C, 0xFA8D, 0xFAAD, 0xFAED, 0xFAED, 0xFB0D, 0xFB2D,
    0xFB2E, 0xFB2E, 0xFB6E, 0xFBAF, 0xFBCF, 0xFBEF, 0xFC10, 0xFC30, 0xFC50, 0xFC91, 0xFCB1, 0xFCF2, 0xFD12, 0xFD52,
    0xFD73, 0xFD93, 0xFD93, 0xFDD4, 0xFDF4, 0xFE15, 0xFE35, 0xFE55, 0xFE76, 0xFE96, 0xFED7, 0xFED7, 0xFEF8, 0xFEF9,
    0xFF19, 0xFF19, 0xFF39, 0xFF5A,
};

const uint16_t MLX90640Component::thermal_palette_golden_[256] PROGMEM = {
    0x0004, 0x0004, 0x0004, 0x0004, 0x0005, 0x0005, 0x0825, 0x0825, 0x0825, 0x0826, 0x0826, 0x0826, 0x1027, 0x1027,
    0x1027, 0x1027, 0x1828, 0x1828, 0x1848, 0x1849, 0x2049, 0x2049, 0x204A, 0x204A, 0x284A, 0x284B, 0x284B, 0x284B,
    0x306C, 0x306C, 0x306C, 0x386D, 0x386D, 0x386D, 0x408E, 0x408E, 0x408E, 0x408F, 0x488F, 0x488F, 0x4890, 0x5090,
    0x50B0, 0x50B0, 0x58B1, 0x58B1, 0x58B1, 0x58B1, 0x60D2, 0x60D2, 0x60D2, 0x68D2, 0x68D2, 0x68D2, 0x68F3, 0x70F3,
    0x70F3, 0x70F3, 0x78F3, 0x7913, 0x7913, 0x7913, 0x8113, 0x8133, 0x8133, 0x8133, 0x8933, 0x8932, 0x8952, 0x9152,
    0x9152, 0x9152, 0x9151, 0x9971, 0x9971, 0x9971, 0x9970, 0xA190, 0xA190, 0xA18F, 0xA98F, 0xA9AF, 0xA9AE, 0xA9AE,
    0xB1AD, 0xB1CD, 0xB1CD, 0xB9CC, 0xB9EC, 0xB9EB, 0xB9EB, 0xC1EB, 0xC20A, 0xC20A, 0xCA09, 0xCA29, 0xCA29, 0xCA28,
    0xCA28, 0xD247, 0xD247, 0xD247, 0xDA66, 0xDA66, 0xDA65, 0xDA85, 0xDA85, 0xE284, 0xE2A4, 0xE2A4, 0xE2A3, 0xEAC3,
    0xEAC3, 0xEAE2, 0xEAE2, 0xEAE2, 0xF2E2, 0xF301, 0xF301, 0xF321, 0xF321, 0xF321, 0xF340, 0xFB40, 0xFB40, 0xFB60,
    0xFB60, 0xFB80, 0xFB80, 0xFB80, 0xFBA0, 0xFBA0, 0xFBC0, 0xFBC0, 0xFBE0, 0xFBE0, 0xFBE0, 0xFC00, 0xFC00, 0xFC20,
    0xFC20, 0xFC40, 0xFC40, 0xFC60, 0xFC60, 0xFC80, 0xFC80, 0xFC80, 0xFCA0, 0xFCA0, 0xFCC0, 0xFCE0, 0xFCE0, 0xFD00,
    0xFD00, 0xFD20, 0xFD20, 0xFD40, 0xFD40, 0xFD40, 0xFD60, 0xFD60, 0xFD80, 0xFDA0, 0xFDA0, 0xFDC0, 0xFDC0, 0xFDC1,
    0xFDE1, 0xFDE1, 0xFE01, 0xFE01, 0xFE21, 0xFE21, 0xFE41, 0xFE42, 0xFE62, 0xFE62, 0xFE62, 0xFE82, 0xFE82, 0xFE83,
    0xFEA3, 0xFEA3, 0xFEC3, 0xFEC3, 0xFEC3, 0xFEE3, 0xFEE4, 0xFEE4, 0xFF04, 0xFF04, 0xFF04, 0xFF25, 0xFF25, 0xFF25,
    0xFF45, 0xFF46, 0xFF46, 0xFF46, 0xFF67, 0xFF67, 0xFF67, 0xFF68, 0xFF68, 0xFF89, 0xFF89, 0xFF89, 0xFF8A, 0xFF8A,
    0xFFAB, 0xFFAB, 0xFFAC, 0xFFAC, 0xFFAD, 0xFFAD, 0xFFAE, 0xFFCE, 0xFFCE, 0xFFCF, 0xFFD0, 0xFFD0, 0xFFD1, 0xFFD1,
    0xFFD2, 0xFFD2, 0xFFD3, 0xFFD3, 0xFFD4, 0xFFD4, 0xFFD5, 0xFFF5, 0xFFF6, 0xFFF6, 0xFFF7, 0xFFF7, 0xFFF8, 0xFFF8,
    0xFFF9, 0xFFF9, 0xFFF9, 0xFFFA, 0xFFFA, 0xFFFB, 0xFFFB, 0xFFFC, 0xFFFC, 0xFFFC, 0xFFFD, 0xFFFD, 0xFFFD, 0xFFFE,
    0xFFFE, 0xFFFE, 0xFFFF, 0xFFFF,
};

const uint16_t MLX90640Component::thermal_palette_grayscale_[256] PROGMEM = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0020, 0x0020, 0x0820, 0x0820, 0x0840, 0x0840, 0x0841, 0x0841, 0x0861, 0x0861,
    0x1061, 0x1061, 0x1081, 0x1081, 0x1082, 0x1082, 0x10a2, 0x10a2, 0x18a2, 0x18a2, 0x18c2, 0x18c2, 0x18c3, 0x18c3,
    0x18e3, 0x18e3, 0x20e3, 0x20e3, 0x2103, 0x2103, 0x2104, 0x2104, 0x2124, 0x2124, 0x2924, 0x2924, 0x2944, 0x2944,
    0x2945, 0x2945, 0x2965, 0x2965, 0x3165, 0x3165, 0x3185, 0x3185, 0x3186, 0x3186, 0x31a6, 0x31a6, 0x39a6, 0x39a6,
    0x39c6, 0x39c6, 0x39c7, 0x39c7, 0x39e7, 0x39e7, 0x41e7, 0x41e7, 0x4207, 0x4207, 0x4208, 0x4208, 0x4228, 0x4228,
    0x4a28, 0x4a28, 0x4a48, 0x4a48, 0x4a49, 0x4a49, 0x4a69, 0x4a69, 0x5269, 0x5269, 0x5289, 0x5289, 0x528a, 0x528a,
    0x52aa, 0x52aa, 0x5aaa, 0x5aaa, 0x5aca, 0x5aca, 0x5acb, 0x5acb, 0x5aeb, 0x5aeb, 0x62eb, 0x62eb, 0x630b, 0x630b,
    0x630c, 0x630c, 0x632c, 0x632c, 0x6b2c, 0x6b2c, 0x6b4c, 0x6b4c, 0x6b4d, 0x6b4d, 0x6b6d, 0x6b6d, 0x736d, 0x736d,
    0x738d, 0x738d, 0x738e, 0x738e, 0x73ae, 0x73ae, 0x7bae, 0x7bae, 0x7bce, 0x7bce, 0x7bcf, 0x7bcf, 0x7bef, 0x7bef,
    0x83ef, 0x83ef, 0x840f, 0x840f, 0x8410, 0x8410, 0x8430, 0x8430, 0x8c30, 0x8c30, 0x8c50, 0x8c50, 0x8c51, 0x8c51,
    0x8c71, 0x8c71, 0x9471, 0x9471, 0x9491, 0x9491, 0x9492, 0x9492, 0x94b2, 0x94b2, 0x9cb2, 0x9cb2, 0x9cd2, 0x9cd2,
    0x9cd3, 0x9cd3, 0x9cf3, 0x9cf3, 0xa4f3, 0xa4f3, 0xa513, 0xa513, 0xa514, 0xa514, 0xa534, 0xa534, 0xad34, 0xad34,
    0xad54, 0xad54, 0xad55, 0xad55, 0xad75, 0xad75, 0xb575, 0xb575, 0xb595, 0xb595, 0xb596, 0xb596, 0xb5b6, 0xb5b6,
    0xbdb6, 0xbdb6, 0xbdd6, 0xbdd6, 0xbdd7, 0xbdd7, 0xbdf7, 0xbdf7, 0xc5f7, 0xc5f7, 0xc617, 0xc617, 0xc618, 0xc618,
    0xc638, 0xc638, 0xce38, 0xce38, 0xce58, 0xce58, 0xce59, 0xce59, 0xce79, 0xce79, 0xd679, 0xd679, 0xd699, 0xd699,
    0xd69a, 0xd69a, 0xd6ba, 0xd6ba, 0xdeba, 0xdeba, 0xdeda, 0xdeda, 0xdedb, 0xdedb, 0xdefb, 0xdefb, 0xe6fb, 0xe6fb,
    0xe71b, 0xe71b, 0xe71c, 0xe71c, 0xe73c, 0xe73c, 0xef3c, 0xef3c, 0xef5c, 0xef5c, 0xef5d, 0xef5d, 0xef7d, 0xef7d,
    0xf77d, 0xf77d, 0xf79d, 0xf79d, 0xf79e, 0xf79e, 0xf7be, 0xf7be, 0xffbe, 0xffbe, 0xffde, 0xffde, 0xffdf, 0xffdf,
    0xffff, 0xffff, 0xffff, 0xffff,
};

const uint16_t MLX90640Component::thermal_palette_ironblack_[256] PROGMEM = {
    0xFFFF, 0xFFFF, 0xFFDF, 0xFFDF, 0xF7BE, 0xF7BE, 0xF79E, 0xF79E, 0xEF7D, 0xEF7D, 0xEF5D, 0xEF5D, 0xE73C, 0xE73C,
    0xE71C, 0xE71C, 0xDEFB, 0xDEFB, 0xDEDB, 0xDEDB, 0xD6BA, 0xD6BA, 0xD69A, 0xD69A, 0xCE79, 0xCE79, 0xCE59, 0xCE59,
    0xC638, 0xC638, 0xC618, 0xC618, 0xBDF7, 0xBDF7, 0xBDD7, 0xBDD7, 0xB5B6, 0xB5B6, 0xB596, 0xB596, 0xAD75, 0xAD75,
    0xAD55, 0xAD55, 0xA534, 0xA534, 0xA514, 0xA514, 0x9CF3, 0x9CF3, 0x9CD3, 0x9CD3, 0x94B2, 0x94B2, 0x9492, 0x9492,
    0x8C71, 0x8C71, 0x8C51, 0x8C51, 0x8430, 0x8430, 0x8410, 0x8410, 0x7BEF, 0x7BEF, 0x7BCF, 0x7BCF, 0x73AE, 0x73AE,
    0x738E, 0x738E, 0x6B6D, 0x6B6D, 0x6B4D, 0x6B4D, 0x632C, 0x632C, 0x630C, 0x630C, 0x5AEB, 0x5AEB, 0x5ACB, 0x5ACB,
    0x52AA, 0x52AA, 0x528A, 0x528A, 0x4A69, 0x4A69, 0x4A49, 0x4A49, 0x4228, 0x4228, 0x4208, 0x4208, 0x39E7, 0x39E7,
    0x39C7, 0x39C7, 0x31A6, 0x31A6, 0x3186, 0x3186, 0x2965, 0x2965, 0x2945, 0x2945, 0x2124, 0x2124, 0x2104, 0x2104,
    0x18E3, 0x18E3, 0x18C3, 0x18C3, 0x10A2, 0x10A2, 0x1082, 0x1082, 0x0861, 0x0861, 0x0841, 0x0841, 0x0020, 0x0020,
    0x0000, 0x0000, 0x0001, 0x0002, 0x0003, 0x0003, 0x0804, 0x0805, 0x0806, 0x0807, 0x1008, 0x1009, 0x100A, 0x100B,
    0x180C, 0x180C, 0x180D, 0x180E, 0x200F, 0x280F, 0x280F, 0x300F, 0x380F, 0x380F, 0x400F, 0x400F, 0x4810, 0x5010,
    0x5010, 0x5810, 0x6010, 0x6010, 0x6810, 0x6810, 0x7011, 0x7811, 0x7811, 0x8011, 0x8011, 0x8811, 0x9011, 0x9031,
    0x9831, 0x9831, 0xA031, 0xA031, 0xA831, 0xB031, 0xB031, 0xB831, 0xB851, 0xB870, 0xC08F, 0xC08F, 0xC0AE, 0xC8CD,
    0xC8ED, 0xC8EC, 0xC90B, 0xD12B, 0xD14A, 0xD14A, 0xD969, 0xD988, 0xD9A8, 0xD9A7, 0xE1C6, 0xE1E5, 0xE205, 0xE205,
    0xE224, 0xE244, 0xE264, 0xE284, 0xE2A3, 0xEAC3, 0xEAE3, 0xEAE2, 0xEB02, 0xEB22, 0xEB41, 0xEB61, 0xEB81, 0xF3A1,
    0xF3A1, 0xF3C1, 0xF3E1, 0xF401, 0xF421, 0xF441, 0xF461, 0xF481, 0xF4A1, 0xF4C1, 0xF4E1, 0xF501, 0xF501, 0xF521,
    0xF541, 0xFD61, 0xFD81, 0xFDA2, 0xFDC2, 0xFDE2, 0xFE02, 0xFE22, 0xFE22, 0xFE42, 0xFE63, 0xFE83, 0xFEA3, 0xFEC3,
    0xFEE3, 0xFF03, 0xFF04, 0xFF26, 0xFF28, 0xFF4A, 0xFF4B, 0xFF6D, 0xFF6F, 0xFF91, 0xFF92, 0xFFB4, 0xFFB6, 0xFFD8,
    0xFFD9, 0xFFDB, 0xFFFD, 0xFFE3,
};

const uint16_t MLX90640Component::thermal_palette_cam_[256] PROGMEM = {
    0x480F, 0x400F, 0x400F, 0x400F, 0x4010, 0x3810, 0x3810, 0x3810, 0x3810, 0x3010, 0x3010, 0x3010, 0x2810, 0x2810,
    0x2810, 0x2810, 0x2010, 0x2010, 0x2010, 0x1810, 0x1810, 0x1811, 0x1811, 0x1011, 0x1011, 0x1011, 0x0811, 0x0811,
    0x0811, 0x0011, 0x0011, 0x0011, 0x0011, 0x0011, 0x0031, 0x0031, 0x0051, 0x0072, 0x0072, 0x0092, 0x00B2, 0x00B2,
    0x00D2, 0x00F2, 0x00F2, 0x0112, 0x0132, 0x0152, 0x0152, 0x0172, 0x0192, 0x0192, 0x01B2, 0x01D2, 0x01F3, 0x01F3,
    0x0213, 0x0233, 0x0253, 0x0253, 0x0273, 0x0293, 0x02B3, 0x02D3, 0x02D3, 0x02F3, 0x0313, 0x0333, 0x0333, 0x0353,
    0x0373, 0x0394, 0x03B4, 0x03D4, 0x03D4, 0x03F4, 0x0414, 0x0434, 0x0454, 0x0474, 0x0474, 0x0494, 0x04B4, 0x04D4,
    0x04F4, 0x0514, 0x0534, 0x0534, 0x0554, 0x0554, 0x0574, 0x0574, 0x0573, 0x0573, 0x0573, 0x0572, 0x0572, 0x0572,
    0x0571, 0x0591, 0x0591, 0x0590, 0x0590, 0x058F, 0x058F, 0x058F, 0x058E, 0x05AE, 0x05AE, 0x05AD, 0x05AD, 0x05AD,
    0x05AC, 0x05AC, 0x05AB, 0x05CB, 0x05CB, 0x05CA, 0x05CA, 0x05CA, 0x05C9, 0x05C9, 0x05C8, 0x05E8, 0x05E8, 0x05E7,
    0x05E7, 0x05E6, 0x05E6, 0x05E6, 0x05E5, 0x05E5, 0x0604, 0x0604, 0x0604, 0x0603, 0x0603, 0x0602, 0x0602, 0x0601,
    0x0621, 0x0621, 0x0620, 0x0620, 0x0620, 0x0620, 0x0E20, 0x0E20, 0x0E40, 0x1640, 0x1640, 0x1E40, 0x1E40, 0x2640,
    0x2640, 0x2E40, 0x2E60, 0x3660, 0x3660, 0x3E60, 0x3E60, 0x3E60, 0x4660, 0x4660, 0x4E60, 0x4E80, 0x5680, 0x5680,
    0x5E80, 0x5E80, 0x6680, 0x6680, 0x6E80, 0x6EA0, 0x76A0, 0x76A0, 0x7EA0, 0x7EA0, 0x86A0, 0x86A0, 0x8EA0, 0x8EC0,
    0x96C0, 0x96C0, 0x9EC0, 0x9EC0, 0xA6C0, 0xAEC0, 0xAEC0, 0xB6E0, 0xB6E0, 0xBEE0, 0xBEE0, 0xC6E0, 0xC6E0, 0xCEE0,
    0xCEE0, 0xD6E0, 0xD700, 0xDF00, 0xDEE0, 0xDEC0, 0xDEA0, 0xDE80, 0xDE80, 0xE660, 0xE640, 0xE620, 0xE600, 0xE5E0,
    0xE5C0, 0xE5A0, 0xE580, 0xE560, 0xE540, 0xE520, 0xE500, 0xE4E0, 0xE4C0, 0xE4A0, 0xE480, 0xE460, 0xEC40, 0xEC20,
    0xEC00, 0xEBE0, 0xEBC0, 0xEBA0, 0xEB80, 0xEB60, 0xEB40, 0xEB20, 0xEB00, 0xEAE0, 0xEAC0, 0xEAA0, 0xEA80, 0xEA60,
    0xEA40, 0xF220, 0xF200, 0xF1E0, 0xF1C0, 0xF1A0, 0xF180, 0xF160, 0xF140, 0xF100, 0xF0E0, 0xF0C0, 0xF0A0, 0xF080,
    0xF060, 0xF040, 0xF020, 0xF800,
};

const uint16_t MLX90640Component::thermal_palette_ironbow_[256] PROGMEM = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
    0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002,
    0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0002, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
    0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0004, 0x0004, 0x0004,
    0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0004, 0x0005,
    0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005, 0x0005,
    0x0005, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006, 0x0006,
    0x0006, 0x0006, 0x0006, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0007,
    0x0007, 0x0007, 0x0007, 0x0007, 0x0007, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008,
    0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0008, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009,
    0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x0009, 0x000A, 0x000A, 0x000A, 0x000A, 0x000A,
    0x000A, 0x000A, 0x000A, 0x000A, 0x000A, 0x000A, 0x000A, 0x000A, 0x000A, 0x000A, 0x000A, 0x000B, 0x000B, 0x000B,
    0x000B, 0x000B, 0x000B, 0x000B, 0x000B, 0x000B, 0x000B, 0x000B, 0x000B, 0x000B, 0x000B, 0x000B, 0x000B, 0x000C,
    0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C, 0x000C,
    0x000C, 0x000D, 0x000D, 0x000D, 0x000D, 0x000D, 0x000D, 0x000D, 0x000D, 0x000D, 0x000D, 0x000D, 0x000D, 0x000D,
    0x000D, 0x000D, 0x000D, 0x000E, 0x000E, 0x000E, 0x000E, 0x000E, 0x000E, 0x000E, 0x000E, 0x000E, 0x000E, 0x000E,
    0x000E, 0x000E, 0x000E, 0x000E, 0x000E, 0x000F, 0x000F, 0x000F, 0x000F, 0x000F, 0x000F, 0x000F, 0x000F, 0x000F,
    0x000F, 0x000F, 0x000F, 0x000F,
};

const uint16_t MLX90640Component::thermal_palette_arctic_[256] PROGMEM = {
    0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F, 0x001F,
    0x001F, 0x001F, 0x003F, 0x003F, 0x003F, 0x003F, 0x003F, 0x003F, 0x003F, 0x003F, 0x005F, 0x005F, 0x005F, 0x005F,
    0x005F, 0x005F, 0x005F, 0x005F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x007F, 0x009F, 0x009F,
    0x009F, 0x009F, 0x009F, 0x009F, 0x009F, 0x009F, 0x00BF, 0x00BF, 0x00BF, 0x00BF, 0x00BF, 0x00BF, 0x00BF, 0x00BF,
    0x00DF, 0x00DF, 0x00DF, 0x00DF, 0x00DF, 0x00DF, 0x00DF, 0x00DF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF,
    0x00FF, 0x00FF, 0x01FF, 0x01FF, 0x01FF, 0x01FF, 0x01FF, 0x01FF, 0x01FF, 0x01FF, 0x03FF, 0x03FF, 0x03FF, 0x03FF,
    0x03FF, 0x03FF, 0x03FF, 0x03FF, 0x05FF, 0x05FF, 0x05FF, 0x05FF, 0x05FF, 0x05FF, 0x05FF, 0x05FF, 0x07FF, 0x07FF,
    0x07FF, 0x07FF, 0x07FF, 0x07FF, 0x07FF, 0x07FF, 0x0FFF, 0x0FFF, 0x0FFF, 0x0FFF, 0x0FFF, 0x0FFF, 0x0FFF, 0x0FFF,
    0x1FFF, 0x1FFF, 0x1FFF, 0x1FFF, 0x1FFF, 0x1FFF, 0x1FFF, 0x1FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF,
    0x3FFF, 0x3FFF, 0x5FFF, 0x5FFF, 0x5FFF, 0x5FFF, 0x5FFF, 0x5FFF, 0x5FFF, 0x5FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF,
    0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FE0, 0x7FE0, 0x7FE0, 0x7FE0, 0x7FE0, 0x7FE0, 0x7FE0, 0x7FE0, 0x7FC0, 0x7FC0,
    0x7FC0, 0x7FC0, 0x7FC0, 0x7FC0, 0x7FC0, 0x7FC0, 0x7FA0, 0x7FA0, 0x7FA0, 0x7FA0, 0x7FA0, 0x7FA0, 0x7FA0, 0x7FA0,
    0x7F80, 0x7F80, 0x7F80, 0x7F80, 0x7F80, 0x7F80, 0x7F80, 0x7F80, 0x7F60, 0x7F60, 0x7F60, 0x7F60, 0x7F60, 0x7F60,
    0x7F60, 0x7F60, 0x7F40, 0x7F40, 0x7F40, 0x7F40, 0x7F40, 0x7F40, 0x7F40, 0x7F40, 0x7F20, 0x7F20, 0x7F20, 0x7F20,
    0x7F20, 0x7F20, 0x7F20, 0x7F20, 0x7F00, 0x7F00, 0x7F00, 0x7F00, 0x7F00, 0x7F00, 0x7F00, 0x7F00, 0x7E00, 0x7E00,
    0x7E00, 0x7E00, 0x7E00, 0x7E00, 0x7E00, 0x7E00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00, 0x7C00,
    0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000,
    0x7000, 0x7000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x4000, 0x4000, 0x4000, 0x4000,
    0x4000, 0x4000, 0x4000, 0x4000,
};

const uint16_t MLX90640Component::thermal_palette_lava_[256] PROGMEM = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0800, 0x0800, 0x0800, 0x0800, 0x0800, 0x0800, 0x0800, 0x0800, 0x0800, 0x0800, 0x0800, 0x0800,
    0x0800, 0x0800, 0x0800, 0x0800, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000,
    0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1000, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800,
    0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x1800, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000,
    0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2000, 0x2800, 0x2800, 0x2800, 0x2800,
    0x2800, 0x2800, 0x2800, 0x2800, 0x2800, 0x2800, 0x2800, 0x2800, 0x2800, 0x2800, 0x2800, 0x2800, 0x3000, 0x3000,
    0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000, 0x3000,
    0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800,
    0x3800, 0x3800, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000, 0x4000,
    0x4000, 0x4000, 0x4000, 0x4000, 0x4800, 0x4800, 0x4800, 0x4800, 0x4800, 0x4800, 0x4800, 0x4800, 0x4800, 0x4800,
    0x4800, 0x4800, 0x4800, 0x4800, 0x4800, 0x4800, 0x5000, 0x5000, 0x5000, 0x5000, 0x5000, 0x5000, 0x5000, 0x5000,
    0x5000, 0x5000, 0x5000, 0x5000, 0x5000, 0x5000, 0x5000, 0x5000, 0x5800, 0x5800, 0x5800, 0x5800, 0x5800, 0x5800,
    0x5800, 0x5800, 0x5800, 0x5800, 0x5800, 0x5800, 0x5800, 0x5800, 0x5800, 0x5800, 0x6000, 0x6000, 0x6000, 0x6000,
    0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6000, 0x6800, 0x6800,
    0x6800, 0x6800, 0x6800, 0x6800, 0x6800, 0x6800, 0x6800, 0x6800, 0x6800, 0x6800, 0x6800, 0x6800, 0x6800, 0x6800,
    0x7000, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000, 0x7000,
    0x7000, 0x7000, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800, 0x7800,
    0x7800, 0x7800, 0x7800, 0x7800,
};

const uint16_t MLX90640Component::thermal_palette_whitehot_[256] PROGMEM = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0841, 0x0841, 0x0841, 0x0841, 0x0841, 0x0841,
    0x0841, 0x0841, 0x1082, 0x1082, 0x1082, 0x1082, 0x1082, 0x1082, 0x1082, 0x1082, 0x18C3, 0x18C3, 0x18C3, 0x18C3,
    0x18C3, 0x18C3, 0x18C3, 0x18C3, 0x2104, 0x2104, 0x2104, 0x2104, 0x2104, 0x2104, 0x2104, 0x2104, 0x2945, 0x2945,
    0x2945, 0x2945, 0x2945, 0x2945, 0x2945, 0x2945, 0x3186, 0x3186, 0x3186, 0x3186, 0x3186, 0x3186, 0x3186, 0x3186,
    0x39C7, 0x39C7, 0x39C7, 0x39C7, 0x39C7, 0x39C7, 0x39C7, 0x39C7, 0x4208, 0x4208, 0x4208, 0x4208, 0x4208, 0x4208,
    0x4208, 0x4208, 0x4A49, 0x4A49, 0x4A49, 0x4A49, 0x4A49, 0x4A49, 0x4A49, 0x4A49, 0x528A, 0x528A, 0x528A, 0x528A,
    0x528A, 0x528A, 0x528A, 0x528A, 0x5ACB, 0x5ACB, 0x5ACB, 0x5ACB, 0x5ACB, 0x5ACB, 0x5ACB, 0x5ACB, 0x630C, 0x630C,
    0x630C, 0x630C, 0x630C, 0x630C, 0x630C, 0x630C, 0x6B4D, 0x6B4D, 0x6B4D, 0x6B4D, 0x6B4D, 0x6B4D, 0x6B4D, 0x6B4D,
    0x738E, 0x738E, 0x738E, 0x738E, 0x738E, 0x738E, 0x738E, 0x738E, 0x7BCF, 0x7BCF, 0x7BCF, 0x7BCF, 0x7BCF, 0x7BCF,
    0x7BCF, 0x7BCF, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8C51, 0x8C51, 0x8C51, 0x8C51,
    0x8C51, 0x8C51, 0x8C51, 0x8C51, 0x9492, 0x9492, 0x9492, 0x9492, 0x9492, 0x9492, 0x9492, 0x9492, 0x9CD3, 0x9CD3,
    0x9CD3, 0x9CD3, 0x9CD3, 0x9CD3, 0x9CD3, 0x9CD3, 0xA514, 0xA514, 0xA514, 0xA514, 0xA514, 0xA514, 0xA514, 0xA514,
    0xAD55, 0xAD55, 0xAD55, 0xAD55, 0xAD55, 0xAD55, 0xAD55, 0xAD55, 0xB596, 0xB596, 0xB596, 0xB596, 0xB596, 0xB596,
    0xB596, 0xB596, 0xBDD7, 0xBDD7, 0xBDD7, 0xBDD7, 0xBDD7, 0xBDD7, 0xBDD7, 0xBDD7, 0xC618, 0xC618, 0xC618, 0xC618,
    0xC618, 0xC618, 0xC618, 0xC618, 0xCE59, 0xCE59, 0xCE59, 0xCE59, 0xCE59, 0xCE59, 0xCE59, 0xCE59, 0xD69A, 0xD69A,
    0xD69A, 0xD69A, 0xD69A, 0xD69A, 0xD69A, 0xD69A, 0xDEDB, 0xDEDB, 0xDEDB, 0xDEDB, 0xDEDB, 0xDEDB, 0xDEDB, 0xDEDB,
    0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D,
    0xEF5D, 0xEF5D, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xFFDF, 0xFFDF, 0xFFDF, 0xFFDF,
    0xFFDF, 0xFFDF, 0xFFDF, 0xFFDF,
};

const uint16_t MLX90640Component::thermal_palette_blackhot_[256] PROGMEM = {
    0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFDF, 0xFFDF, 0xFFDF, 0xFFDF, 0xFFDF, 0xFFDF,
    0xFFDF, 0xFFDF, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xF79E, 0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D,
    0xEF5D, 0xEF5D, 0xEF5D, 0xEF5D, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xE71C, 0xDEDB, 0xDEDB,
    0xDEDB, 0xDEDB, 0xDEDB, 0xDEDB, 0xDEDB, 0xDEDB, 0xD69A, 0xD69A, 0xD69A, 0xD69A, 0xD69A, 0xD69A, 0xD69A, 0xD69A,
    0xCE59, 0xCE59, 0xCE59, 0xCE59, 0xCE59, 0xCE59, 0xCE59, 0xCE59, 0xC618, 0xC618, 0xC618, 0xC618, 0xC618, 0xC618,
    0xC618, 0xC618, 0xBDD7, 0xBDD7, 0xBDD7, 0xBDD7, 0xBDD7, 0xBDD7, 0xBDD7, 0xBDD7, 0xB596, 0xB596, 0xB596, 0xB596,
    0xB596, 0xB596, 0xB596, 0xB596, 0xAD55, 0xAD55, 0xAD55, 0xAD55, 0xAD55, 0xAD55, 0xAD55, 0xAD55, 0xA514, 0xA514,
    0xA514, 0xA514, 0xA514, 0xA514, 0xA514, 0xA514, 0x9CD3, 0x9CD3, 0x9CD3, 0x9CD3, 0x9CD3, 0x9CD3, 0x9CD3, 0x9CD3,
    0x9492, 0x9492, 0x9492, 0x9492, 0x9492, 0x9492, 0x9492, 0x9492, 0x8C51, 0x8C51, 0x8C51, 0x8C51, 0x8C51, 0x8C51,
    0x8C51, 0x8C51, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x8410, 0x7BCF, 0x7BCF, 0x7BCF, 0x7BCF,
    0x7BCF, 0x7BCF, 0x7BCF, 0x7BCF, 0x738E, 0x738E, 0x738E, 0x738E, 0x738E, 0x738E, 0x738E, 0x738E, 0x6B4D, 0x6B4D,
    0x6B4D, 0x6B4D, 0x6B4D, 0x6B4D, 0x6B4D, 0x6B4D, 0x630C, 0x630C, 0x630C, 0x630C, 0x630C, 0x630C, 0x630C, 0x630C,
    0x5ACB, 0x5ACB, 0x5ACB, 0x5ACB, 0x5ACB, 0x5ACB, 0x5ACB, 0x5ACB, 0x528A, 0x528A, 0x528A, 0x528A, 0x528A, 0x528A,
    0x528A, 0x528A, 0x4A49, 0x4A49, 0x4A49, 0x4A49, 0x4A49, 0x4A49, 0x4A49, 0x4A49, 0x4208, 0x4208, 0x4208, 0x4208,
    0x4208, 0x4208, 0x4208, 0x4208, 0x39C7, 0x39C7, 0x39C7, 0x39C7, 0x39C7, 0x39C7, 0x39C7, 0x39C7, 0x3186, 0x3186,
    0x3186, 0x3186, 0x3186, 0x3186, 0x3186, 0x3186, 0x2945, 0x2945, 0x2945, 0x2945, 0x2945, 0x2945, 0x2945, 0x2945,
    0x2104, 0x2104, 0x2104, 0x2104, 0x2104, 0x2104, 0x2104, 0x2104, 0x18C3, 0x18C3, 0x18C3, 0x18C3, 0x18C3, 0x18C3,
    0x18C3, 0x18C3, 0x1082, 0x1082, 0x1082, 0x1082, 0x1082, 0x1082, 0x1082, 0x1082, 0x0841, 0x0841, 0x0841, 0x0841,
    0x0841, 0x0841, 0x0841, 0x0841,
};

// Thermal color mapping and palette management functions
void MLX90640Component::set_thermal_palette(const std::string &palette) {
  thermal_palette_ = palette;
  set_active_palette_();
}

uint16_t MLX90640Component::temp_to_color(float temperature, float min_temp, float max_temp) const {
  // Normalize temperature to 0-255 range
  int color_index = 0;
  if (max_temp > min_temp) {
    float normalized = (temperature - min_temp) / (max_temp - min_temp);
    normalized = (normalized < 0.0f) ? 0.0f : (normalized > 1.0f) ? 1.0f : normalized;
    color_index = (int) (normalized * 255.0f);
  }
  color_index = (color_index < 0) ? 0 : (color_index > 255) ? 255 : color_index;

  // Use active palette instead of hardcoded rainbow (read from PROGMEM)
  return current_palette_ ? pgm_read_word(&current_palette_[color_index])
                          : pgm_read_word(&thermal_palette_rainbow_[color_index]);
}

void MLX90640Component::set_active_palette_() {
  if (thermal_palette_ == "rainbow") {
    current_palette_ = thermal_palette_rainbow_;
  } else if (thermal_palette_ == "golden") {
    current_palette_ = thermal_palette_golden_;
  } else if (thermal_palette_ == "grayscale") {
    current_palette_ = thermal_palette_grayscale_;
  } else if (thermal_palette_ == "ironblack") {
    current_palette_ = thermal_palette_ironblack_;
  } else if (thermal_palette_ == "cam") {
    current_palette_ = thermal_palette_cam_;
  } else if (thermal_palette_ == "ironbow") {
    current_palette_ = thermal_palette_ironbow_;
  } else if (thermal_palette_ == "arctic") {
    current_palette_ = thermal_palette_arctic_;
  } else if (thermal_palette_ == "lava") {
    current_palette_ = thermal_palette_lava_;
  } else if (thermal_palette_ == "whitehot") {
    current_palette_ = thermal_palette_whitehot_;
  } else if (thermal_palette_ == "blackhot") {
    current_palette_ = thermal_palette_blackhot_;
  } else {
    current_palette_ = thermal_palette_rainbow_;  // Default fallback
  }

  ESP_LOGCONFIG(TAG, "MLX90640 color palette set to: %s", thermal_palette_.c_str());
}

// ROI overlay coordinate calculation methods (hardware-agnostic)
bool MLX90640Component::get_roi_overlay_bounds(int image_x, int image_y, int image_w, int image_h, int &roi_x1,
                                               int &roi_y1, int &roi_x2, int &roi_y2) const {
  if (!roi_config_.enabled || !initialized_) {
    return false;
  }

  // Calculate ROI bounds in original 32x24 thermal data coordinate system
  int min_row, max_row, min_col, max_col;
  calculate_roi_bounds_(roi_config_.center_row, roi_config_.center_col, roi_config_.size, min_row, max_row, min_col,
                        max_col);

  // Scale ROI bounds to display coordinates (thermal image is 64x48 interpolated, but we map to original 32x24 grid)
  // We need to map from 32x24 coordinates to the display coordinates
  roi_x1 = image_x + (min_col * image_w / 32);
  roi_y1 = image_y + (min_row * image_h / 24);
  roi_x2 = image_x + ((max_col + 1) * image_w / 32);
  roi_y2 = image_y + ((max_row + 1) * image_h / 24);

  return true;
}

bool MLX90640Component::get_roi_crosshair_coords(int image_x, int image_y, int image_w, int image_h, int &center_x,
                                                 int &center_y) const {
  if (!roi_config_.enabled || !initialized_) {
    return false;
  }

  // Calculate crosshair center position at ROI center point
  center_x = image_x + ((roi_config_.center_col - 1) * image_w / 32) +
             (image_w / 64);  // Convert to display coords with half-pixel offset
  center_y = image_y + ((roi_config_.center_row - 1) * image_h / 24) +
             (image_h / 48);  // Convert to display coords with half-pixel offset

  return true;
}

#ifdef USE_NETWORK
// Web server JPEG generation implementation
void MLX90640Component::setup_web_server_() {
  ESP_LOGCONFIG(TAG, "Setting up web server endpoint at %s", web_server_path_.c_str());

  // Check if web server base instance was provided
  if (!base_) {
    ESP_LOGW(TAG, "WebServerBase not available, thermal image endpoint not registered");
    return;
  }

  // Ensure the web server base is initialized
  base_->init();

  // Check if AsyncWebServer instance is ready
  auto server = base_->get_server();
  if (!server) {
    ESP_LOGW(TAG, "AsyncWebServer not ready, thermal image endpoint not registered");
    return;
  }

  // Now safe to register our handler
  server->on(web_server_path_.c_str(), HTTP_GET,
             [this](AsyncWebServerRequest *request) { this->handle_thermal_image_request_(request); });

  ESP_LOGCONFIG(TAG, "Thermal image endpoint registered successfully");
}

void MLX90640Component::handle_thermal_image_request_(AsyncWebServerRequest *request) {
  if (!initialized_) {
    ESP_LOGW(TAG, "Thermal camera not initialized, cannot serve image");
    request->send(503, "text/plain", "Thermal camera not ready");
    return;
  }

  ESP_LOGD(TAG, "Generating JPEG using JPEGENC library");
  generate_jpg_jpegenc_(request, 160, 120, web_server_quality_);
}

void MLX90640Component::generate_jpg_jpegenc_(AsyncWebServerRequest *request, int width, int height, int quality) {
  JPEGENC jpg;
  JPEGENCODE jpe;

  // Use smaller resolution to avoid heap exhaustion
  int img_width = std::min(width, 160);  // Max 160x120 to save memory
  int img_height = std::min(height, 120);
  ESP_LOGD(TAG, "Generating %dx%d thermal JPEG (requested %dx%d)", img_width, img_height, width, height);

  // Create image data with reduced memory footprint
  std::vector<uint16_t> image_data(img_width * img_height);
  for (int y = 0; y < img_height; y++) {
    for (int x = 0; x < img_width; x++) {
      // Map to thermal data
      int thermal_x = (x * 64) / img_width;
      int thermal_y = (y * 48) / img_height;
      int pixel_idx = thermal_y * 64 + thermal_x;

      uint16_t color = 0x0000;
      if (pixel_idx >= 0 && pixel_idx < (64 * 48)) {
        float temperature = interpolated_pixels_[pixel_idx];
        color = temp_to_color(temperature, min_temp_, max_temp_);
      }

      image_data[y * img_width + x] = color;
    }
  }

  // Add overlays if enabled
  if (web_overlay_enabled_) {
    add_roi_overlay_to_image_(image_data, img_width, img_height);
    add_temperature_text_to_image_(image_data, img_width, img_height);
  }

  // Create smaller JPEG buffer to save memory
  uint8_t *jpg_buffer = (uint8_t *) malloc(16384);  // 16KB buffer
  if (!jpg_buffer) {
    ESP_LOGE(TAG, "Failed to allocate JPEG buffer");
    request->send(500, "text/plain", "Memory allocation failed");
    return;
  }

  int result = jpg.open(jpg_buffer, 16384);
  if (result != JPEGE_SUCCESS) {
    ESP_LOGE(TAG, "Failed to open JPEG encoder");
    free(jpg_buffer);
    request->send(500, "text/plain", "JPEG encoder failed");
    return;
  }

  memset(&jpe, 0, sizeof(JPEGENCODE));
  int jpeg_quality = (quality >= 90)   ? JPEGE_Q_BEST
                     : (quality >= 75) ? JPEGE_Q_HIGH
                     : (quality >= 50) ? JPEGE_Q_MED
                                       : JPEGE_Q_LOW;

  result = jpg.encodeBegin(&jpe, img_width, img_height, JPEGE_PIXEL_RGB565, JPEGE_SUBSAMPLE_420, jpeg_quality);
  if (result != JPEGE_SUCCESS) {
    ESP_LOGE(TAG, "Failed to begin JPEG encoding");
    free(jpg_buffer);
    request->send(500, "text/plain", "JPEG encoding failed");
    return;
  }

  result = jpg.addFrame(&jpe, (uint8_t *) image_data.data(), img_width * sizeof(uint16_t));
  if (result != JPEGE_SUCCESS) {
    ESP_LOGE(TAG, "Failed to add frame data");
    free(jpg_buffer);
    request->send(500, "text/plain", "JPEG frame failed");
    return;
  }

  int jpeg_size = jpg.close();
  ESP_LOGD(TAG, "JPEG created in memory: %d bytes", jpeg_size);

  if (jpeg_size > 0) {
    // Serve JPEG directly from memory
    AsyncWebServerResponse *response = request->beginResponse_P(200, "image/jpeg", jpg_buffer, jpeg_size);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "0");
    request->send(response);
    ESP_LOGD(TAG, "JPEG served directly from memory: %d bytes", jpeg_size);
  } else {
    ESP_LOGE(TAG, "JPEG generation failed");
    request->send(500, "text/plain", "JPEG generation failed");
  }

  free(jpg_buffer);
}

void MLX90640Component::add_roi_overlay_to_image_(std::vector<uint16_t> &image_data, int img_width, int img_height) {
  if (!roi_config_.enabled || !initialized_) {
    return;
  }

  // Get ROI overlay bounds for the full image dimensions
  int roi_display_x1, roi_display_y1, roi_display_x2, roi_display_y2;
  if (!get_roi_overlay_bounds(0, 0, img_width, img_height, roi_display_x1, roi_display_y1, roi_display_x2,
                              roi_display_y2)) {
    return;
  }

  int roi_width = roi_display_x2 - roi_display_x1;
  int roi_height = roi_display_y2 - roi_display_y1;

  // Use bright cyan color (RGB565) for good contrast against thermal colors
  uint16_t roi_color = 0x07FF;  // Bright cyan (RGB565)

  // Draw ROI rectangle border
  // Top and bottom horizontal lines
  for (int x = roi_display_x1; x < roi_display_x2 && x < img_width; x++) {
    if (x >= 0) {
      // Top line
      if (roi_display_y1 >= 0 && roi_display_y1 < img_height) {
        image_data[roi_display_y1 * img_width + x] = roi_color;
      }
      // Bottom line
      if (roi_display_y2 - 1 >= 0 && roi_display_y2 - 1 < img_height) {
        image_data[(roi_display_y2 - 1) * img_width + x] = roi_color;
      }
    }
  }

  // Left and right vertical lines
  for (int y = roi_display_y1; y < roi_display_y2 && y < img_height; y++) {
    if (y >= 0) {
      // Left line
      if (roi_display_x1 >= 0 && roi_display_x1 < img_width) {
        image_data[y * img_width + roi_display_x1] = roi_color;
      }
      // Right line
      if (roi_display_x2 - 1 >= 0 && roi_display_x2 - 1 < img_width) {
        image_data[y * img_width + (roi_display_x2 - 1)] = roi_color;
      }
    }
  }

  // Draw a second border 1 pixel inset for better visibility (if roi is large enough)
  if (roi_width > 4 && roi_height > 4) {
    // Top and bottom horizontal lines (inset)
    for (int x = roi_display_x1 + 1; x < roi_display_x2 - 1 && x < img_width; x++) {
      if (x >= 0) {
        // Top line (inset)
        if (roi_display_y1 + 1 >= 0 && roi_display_y1 + 1 < img_height) {
          image_data[(roi_display_y1 + 1) * img_width + x] = roi_color;
        }
        // Bottom line (inset)
        if (roi_display_y2 - 2 >= 0 && roi_display_y2 - 2 < img_height) {
          image_data[(roi_display_y2 - 2) * img_width + x] = roi_color;
        }
      }
    }

    // Left and right vertical lines (inset)
    for (int y = roi_display_y1 + 1; y < roi_display_y2 - 1 && y < img_height; y++) {
      if (y >= 0) {
        // Left line (inset)
        if (roi_display_x1 + 1 >= 0 && roi_display_x1 + 1 < img_width) {
          image_data[y * img_width + (roi_display_x1 + 1)] = roi_color;
        }
        // Right line (inset)
        if (roi_display_x2 - 2 >= 0 && roi_display_x2 - 2 < img_width) {
          image_data[y * img_width + (roi_display_x2 - 2)] = roi_color;
        }
      }
    }
  }

  // Draw crosshairs at center point
  int center_display_x, center_display_y;
  if (get_roi_crosshair_coords(0, 0, img_width, img_height, center_display_x, center_display_y)) {
    int crosshair_size = 6;  // Length of crosshair lines in pixels

    // Horizontal crosshair line
    for (int x = center_display_x - crosshair_size; x <= center_display_x + crosshair_size; x++) {
      if (x >= 0 && x < img_width && center_display_y >= 0 && center_display_y < img_height) {
        image_data[center_display_y * img_width + x] = roi_color;
      }
    }

    // Vertical crosshair line
    for (int y = center_display_y - crosshair_size; y <= center_display_y + crosshair_size; y++) {
      if (y >= 0 && y < img_height && center_display_x >= 0 && center_display_x < img_width) {
        image_data[y * img_width + center_display_x] = roi_color;
      }
    }
  }
}

void MLX90640Component::add_temperature_text_to_image_(std::vector<uint16_t> &image_data, int img_width,
                                                       int img_height) {
  // Display temperature data with format:
  // Line 1: "MIN  MAX  AVG" or "MIN  MAX  AVG ROI"
  // Line 2: "23.3 40.4 34.5 C"

  uint16_t text_color = 0xFFFF;  // White in RGB565
  int char_width = 4;
  int char_height = 7;
  int char_spacing = 1;

  // Helper functions for drawing
  auto draw_vline = [&](int x, int y, int height) {
    for (int i = 0; i < height && (y + i) < img_height; i++) {
      if (x >= 0 && x < img_width && (y + i) >= 0) {
        image_data[(y + i) * img_width + x] = text_color;
      }
    }
  };

  auto draw_hline = [&](int x, int y, int width) {
    for (int i = 0; i < width && (x + i) < img_width; i++) {
      if ((x + i) >= 0 && y >= 0 && y < img_height) {
        image_data[y * img_width + (x + i)] = text_color;
      }
    }
  };

  auto draw_pixel = [&](int x, int y) {
    if (x >= 0 && x < img_width && y >= 0 && y < img_height) {
      image_data[y * img_width + x] = text_color;
    }
  };

  // Character renderer - draws character at given position
  auto draw_char = [&](char c, int x, int y) {
    switch (c) {
      case '0':
        draw_hline(x, y, 3);
        draw_hline(x, y + 6, 3);
        draw_vline(x, y + 1, 5);
        draw_vline(x + 2, y + 1, 5);
        break;
      case '1':
        draw_vline(x + 1, y, 7);
        draw_pixel(x, y + 1);
        break;
      case '2':
        draw_hline(x, y, 3);
        draw_hline(x, y + 3, 3);
        draw_hline(x, y + 6, 3);
        draw_pixel(x + 2, y + 1);
        draw_pixel(x + 2, y + 2);
        draw_pixel(x, y + 4);
        draw_pixel(x, y + 5);
        break;
      case '3':
        draw_hline(x, y, 3);
        draw_hline(x, y + 3, 3);
        draw_hline(x, y + 6, 3);
        draw_pixel(x + 2, y + 1);
        draw_pixel(x + 2, y + 2);
        draw_pixel(x + 2, y + 4);
        draw_pixel(x + 2, y + 5);
        break;
      case '4':
        draw_vline(x, y, 4);
        draw_vline(x + 2, y, 7);
        draw_hline(x, y + 3, 3);
        break;
      case '5':
        draw_hline(x, y, 3);
        draw_hline(x, y + 3, 3);
        draw_hline(x, y + 6, 3);
        draw_pixel(x, y + 1);
        draw_pixel(x, y + 2);
        draw_pixel(x + 2, y + 4);
        draw_pixel(x + 2, y + 5);
        break;
      case '6':
        draw_hline(x, y, 3);
        draw_hline(x, y + 3, 3);
        draw_hline(x, y + 6, 3);
        draw_vline(x, y + 1, 5);
        draw_pixel(x + 2, y + 4);
        draw_pixel(x + 2, y + 5);
        break;
      case '7':
        draw_hline(x, y, 3);
        draw_vline(x + 2, y + 1, 6);
        break;
      case '8':
        draw_hline(x, y, 3);
        draw_hline(x, y + 3, 3);
        draw_hline(x, y + 6, 3);
        draw_vline(x, y + 1, 5);
        draw_vline(x + 2, y + 1, 5);
        break;
      case '9':
        draw_hline(x, y, 3);
        draw_hline(x, y + 3, 3);
        draw_hline(x, y + 6, 3);
        draw_pixel(x, y + 1);
        draw_pixel(x, y + 2);
        draw_vline(x + 2, y + 1, 5);
        break;
      case '.':
        draw_pixel(x + 1, y + 6);
        break;
      case '-':
        draw_hline(x, y + 3, 3);
        break;
      case 'M':
        draw_vline(x, y, 7);
        draw_vline(x + 3, y, 7);
        draw_pixel(x + 1, y + 1);
        draw_pixel(x + 2, y + 1);
        break;
      case 'I':
        draw_hline(x, y, 3);
        draw_hline(x, y + 6, 3);
        draw_vline(x + 1, y + 1, 5);
        break;
      case 'N':
        draw_vline(x, y, 7);
        draw_vline(x + 3, y, 7);
        draw_pixel(x + 1, y + 2);
        draw_pixel(x + 2, y + 4);
        break;
      case 'A':
        draw_hline(x, y, 3);
        draw_hline(x, y + 3, 3);
        draw_vline(x, y + 1, 6);
        draw_vline(x + 2, y + 1, 6);
        break;
      case 'X':
        draw_pixel(x, y);
        draw_pixel(x + 2, y);
        draw_pixel(x + 1, y + 1);
        draw_pixel(x + 1, y + 2);
        draw_pixel(x + 1, y + 3);
        draw_pixel(x + 1, y + 4);
        draw_pixel(x + 1, y + 5);
        draw_pixel(x, y + 6);
        draw_pixel(x + 2, y + 6);
        break;
      case 'V':
        draw_vline(x, y, 5);
        draw_vline(x + 2, y, 5);
        draw_pixel(x + 1, y + 5);
        draw_pixel(x + 1, y + 6);
        break;
      case 'G':
        draw_hline(x, y, 3);
        draw_hline(x, y + 6, 3);
        draw_hline(x + 1, y + 3, 2);
        draw_vline(x, y + 1, 5);
        draw_pixel(x + 2, y + 4);
        draw_pixel(x + 2, y + 5);
        break;
      case 'C':
        draw_hline(x, y, 3);
        draw_hline(x, y + 6, 3);
        draw_vline(x, y + 1, 5);
        break;
      case 'R':
        draw_hline(x, y, 3);
        draw_hline(x, y + 3, 3);
        draw_vline(x, y, 7);
        draw_pixel(x + 2, y + 1);
        draw_pixel(x + 2, y + 2);
        draw_pixel(x + 1, y + 4);
        draw_pixel(x + 2, y + 5);
        draw_pixel(x + 2, y + 6);
        break;
      case 'O':
        draw_hline(x, y, 3);
        draw_hline(x, y + 6, 3);
        draw_vline(x, y + 1, 5);
        draw_vline(x + 2, y + 1, 5);
        break;
      case ' ':
        // Space - do nothing
        break;
    }
  };

  // Get temperature data based on ROI status
  float min_temp, max_temp, avg_temp;
  bool show_roi = roi_config_.enabled;

  if (show_roi) {
    min_temp = get_roi_min_temp();
    max_temp = get_roi_max_temp();
    avg_temp = get_roi_avg_temp();
  } else {
    min_temp = get_min_temp();
    max_temp = get_max_temp();
    avg_temp = get_avg_temp();
  }

  // Format temperatures to 1 decimal place strings
  auto format_temp = [](float temp) -> char * {
    static char buffer[8];
    int temp_int = (int) (temp * 10);
    if (temp_int < 0) {
      temp_int = -temp_int;
      snprintf(buffer, sizeof(buffer), "-%d.%d", temp_int / 10, temp_int % 10);
    } else {
      snprintf(buffer, sizeof(buffer), "%d.%d", temp_int / 10, temp_int % 10);
    }
    return buffer;
  };

  // Position text at top right of image
  int line1_y = 2;   // Headers line (2 pixels from top)
  int line2_y = 12;  // Temperature values line (12 pixels from top)

  // Right-aligned columns starting from right edge
  int col4_x = img_width - 18;  // ROI or rightmost position (18 pixels from right edge)
  int col3_x = img_width - 38;  // AVG (38 pixels from right edge)
  int col2_x = img_width - 58;  // MAX (58 pixels from right edge)
  int col1_x = img_width - 78;  // MIN (78 pixels from right edge)

  // Draw "MIN"
  draw_char('M', col1_x, line1_y);
  draw_char('I', col1_x + 5, line1_y);
  draw_char('N', col1_x + 9, line1_y);

  // Draw "MAX"
  draw_char('M', col2_x, line1_y);
  draw_char('A', col2_x + 5, line1_y);
  draw_char('X', col2_x + 9, line1_y);

  // Draw "AVG"
  draw_char('A', col3_x, line1_y);
  draw_char('V', col3_x + 5, line1_y);
  draw_char('G', col3_x + 9, line1_y);

  // Draw "ROI" if enabled
  if (show_roi) {
    draw_char('R', col4_x, line1_y);
    draw_char('O', col4_x + 5, line1_y);
    draw_char('I', col4_x + 9, line1_y);
  }

  // Draw min temp
  char *min_str = format_temp(min_temp);
  int min_x = col1_x;
  for (int i = 0; min_str[i] != '\0'; i++) {
    draw_char(min_str[i], min_x + i * 4, line2_y);
  }

  // Draw max temp
  char *max_str = format_temp(max_temp);
  int max_x = col2_x;
  for (int i = 0; max_str[i] != '\0'; i++) {
    draw_char(max_str[i], max_x + i * 4, line2_y);
  }

  // Draw avg temp
  char *avg_str = format_temp(avg_temp);
  int avg_x = col3_x;
  for (int i = 0; avg_str[i] != '\0'; i++) {
    draw_char(avg_str[i], avg_x + i * 4, line2_y);
  }

  // Draw "C" at the rightmost position (after the values)
  draw_char('C', col4_x, line2_y);
}
#endif  // USE_NETWORK

// State synchronization - sync internal roi_config_ with control entity values
void MLX90640Component::sync_roi_state_from_controls() {
  ESP_LOGD(TAG, "Syncing ROI state from control entities");

  // Sync ROI enabled state from switch control
  if (roi_enabled_control_ != nullptr) {
    roi_config_.enabled = roi_enabled_control_->state;
    ESP_LOGD(TAG, "Synced ROI enabled: %s", roi_config_.enabled ? "true" : "false");
  }

  // Sync ROI center row from number control
  if (roi_center_row_control_ != nullptr) {
    int row = static_cast<int>(roi_center_row_control_->state);
    if (row >= 1 && row <= 24) {
      roi_config_.center_row = row;
      ESP_LOGD(TAG, "Synced ROI center row: %d", roi_config_.center_row);
    }
  }

  // Sync ROI center column from number control
  if (roi_center_col_control_ != nullptr) {
    int col = static_cast<int>(roi_center_col_control_->state);
    if (col >= 1 && col <= 32) {
      roi_config_.center_col = col;
      ESP_LOGD(TAG, "Synced ROI center col: %d", roi_config_.center_col);
    }
  }

  // Sync ROI size from number control
  if (roi_size_control_ != nullptr) {
    int size = static_cast<int>(roi_size_control_->state);
    if (size >= 1 && size <= 10) {
      roi_config_.size = size;
      ESP_LOGD(TAG, "Synced ROI size: %d", roi_config_.size);
    }
  }

  ESP_LOGD(TAG, "ROI state sync complete - enabled=%s, center=(%d,%d), size=%d", roi_config_.enabled ? "true" : "false",
           roi_config_.center_row, roi_config_.center_col, roi_config_.size);
}

// MLX90640Number implementation
void MLX90640Number::setup() {
  float value;
  if (!restore_value_) {
    value = initial_value_;
  } else {
    this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
    if (!this->pref_.load(&value)) {
      if (!std::isnan(this->initial_value_)) {
        value = this->initial_value_;
      } else {
        value = this->traits.get_min_value();
      }
    }
  }
  if (!std::isnan(value)) {
    this->publish_state(value);
  }
}

// MLX90640Select implementation
void MLX90640Select::setup() {
  std::string value;
  if (!this->restore_value_) {
    value = this->initial_option_;
  } else {
    size_t index;
    this->pref_ = global_preferences->make_preference<size_t>(this->get_object_id_hash());
    if (!this->pref_.load(&index)) {
      value = this->initial_option_;
    } else if (!this->has_index(index)) {
      value = this->initial_option_;
    } else {
      value = this->at(index).value();
    }
  }

  if (!value.empty()) {
    this->control(value);
  }
}

// MLX90640Switch implementation
void MLX90640Switch::setup() {
  optional<bool> initial_state = this->get_initial_state_with_restore_mode();

  if (initial_state.has_value()) {
    const char *control_name = (control_type_ == WEB_OVERLAY_ENABLED) ? "Web overlay enabled" : "ROI enabled";
    ESP_LOGD("MLX90640Switch", "Restored %s state: %s", control_name, initial_state.value() ? "ON" : "OFF");
    if (parent_ != nullptr) {
      if (control_type_ == WEB_OVERLAY_ENABLED) {
        parent_->update_web_overlay_enabled(initial_state.value());
      } else {
        parent_->update_roi_enabled(initial_state.value());
      }
    }
    this->publish_state(initial_state.value());
  }
}

// MLX90640Number implementation
void MLX90640Number::control(float value) {
  if (parent_ == nullptr) {
    ESP_LOGE("MLX90640Number", "Parent component not set");
    return;
  }

  switch (control_type_) {
    case UPDATE_INTERVAL:
      parent_->set_update_interval((uint32_t) value);
      ESP_LOGD("MLX90640Number", "Update interval changed to %d ms", (uint32_t) value);
      break;

    case ROI_CENTER_ROW:
      parent_->update_roi_center_row((int) value);
      ESP_LOGD("MLX90640Number", "ROI center row changed to %d", (int) value);
      break;

    case ROI_CENTER_COL:
      parent_->update_roi_center_col((int) value);
      ESP_LOGD("MLX90640Number", "ROI center column changed to %d", (int) value);
      break;

    case ROI_SIZE:
      parent_->update_roi_size((int) value);
      ESP_LOGD("MLX90640Number", "ROI size changed to %d", (int) value);
      break;

    default:
      ESP_LOGE("MLX90640Number", "Unknown control type");
      return;
  }

  // Update the UI state
  publish_state(value);

  if (this->restore_value_)
    this->pref_.save(&value);
}

// MLX90640Select implementation
void MLX90640Select::control(const std::string &value) {
  if (parent_ == nullptr) {
    ESP_LOGE("MLX90640Select", "Parent component not set");
    return;
  }

  parent_->set_thermal_palette(value);
  ESP_LOGD("MLX90640Select", "Thermal palette changed to %s", value.c_str());

  // Update the UI state
  publish_state(value);

  if (this->restore_value_) {
    auto index = this->index_of(value);
    if (index.has_value()) {
      this->pref_.save(&index.value());
    }
  }
}

// MLX90640Switch implementation
void MLX90640Switch::write_state(bool state) {
  if (parent_ == nullptr) {
    ESP_LOGE("MLX90640Switch", "Parent component not set");
    return;
  }

  if (control_type_ == WEB_OVERLAY_ENABLED) {
    parent_->update_web_overlay_enabled(state);
    ESP_LOGD("MLX90640Switch", "Web overlay enabled changed to %s", state ? "true" : "false");
  } else {
    parent_->update_roi_enabled(state);
    ESP_LOGD("MLX90640Switch", "ROI enabled changed to %s", state ? "true" : "false");
  }

  // Update the UI state
  publish_state(state);
}

}  // namespace mlx90640
}  // namespace esphome
