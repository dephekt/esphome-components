#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/i2c/i2c.h"
#ifdef USE_NETWORK
#include "esphome/components/web_server_base/web_server_base.h"
#endif
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"
#include <Wire.h>
#include <algorithm>
#ifdef USE_NETWORK
// Always include JPEGENC for JPEG generation
#include <JPEGENC.h>
// Simple text rendering without external dependencies
#endif

namespace esphome {
namespace mlx90640 {

struct ROIConfig {
  bool enabled = false;
  int center_row = 12;  // 1-24 user range
  int center_col = 16;  // 1-32 user range
  int size = 2;         // ROI scaling factor
};

class MLX90640Component : public Component, public i2c::I2CDevice {
 public:
  MLX90640Component() {
#ifdef USE_NETWORK
    base_ = nullptr;
#endif
  }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI - 1.0f; }

  // Configuration setters
  void set_refresh_rate(const std::string &rate) { refresh_rate_ = rate; }
  void set_resolution(const std::string &resolution) { resolution_ = resolution; }
  void set_pattern(const std::string &pattern) { pattern_ = pattern; }
  void set_single_frame(bool single_frame) { single_frame_ = single_frame; }
  void set_update_interval(uint32_t interval) { update_interval_ = interval; }
  uint32_t get_update_interval() const { return update_interval_; }

  // ROI configuration
  void set_roi_config(const ROIConfig &config) { roi_config_ = config; }
  void update_roi_enabled(bool enabled) { roi_config_.enabled = enabled; }
  void update_roi_center_row(int row) {
    if (row >= 1 && row <= 24)
      roi_config_.center_row = row;
  }
  void update_roi_center_col(int col) {
    if (col >= 1 && col <= 32)
      roi_config_.center_col = col;
  }
  void update_roi_size(int size) {
    if (size >= 1 && size <= 10)
      roi_config_.size = size;
  }

#ifdef USE_NETWORK
  // Web server configuration
  void set_web_server_enabled(bool enabled) { web_server_enabled_ = enabled; }
  void set_web_server_path(const std::string &path) { web_server_path_ = path; }
  void set_web_server_quality(int quality) { web_server_quality_ = quality; }
  void set_web_overlay_enabled(bool enabled) { web_overlay_enabled_ = enabled; }
  void update_web_overlay_enabled(bool enabled) { web_overlay_enabled_ = enabled; }
  bool is_web_overlay_enabled() const { return web_overlay_enabled_; }
  void set_web_server_base(web_server_base::WebServerBase *base) { base_ = base; }
#endif

  // Sensor setters
  void set_temperature_min_sensor(sensor::Sensor *sensor) { temp_min_sensor_ = sensor; }
  void set_temperature_max_sensor(sensor::Sensor *sensor) { temp_max_sensor_ = sensor; }
  void set_temperature_avg_sensor(sensor::Sensor *sensor) { temp_avg_sensor_ = sensor; }
  void set_roi_min_sensor(sensor::Sensor *sensor) { roi_min_sensor_ = sensor; }
  void set_roi_max_sensor(sensor::Sensor *sensor) { roi_max_sensor_ = sensor; }
  void set_roi_avg_sensor(sensor::Sensor *sensor) { roi_avg_sensor_ = sensor; }

  // Auto-generated control entity setters
  void set_update_interval_control(number::Number *control) { update_interval_control_ = control; }
  void set_thermal_palette_control(select::Select *control) { thermal_palette_control_ = control; }
  void set_roi_enabled_control(switch_::Switch *control) { roi_enabled_control_ = control; }
  void set_roi_center_row_control(number::Number *control) { roi_center_row_control_ = control; }
  void set_roi_center_col_control(number::Number *control) { roi_center_col_control_ = control; }
  void set_roi_size_control(number::Number *control) { roi_size_control_ = control; }
  void set_web_overlay_enabled_control(switch_::Switch *control) { web_overlay_enabled_control_ = control; }

  // Data access methods for external components
  bool is_initialized() const { return initialized_; }
  const float *get_thermal_pixels() const { return mlx90640_pixels_; }
  const float *get_interpolated_pixels() const { return interpolated_pixels_; }

  // Temperature data getters
  float get_min_temp() const { return min_temp_; }
  float get_max_temp() const { return max_temp_; }
  float get_avg_temp() const { return avg_temp_; }
  float get_median_temp() const { return median_temp_; }

  // ROI data getters
  bool is_roi_enabled() const { return roi_config_.enabled; }
  float get_roi_min_temp() const { return roi_min_temp_; }
  float get_roi_max_temp() const { return roi_max_temp_; }
  float get_roi_avg_temp() const { return roi_avg_temp_; }
  float get_roi_median_temp() const { return roi_median_temp_; }
  int get_roi_pixel_count() const { return roi_pixel_count_; }
  const ROIConfig &get_roi_config() const { return roi_config_; }

  // Thermal color mapping
  void set_thermal_palette(const std::string &palette);
  const std::string &get_thermal_palette() const { return thermal_palette_; }
  uint16_t temp_to_color(float temperature, float min_temp, float max_temp) const;

  // ROI overlay coordinate calculation (hardware-agnostic)
  bool get_roi_overlay_bounds(int image_x, int image_y, int image_w, int image_h, int &roi_x1, int &roi_y1, int &roi_x2,
                              int &roi_y2) const;
  bool get_roi_crosshair_coords(int image_x, int image_y, int image_w, int image_h, int &center_x, int &center_y) const;

  // State synchronization - sync internal roi_config_ with control entity values
  void sync_roi_state_from_controls();

#ifdef USE_NETWORK
  // Web server thermal image handler
  void handle_thermal_image_request_(AsyncWebServerRequest *request);
#endif

 protected:
  void setup_thermal_();
  void update_thermal_data_();
  void calculate_roi_bounds_(int center_row, int center_col, int size, int &min_row, int &max_row, int &min_col,
                             int &max_col) const;
  void process_roi_temperatures_();

  // Configuration helpers
  uint16_t parse_refresh_rate_(const std::string &rate_str);
  int parse_resolution_(const std::string &res_str);
  int setup_thermal_resolution_(int bits);
  int setup_thermal_pattern_(const std::string &pattern);
  void set_active_palette_();

  // Thermal interpolation for smooth upscaling (optional, for display purposes)
  void interpolate_image_(float *src, uint8_t src_rows, uint8_t src_cols, float *dest, uint8_t dest_rows,
                          uint8_t dest_cols);
  float get_point_(float *p, uint8_t rows, uint8_t cols, int8_t x, int8_t y);
  void set_point_(float *p, uint8_t rows, uint8_t cols, int8_t x, int8_t y, float f);
  void get_adjacents_2d_(float *src, float *dest, uint8_t rows, uint8_t cols, int8_t x, int8_t y);
  float cubic_interpolate_(float p[], float x);
  float bicubic_interpolate_(float p[], float x, float y);

#ifdef USE_NETWORK
  // Web server JPEG generation
  void setup_web_server_();
  void generate_jpg_jpegenc_(AsyncWebServerRequest *request, int width, int height, int quality);
  void add_roi_overlay_to_image_(std::vector<uint16_t> &image_data, int img_width, int img_height);
  void add_temperature_text_to_image_(std::vector<uint16_t> &image_data, int img_width, int img_height);
#endif

  // Hardware configuration
  static const int TA_SHIFT = 8;

  // Configuration
  std::string refresh_rate_{"16Hz"};
  std::string resolution_{"18-bit"};
  std::string pattern_{"chess"};
  bool single_frame_{false};
  uint32_t update_interval_{20000};  // 20 seconds default
  ROIConfig roi_config_;

  // Thermal palette configuration
  std::string thermal_palette_{"rainbow"};
  const uint16_t *current_palette_{nullptr};

#ifdef USE_NETWORK
  // Web server configuration
  bool web_server_enabled_{false};
  std::string web_server_path_{"/thermal.jpg"};
  int web_server_quality_{85};
  bool web_overlay_enabled_{true};
  web_server_base::WebServerBase *base_;
#endif

  // Hardware state
  bool initialized_{false};
  paramsMLX90640 mlx90640_params_;

  // Data buffers (class members to prevent stack overflow)
  float mlx90640_pixels_[32 * 24];      // Raw thermal data
  float interpolated_pixels_[64 * 48];  // 2x upscaled thermal data
  uint16_t mlx90640Frame_[834];         // MLX90640 frame buffer
  float valid_pixels_[768];             // Valid pixel buffer for sorting
  float adj_2d_[16];                    // Adjacent matrix for interpolation

  // Temperature statistics
  float min_temp_{20.0};
  float max_temp_{30.0};
  float avg_temp_{25.0};
  float median_temp_{25.0};

  // ROI statistics
  float roi_min_temp_{20.0};
  float roi_max_temp_{30.0};
  float roi_avg_temp_{25.0};
  float roi_median_temp_{25.0};
  int roi_pixel_count_{0};

  // Sensors (optional - for publishing to ESPHome)
  sensor::Sensor *temp_min_sensor_{nullptr};
  sensor::Sensor *temp_max_sensor_{nullptr};
  sensor::Sensor *temp_avg_sensor_{nullptr};
  sensor::Sensor *roi_min_sensor_{nullptr};
  sensor::Sensor *roi_max_sensor_{nullptr};
  sensor::Sensor *roi_avg_sensor_{nullptr};

  // Auto-generated control entities (optional)
  number::Number *update_interval_control_{nullptr};
  select::Select *thermal_palette_control_{nullptr};
  switch_::Switch *roi_enabled_control_{nullptr};
  number::Number *roi_center_row_control_{nullptr};
  number::Number *roi_center_col_control_{nullptr};
  number::Number *roi_size_control_{nullptr};
  switch_::Switch *web_overlay_enabled_control_{nullptr};

  // Timing
  uint32_t last_update_time_{0};

  // Thermal color palettes (moved to PROGMEM to save RAM)
  static const uint16_t thermal_palette_rainbow_[256] PROGMEM;
  static const uint16_t thermal_palette_golden_[256] PROGMEM;
  static const uint16_t thermal_palette_grayscale_[256] PROGMEM;
  static const uint16_t thermal_palette_ironblack_[256] PROGMEM;
  static const uint16_t thermal_palette_cam_[256] PROGMEM;
  static const uint16_t thermal_palette_ironbow_[256] PROGMEM;
  static const uint16_t thermal_palette_arctic_[256] PROGMEM;
  static const uint16_t thermal_palette_lava_[256] PROGMEM;
  static const uint16_t thermal_palette_whitehot_[256] PROGMEM;
  static const uint16_t thermal_palette_blackhot_[256] PROGMEM;
};

// Control type enum for MLX90640 components
enum MLX90640ControlType {
  UPDATE_INTERVAL,
  ROI_CENTER_ROW,
  ROI_CENTER_COL,
  ROI_SIZE,
  THERMAL_PALETTE,
  ROI_ENABLED,
  WEB_OVERLAY_ENABLED
};

// MLX90640Number - handles numeric controls (update interval, ROI settings)
class MLX90640Number : public number::Number, public Component {
 public:
  void set_mlx90640_parent(MLX90640Component *parent) { parent_ = parent; }
  void set_control_type(MLX90640ControlType type) { control_type_ = type; }

  // Configuration methods (similar to template number)
  void set_restore_value(bool restore_value) { restore_value_ = restore_value; }
  void set_initial_value(float initial_value) { initial_value_ = initial_value; }

  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(float value) override;

 private:
  MLX90640Component *parent_{nullptr};
  MLX90640ControlType control_type_;
  bool restore_value_{false};
  float initial_value_{NAN};
  ESPPreferenceObject pref_;
};

// MLX90640Select - handles thermal palette selection
class MLX90640Select : public select::Select, public Component {
 public:
  void set_mlx90640_parent(MLX90640Component *parent) { parent_ = parent; }

  // Configuration methods (similar to template select)
  void set_restore_value(bool restore_value) { restore_value_ = restore_value; }
  void set_initial_option(const std::string &initial_option) { initial_option_ = initial_option; }

  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(const std::string &value) override;

 private:
  MLX90640Component *parent_{nullptr};
  bool restore_value_{false};
  std::string initial_option_;
  ESPPreferenceObject pref_;
};

// MLX90640Switch - handles ROI and web overlay enabled controls
class MLX90640Switch : public switch_::Switch, public Component {
 public:
  void set_mlx90640_parent(MLX90640Component *parent) { parent_ = parent; }
  void set_control_type(MLX90640ControlType type) { control_type_ = type; }
  void set_restore_mode(switch_::SwitchRestoreMode restore_mode) { this->restore_mode = restore_mode; }

  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void write_state(bool state) override;

 private:
  MLX90640Component *parent_{nullptr};
  MLX90640ControlType control_type_{ROI_ENABLED};
};

}  // namespace mlx90640
}  // namespace esphome
