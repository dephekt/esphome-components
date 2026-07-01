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
#include <algorithm>
#include <cmath>
#ifdef USE_NETWORK
// Always include JPEGENC for JPEG generation
#include <JPEGENC.h>
// Simple text rendering without external dependencies
#endif

namespace esphome {
namespace thermal_camera_core {

// Shared thermal-frame geometry (24x32 MLX90640-derived sensors: the M5Stack
// Thermal2's onboard PICO-D4 and the bare MLX90640 both expose this grid).
struct ROIConfig {
  bool enabled = false;
  int center_row = 12;  // 1-24 user range
  int center_col = 16;  // 1-32 user range
  int size = 2;         // ROI scaling factor: (2*size+1) square
};

// Base class for thermal-camera components (M5Stack Thermal2, bare MLX90640,
// ...). Owns the frame buffers, temperature statistics, ROI math, palette
// mapping, and the optional /thermal.jpg web endpoint. Device-specific I/O
// and any device-only subsystems (alarms, buttons, LEDs, ...) live in the
// subclass, which plugs into the pipeline below through the virtual seams.
class ThermalCameraBase : public Component, public i2c::I2CDevice {
 public:
  static constexpr int THERMAL_ROWS = 24;
  static constexpr int THERMAL_COLS = 32;
  static constexpr int THERMAL_PIXELS = THERMAL_ROWS * THERMAL_COLS;  // 768
  static constexpr int SUBPAGE_PIXELS = THERMAL_PIXELS / 2;           // 384

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI - 1.0f; }

  // Configuration setters
  virtual void set_update_interval(uint32_t interval) { update_interval_ = interval; }
  uint32_t get_update_interval() const { return update_interval_; }

  // ROI configuration
  void set_roi_config(const ROIConfig &config) { roi_config_ = config; }
  virtual void update_roi_enabled(bool enabled) {
    LockGuard lock(this->frame_mutex_);
    roi_config_.enabled = enabled;
  }
  virtual void update_roi_center_row(int row) {
    if (row >= 1 && row <= 24) {
      LockGuard lock(this->frame_mutex_);
      roi_config_.center_row = row;
    }
  }
  virtual void update_roi_center_col(int col) {
    if (col >= 1 && col <= 32) {
      LockGuard lock(this->frame_mutex_);
      roi_config_.center_col = col;
    }
  }
  virtual void update_roi_size(int size) {
    if (size >= 1 && size <= 10) {
      LockGuard lock(this->frame_mutex_);
      roi_config_.size = size;
    }
  }

  // Web-overlay toggle. Always available: the value is only consumed by the
  // network-gated JPEG code, but the switch/control that drives it is compiled
  // regardless of USE_NETWORK, so these callers must resolve without it.
  void set_web_overlay_enabled(bool enabled) { web_overlay_enabled_ = enabled; }
  virtual void update_web_overlay_enabled(bool enabled) { web_overlay_enabled_ = enabled; }
  bool is_web_overlay_enabled() const { return web_overlay_enabled_; }

#ifdef USE_NETWORK
  // Web server configuration
  void set_web_server_enabled(bool enabled) { web_server_enabled_ = enabled; }
  void set_web_server_path(const std::string &path) { web_server_path_ = path; }
  void set_web_server_quality(int quality) { web_server_quality_ = quality; }
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

  // Auto-generated control entity setters (the 7 controls common to every
  // thermal-camera device)
  void set_update_interval_control(number::Number *control) { update_interval_control_ = control; }
  void set_thermal_palette_control(select::Select *control) { thermal_palette_control_ = control; }
  void set_roi_enabled_control(switch_::Switch *control) { roi_enabled_control_ = control; }
  void set_roi_center_row_control(number::Number *control) { roi_center_row_control_ = control; }
  void set_roi_center_col_control(number::Number *control) { roi_center_col_control_ = control; }
  void set_roi_size_control(number::Number *control) { roi_size_control_ = control; }
  void set_web_overlay_enabled_control(switch_::Switch *control) { web_overlay_enabled_control_ = control; }

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

  // Thermal color mapping
  virtual void set_thermal_palette(const std::string &palette);
  const std::string &get_thermal_palette() const { return thermal_palette_; }
  uint16_t temp_to_color(float temperature, float min_temp, float max_temp) const;

  // ROI overlay coordinate calculation (hardware-agnostic)
  bool get_roi_overlay_bounds(int image_x, int image_y, int image_w, int image_h, int &roi_x1, int &roi_y1, int &roi_x2,
                              int &roi_y2) const;
  bool get_roi_crosshair_coords(int image_x, int image_y, int image_w, int image_h, int &center_x, int &center_y) const;

  // State synchronization - sync internal roi_config_ (+ update_interval_)
  // with control entity values. Virtual so a subclass can extend it (e.g. to
  // sync its own extra controls) by calling the base implementation first.
  virtual void sync_roi_state_from_controls();

#ifdef USE_NETWORK
  // Web server thermal image handler
  void handle_thermal_image_request_(AsyncWebServerRequest *request);
  // Web server thermal viewer page (a small HTML page that renders the JPEG)
  void handle_thermal_page_request_(AsyncWebServerRequest *request);
#endif

  // Fall-through for device-specific numeric/switch control entities whose
  // control_type isn't one of the 7 common ThermalControlType values. Public
  // because the shared ThermalNumber/ThermalSwitch control entities (not
  // subclasses of ThermalCameraBase) call these on their parent_ pointer.
  virtual void on_extra_number_control(int type, float value) {}
  virtual void on_extra_switch_control(int type, bool value) {}

 protected:
  // --- Virtual seams implemented by device subclasses ----------------------
  // Device I/O: read the next frame data into pixels_. Return true only when
  // a full, coherent frame is ready for stats/ROI processing this tick.
  virtual bool read_frame_() = 0;
  // One-time device setup (probing, register init, ...). Returning false
  // marks the component failed (mirrors ESPHome's usual setup() contract).
  virtual bool init_device_() { return true; }
  // Human-readable device name used in logs and the web viewer page.
  virtual const char *display_name() const = 0;
  // Device-specific dump_config() lines, logged right after LOG_I2C_DEVICE.
  virtual void dump_device_config_() {}
  // Runs once per setup(), after the palette/ROI sync, before web setup.
  virtual void on_setup_() {}
  // Runs once per update_interval tick, before read_frame_() is attempted
  // (e.g. device button polling that must happen every tick regardless of
  // whether a frame was successfully read).
  virtual void on_update_tick_(uint32_t now) {}
  // Runs after compute_stats_()/process_roi_temperatures_(), only on ticks
  // where read_frame_() reported a ready frame (e.g. alarm evaluation).
  virtual void on_frame_() {}
  // Runs every loop() call, independent of the update_interval cadence (e.g.
  // alarm/LED/buzzer output driving that must stay smooth between frames).
  virtual void on_loop_(uint32_t now) {}

  void compute_stats_();
  void process_roi_temperatures_();
  void calculate_roi_bounds_(int center_row, int center_col, int size, int &min_row, int &max_row, int &min_col,
                             int &max_col) const;

  // Configuration helpers
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
  uint32_t update_interval_{2000};
  ROIConfig roi_config_;

  // Validity band applied to both whole-frame and ROI stats. mlx90640 (a bare
  // sensor) narrows temp_valid_max_ to ~85; the Thermal2's onboard controller
  // reports a wider raw range, so it keeps the default.
  float temp_valid_min_{-40.0f};
  float temp_valid_max_{300.0f};

  // Thermal palette configuration
  std::string thermal_palette_{"rainbow"};
  const uint16_t *current_palette_{nullptr};

  // Web-overlay flag (always compiled — see the toggle methods above).
  bool web_overlay_enabled_{true};

#ifdef USE_NETWORK
  // Web server configuration
  bool web_server_enabled_{false};
  std::string web_server_path_{"/thermal.jpg"};
  int web_server_quality_{85};
  bool web_html_page_enabled_{true};
  std::string web_html_path_;  // viewer page path, derived from web_server_path_ at setup
  web_server_base::WebServerBase *base_{nullptr};
#endif

  // Hardware state
  bool initialized_{false};

  // Serializes the loop-task frame update (read_frame_ + compute_stats_ +
  // process_roi_temperatures_ + roi_config_ writes) against the esp_http_server
  // render read (interpolate + colorize + overlay). Non-recursive: never
  // re-acquire while held, and keep the getters/render helpers lock-free.
  Mutex frame_mutex_;

  // Data buffers (class members to prevent stack overflow; zero-init so a
  // pre-prime read never exposes indeterminate RAM to stats/alarm/JPEG).
  float pixels_[THERMAL_PIXELS]{};        // Assembled thermal frame (32x24), °C
  float interpolated_pixels_[64 * 48]{};  // Upscaled thermal data for display
  float valid_pixels_[THERMAL_PIXELS]{};  // Valid pixel buffer for sorting
  float adj_2d_[16]{};                    // Adjacent matrix for interpolation

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

  // Auto-generated control entities (optional) - the 7 common controls
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

// Control type for the shared control entities below. The 7 values here are
// common to every thermal-camera device; device subclasses that need extra
// control types (e.g. m5stack_thermal2's BUZZER_ENABLED/ALARM_*) define their
// own enum starting at THERMAL_CONTROL_TYPE_EXTRA_START and are dispatched to
// via ThermalCameraBase::on_extra_number_control()/on_extra_switch_control().
enum ThermalControlType {
  UPDATE_INTERVAL,
  ROI_CENTER_ROW,
  ROI_CENTER_COL,
  ROI_SIZE,
  THERMAL_PALETTE,
  ROI_ENABLED,
  WEB_OVERLAY_ENABLED,
  THERMAL_CONTROL_TYPE_EXTRA_START = 100,
};

// ThermalNumber - handles the shared numeric controls (update interval, ROI).
// Device-specific numeric controls (e.g. alarm thresholds) are dispatched
// through ThermalCameraBase::on_extra_number_control().
class ThermalNumber : public number::Number, public Component {
 public:
  void set_thermal_parent(ThermalCameraBase *parent) { parent_ = parent; }
  void set_control_type(int type) { control_type_ = type; }

  // Configuration methods (similar to template number)
  void set_restore_value(bool restore_value) { restore_value_ = restore_value; }
  void set_initial_value(float initial_value) { initial_value_ = initial_value; }

  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(float value) override;

  ThermalCameraBase *parent_{nullptr};
  int control_type_{UPDATE_INTERVAL};
  bool restore_value_{false};
  float initial_value_{NAN};
  ESPPreferenceObject pref_;
};

// ThermalSelect - handles thermal palette selection
class ThermalSelect : public select::Select, public Component {
 public:
  void set_thermal_parent(ThermalCameraBase *parent) { parent_ = parent; }

  // Configuration methods (similar to template select)
  void set_restore_value(bool restore_value) { restore_value_ = restore_value; }
  void set_initial_option(const std::string &initial_option) { initial_option_ = initial_option; }

  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void control(const std::string &value) override;

  ThermalCameraBase *parent_{nullptr};
  bool restore_value_{false};
  std::string initial_option_;
  ESPPreferenceObject pref_;
};

// ThermalSwitch - handles ROI-enabled and web-overlay-enabled controls.
// Device-specific switch controls (e.g. buzzer enable) are dispatched
// through ThermalCameraBase::on_extra_switch_control().
class ThermalSwitch : public switch_::Switch, public Component {
 public:
  void set_thermal_parent(ThermalCameraBase *parent) { parent_ = parent; }
  void set_control_type(int type) { control_type_ = type; }
  void set_restore_mode(switch_::SwitchRestoreMode restore_mode) { this->restore_mode = restore_mode; }

  void setup() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  void write_state(bool state) override;
  void dispatch_control_(bool state);

  ThermalCameraBase *parent_{nullptr};
  int control_type_{ROI_ENABLED};
};

}  // namespace thermal_camera_core
}  // namespace esphome
