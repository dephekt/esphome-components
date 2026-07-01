#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/i2c/i2c.h"
#ifdef USE_NETWORK
#include "esphome/components/web_server_base/web_server_base.h"
#endif
#include <algorithm>
#include <cmath>
#ifdef USE_NETWORK
// Always include JPEGENC for JPEG generation
#include <JPEGENC.h>
// Simple text rendering without external dependencies
#endif

namespace esphome {
namespace m5stack_thermal2 {

// M5Stack Unit Thermal2 I2C register map (device answers at 0x32). The onboard
// PICO-D4 does all MLX90640 computation and exposes processed values; we never
// touch the raw Melexis sensor. Temperature conversion: °C = raw / 128 - 64.
static const uint8_t REG_BUTTON = 0x00;          // button state flags (RW: write back to ack)
static const uint8_t REG_ALARM_STATUS = 0x01;    // alarm-fired bitmask (unused: we alarm in software)
static const uint8_t REG_DEVICE_ID0 = 0x04;      // reads 0x90
static const uint8_t REG_DEVICE_ID1 = 0x05;      // reads 0x64
static const uint8_t REG_FUNCTION_CTRL = 0x0A;   // bit0 buzzer_en, bit1 led_en, bit2 auto_refresh
static const uint8_t REG_REFRESH_RATE = 0x0B;    // 0..7 (0.5Hz..64Hz)
static const uint8_t REG_NOISE_FILTER = 0x0C;    // 0..15 (0 = disabled)
static const uint8_t REG_TEMP_MONITOR_AREA = 0x10;
static const uint8_t REG_BUZZER_FREQ = 0x12;     // uint16 LE
static const uint8_t REG_BUZZER_VOLUME = 0x14;   // 0..255
static const uint8_t REG_LED_R = 0x15;           // R,G,B at 0x15,0x16,0x17
static const uint8_t REG_REFRESH_CTRL = 0x6E;    // [0]=data-ready bit0, [1]=current subpage
static const uint8_t REG_OVERVIEW = 0x70;        // 16-byte temperature_reg block
static const uint8_t REG_PIXEL_DATA = 0x80;      // 768 bytes = 384 uint16 (one 16x24 subpage)

static const uint8_t FUNCTION_BUZZER_EN = 0x01;
static const uint8_t FUNCTION_LED_EN = 0x02;
static const uint8_t FUNCTION_AUTO_REFRESH = 0x04;

static const uint8_t DEVICE_ID0_VALUE = 0x90;
static const uint8_t DEVICE_ID1_VALUE = 0x64;

// Button state flags (REG_BUTTON).
static const uint8_t BUTTON_IS_PRESSED = 1 << 0;
static const uint8_t BUTTON_WAS_PRESSED = 1 << 1;
static const uint8_t BUTTON_WAS_RELEASED = 1 << 2;
static const uint8_t BUTTON_WAS_CLICKED = 1 << 3;
static const uint8_t BUTTON_WAS_HOLD = 1 << 4;

// MLX90640 sensor geometry (behind the Thermal2's controller).
static const int THERMAL_ROWS = 24;
static const int THERMAL_COLS = 32;
static const int THERMAL_PIXELS = THERMAL_ROWS * THERMAL_COLS;  // 768
static const int SUBPAGE_PIXELS = THERMAL_PIXELS / 2;           // 384

enum AlarmSource { ALARM_SOURCE_AVERAGE, ALARM_SOURCE_MEDIAN, ALARM_SOURCE_MIN, ALARM_SOURCE_MAX };
enum AlarmRegion { ALARM_REGION_ACTIVE, ALARM_REGION_FRAME, ALARM_REGION_ROI };

struct ROIConfig {
  bool enabled = false;
  int center_row = 12;  // 1-24 user range
  int center_col = 16;  // 1-32 user range
  int size = 2;         // ROI scaling factor: (2*size+1) square
};

class M5Thermal2Component : public Component, public i2c::I2CDevice {
 public:
  M5Thermal2Component() {
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
  void set_noise_filter(uint8_t level) { noise_filter_ = level; }
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

  // Alarm configuration (static, from YAML)
  void set_status_led_enabled(bool enabled) { status_led_enabled_ = enabled; }
  void set_alarm_source(AlarmSource source) { alarm_source_ = source; }
  void set_alarm_region(AlarmRegion region) { alarm_region_ = region; }
  void set_alarm_high_threshold(float t) { alarm_high_threshold_ = t; }
  void set_alarm_low_threshold(float t) { alarm_low_threshold_ = t; }
  void set_alarm_hysteresis(float h) { alarm_hysteresis_ = h; }
  void set_alarm_buzzer_frequency(uint16_t f) { alarm_buzzer_frequency_ = f; }
  void set_alarm_buzzer_volume(uint8_t v) { alarm_buzzer_volume_ = v; }
  void set_alarm_beep_interval(uint32_t ms) { alarm_beep_interval_ = ms; }
  void set_alarm_led_color(uint8_t r, uint8_t g, uint8_t b) {
    alarm_led_r_ = r;
    alarm_led_g_ = g;
    alarm_led_b_ = b;
  }

  // Alarm runtime updates (from controls)
  void update_buzzer_enabled(bool enabled) { buzzer_enabled_ = enabled; }
  bool is_buzzer_enabled() const { return buzzer_enabled_; }
  void update_alarm_high_threshold(float t) { alarm_high_threshold_ = t; }
  void update_alarm_low_threshold(float t) { alarm_low_threshold_ = t; }

#ifdef USE_NETWORK
  // Web server configuration
  void set_web_server_enabled(bool enabled) { web_server_enabled_ = enabled; }
  void set_web_server_path(const std::string &path) { web_server_path_ = path; }
  void set_web_server_quality(int quality) { web_server_quality_ = quality; }
  void set_web_overlay_enabled(bool enabled) { web_overlay_enabled_ = enabled; }
  void update_web_overlay_enabled(bool enabled) { web_overlay_enabled_ = enabled; }
  bool is_web_overlay_enabled() const { return web_overlay_enabled_; }
  void set_web_server_base(web_server_base::WebServerBase *base) { base_ = base; }
  void set_web_html_page_enabled(bool enabled) { web_html_page_enabled_ = enabled; }
#endif

  // Sensor setters
  void set_temperature_min_sensor(sensor::Sensor *sensor) { temp_min_sensor_ = sensor; }
  void set_temperature_max_sensor(sensor::Sensor *sensor) { temp_max_sensor_ = sensor; }
  void set_temperature_avg_sensor(sensor::Sensor *sensor) { temp_avg_sensor_ = sensor; }
  void set_median_sensor(sensor::Sensor *sensor) { median_sensor_ = sensor; }
  void set_roi_min_sensor(sensor::Sensor *sensor) { roi_min_sensor_ = sensor; }
  void set_roi_max_sensor(sensor::Sensor *sensor) { roi_max_sensor_ = sensor; }
  void set_roi_avg_sensor(sensor::Sensor *sensor) { roi_avg_sensor_ = sensor; }
  void set_alarm_active_sensor(binary_sensor::BinarySensor *sensor) { alarm_active_sensor_ = sensor; }
  void set_button_sensor(binary_sensor::BinarySensor *sensor) { button_sensor_ = sensor; }

  // Auto-generated control entity setters
  void set_update_interval_control(number::Number *control) { update_interval_control_ = control; }
  void set_thermal_palette_control(select::Select *control) { thermal_palette_control_ = control; }
  void set_roi_enabled_control(switch_::Switch *control) { roi_enabled_control_ = control; }
  void set_roi_center_row_control(number::Number *control) { roi_center_row_control_ = control; }
  void set_roi_center_col_control(number::Number *control) { roi_center_col_control_ = control; }
  void set_roi_size_control(number::Number *control) { roi_size_control_ = control; }
  void set_web_overlay_enabled_control(switch_::Switch *control) { web_overlay_enabled_control_ = control; }
  void set_buzzer_enabled_control(switch_::Switch *control) { buzzer_enabled_control_ = control; }
  void set_alarm_high_threshold_control(number::Number *control) { alarm_high_threshold_control_ = control; }
  void set_alarm_low_threshold_control(number::Number *control) { alarm_low_threshold_control_ = control; }

  // Data access methods for external components
  bool is_initialized() const { return initialized_; }
  const float *get_thermal_pixels() const { return pixels_; }
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

  bool is_alarm_active() const { return alarm_active_; }

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
  // Web server thermal viewer page (a small HTML page that renders the JPEG)
  void handle_thermal_page_request_(AsyncWebServerRequest *request);
#endif

 protected:
  // Low-level Thermal2 register access
  bool read_reg8_(uint8_t reg, uint8_t *value);
  bool write_reg8_(uint8_t reg, uint8_t value);
  bool write_reg16_(uint8_t reg, uint16_t value);

  bool init_device_();
  bool read_frame_();
  bool read_subpage_pixels_();
  void store_subpage_(const uint16_t *raw, bool subpage);
  void compute_stats_();
  void process_roi_temperatures_();
  void calculate_roi_bounds_(int center_row, int center_col, int size, int &min_row, int &max_row, int &min_col,
                             int &max_col) const;

  // Button + alarm + LED handling
  void handle_button_();
  void evaluate_alarm_();
  float get_alarm_source_temp_() const;
  void drive_alarm_output_();
  void set_led_(uint8_t r, uint8_t g, uint8_t b);
  void set_buzzer_(uint16_t freq);
  void refresh_status_led_();

  // Configuration helpers
  uint8_t parse_refresh_rate_(const std::string &rate_str);
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

  // Configuration
  std::string refresh_rate_{"16Hz"};
  uint8_t noise_filter_{8};
  uint32_t update_interval_{2000};
  ROIConfig roi_config_;

  // Alarm configuration
  bool status_led_enabled_{true};
  AlarmSource alarm_source_{ALARM_SOURCE_AVERAGE};
  AlarmRegion alarm_region_{ALARM_REGION_ACTIVE};
  float alarm_high_threshold_{35.0f};
  float alarm_low_threshold_{5.0f};
  float alarm_hysteresis_{0.5f};
  uint16_t alarm_buzzer_frequency_{4000};
  uint8_t alarm_buzzer_volume_{96};
  uint32_t alarm_beep_interval_{250};
  uint8_t alarm_led_r_{16};
  uint8_t alarm_led_g_{0};
  uint8_t alarm_led_b_{0};

  // Alarm runtime state
  bool buzzer_enabled_{true};
  bool alarm_active_{false};
  bool alarm_high_active_{false};
  bool alarm_low_active_{false};
  bool blink_on_{false};
  uint32_t last_blink_time_{0};

  // Cached hardware output state (avoid redundant I2C writes)
  uint8_t function_ctrl_{0};
  int16_t last_led_r_{-1};
  int16_t last_led_g_{-1};
  int16_t last_led_b_{-1};
  int32_t last_buzzer_freq_{-1};

  // Thermal palette configuration
  std::string thermal_palette_{"rainbow"};
  const uint16_t *current_palette_{nullptr};

#ifdef USE_NETWORK
  // Web server configuration
  bool web_server_enabled_{false};
  std::string web_server_path_{"/thermal.jpg"};
  int web_server_quality_{85};
  bool web_overlay_enabled_{true};
  bool web_html_page_enabled_{true};
  std::string web_html_path_;  // viewer page path, derived from web_server_path_ at setup
  web_server_base::WebServerBase *base_;
#endif

  // Hardware state
  bool initialized_{false};

  // Data buffers (class members to prevent stack overflow)
  float pixels_[THERMAL_PIXELS];        // Assembled thermal frame (32x24), °C
  float interpolated_pixels_[64 * 48];  // Upscaled thermal data for display
  float valid_pixels_[THERMAL_PIXELS];  // Valid pixel buffer for sorting
  uint16_t pixel_raw_[SUBPAGE_PIXELS];  // Raw subpage read buffer
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
  sensor::Sensor *median_sensor_{nullptr};
  sensor::Sensor *roi_min_sensor_{nullptr};
  sensor::Sensor *roi_max_sensor_{nullptr};
  sensor::Sensor *roi_avg_sensor_{nullptr};
  binary_sensor::BinarySensor *alarm_active_sensor_{nullptr};
  binary_sensor::BinarySensor *button_sensor_{nullptr};

  // Auto-generated control entities (optional)
  number::Number *update_interval_control_{nullptr};
  select::Select *thermal_palette_control_{nullptr};
  switch_::Switch *roi_enabled_control_{nullptr};
  number::Number *roi_center_row_control_{nullptr};
  number::Number *roi_center_col_control_{nullptr};
  number::Number *roi_size_control_{nullptr};
  switch_::Switch *web_overlay_enabled_control_{nullptr};
  switch_::Switch *buzzer_enabled_control_{nullptr};
  number::Number *alarm_high_threshold_control_{nullptr};
  number::Number *alarm_low_threshold_control_{nullptr};

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

// Control type enum for M5Thermal2 components
enum M5Thermal2ControlType {
  UPDATE_INTERVAL,
  ROI_CENTER_ROW,
  ROI_CENTER_COL,
  ROI_SIZE,
  THERMAL_PALETTE,
  ROI_ENABLED,
  WEB_OVERLAY_ENABLED,
  BUZZER_ENABLED,
  ALARM_HIGH_THRESHOLD,
  ALARM_LOW_THRESHOLD
};

// M5Thermal2Number - handles numeric controls (update interval, ROI, thresholds)
class M5Thermal2Number : public number::Number, public Component {
 public:
  void set_m5stack_thermal2_parent(M5Thermal2Component *parent) { parent_ = parent; }
  void set_control_type(M5Thermal2ControlType type) { control_type_ = type; }

  // Configuration methods (similar to template number)
  void set_restore_value(bool restore_value) { restore_value_ = restore_value; }
  void set_initial_value(float initial_value) { initial_value_ = initial_value; }

  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(float value) override;

 private:
  M5Thermal2Component *parent_{nullptr};
  M5Thermal2ControlType control_type_;
  bool restore_value_{false};
  float initial_value_{NAN};
  ESPPreferenceObject pref_;
};

// M5Thermal2Select - handles thermal palette selection
class M5Thermal2Select : public select::Select, public Component {
 public:
  void set_m5stack_thermal2_parent(M5Thermal2Component *parent) { parent_ = parent; }

  // Configuration methods (similar to template select)
  void set_restore_value(bool restore_value) { restore_value_ = restore_value; }
  void set_initial_option(const std::string &initial_option) { initial_option_ = initial_option; }

  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(const std::string &value) override;

 private:
  M5Thermal2Component *parent_{nullptr};
  bool restore_value_{false};
  std::string initial_option_;
  ESPPreferenceObject pref_;
};

// M5Thermal2Switch - handles ROI, web overlay, and buzzer controls
class M5Thermal2Switch : public switch_::Switch, public Component {
 public:
  void set_m5stack_thermal2_parent(M5Thermal2Component *parent) { parent_ = parent; }
  void set_control_type(M5Thermal2ControlType type) { control_type_ = type; }
  void set_restore_mode(switch_::SwitchRestoreMode restore_mode) { this->restore_mode = restore_mode; }

  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void write_state(bool state) override;

 private:
  M5Thermal2Component *parent_{nullptr};
  M5Thermal2ControlType control_type_{ROI_ENABLED};
};

}  // namespace m5stack_thermal2
}  // namespace esphome
