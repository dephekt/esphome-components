#include "m5stack_thermal2.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>

namespace esphome {
namespace m5stack_thermal2 {

static const char *const TAG = "m5stack_thermal2";

void M5Thermal2Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up M5Stack Thermal2...");

  if (!init_device_()) {
    ESP_LOGE(TAG, "M5Stack Thermal2 not found / id mismatch at 0x%02X", this->address_);
    this->mark_failed();
    return;
  }

  // Initialize thermal color palette
  set_active_palette_();

  // Sync ROI state from control entities (they have higher setup priority)
  sync_roi_state_from_controls();

  // Establish the initial LED for the current (post-restore) ROI mode.
  refresh_status_led_();

#ifdef USE_NETWORK
  // Setup web server if enabled
  if (web_server_enabled_) {
    setup_web_server_();
  }
#endif

  ESP_LOGCONFIG(TAG, "M5Stack Thermal2 setup complete");
}

void M5Thermal2Component::loop() {
  if (!initialized_)
    return;

  uint32_t now = millis();
  if (now - last_update_time_ >= update_interval_) {
    last_update_time_ = now;
    handle_button_();
    if (read_frame_()) {
      compute_stats_();
      process_roi_temperatures_();
      evaluate_alarm_();
    }
  }

  // Beep/flash driver and status LED run independently of the frame cadence so
  // the alarm blinks smoothly and UI-driven ROI-mode changes reflect promptly.
  if (alarm_active_) {
    drive_alarm_output_();
  } else if (sound_test_active_) {
    // Hold the one-shot test beep + red flash until one beep interval elapses.
    if (millis() - sound_test_start_ >= alarm_beep_interval_) {
      sound_test_active_ = false;
      set_buzzer_(0);
      refresh_status_led_();
    }
  } else {
    refresh_status_led_();
  }
}

void M5Thermal2Component::trigger_sound_test() {
  if (alarm_active_)
    return;  // a real alarm is already sounding
  ESP_LOGD(TAG, "Sound test: one alarm cycle");
  sound_test_active_ = true;
  sound_test_start_ = millis();
  set_led_(alarm_led_r_, alarm_led_g_, alarm_led_b_);
  set_buzzer_(alarm_buzzer_frequency_);  // sounds regardless of the mute switch
}

void M5Thermal2Component::dump_config() {
  ESP_LOGCONFIG(TAG, "M5Stack Thermal2:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Refresh Rate: %s", refresh_rate_.c_str());
  ESP_LOGCONFIG(TAG, "  Noise Filter: %u", noise_filter_);
  ESP_LOGCONFIG(TAG, "  Update Interval: %ums", update_interval_);
  ESP_LOGCONFIG(TAG, "  Status LED: %s", status_led_enabled_ ? "on" : "off");
  ESP_LOGCONFIG(TAG, "  Alarm: high=%.1f°C low=%.1f°C hysteresis=%.1f°C", alarm_high_threshold_, alarm_low_threshold_,
                alarm_hysteresis_);
  ESP_LOGCONFIG(TAG, "  Alarm buzzer: %uHz volume=%u interval=%ums", alarm_buzzer_frequency_, alarm_buzzer_volume_,
                alarm_beep_interval_);
  if (roi_config_.enabled) {
    ESP_LOGCONFIG(TAG, "  ROI mode: Center(%d,%d) Size=%d", roi_config_.center_row, roi_config_.center_col,
                  roi_config_.size);
  }
#ifdef USE_NETWORK
  if (web_server_enabled_) {
    ESP_LOGCONFIG(TAG, "  Web Server: %s (160x120, quality=%d)", web_server_path_.c_str(), web_server_quality_);
    if (web_html_page_enabled_) {
      ESP_LOGCONFIG(TAG, "  Thermal viewer page: %s", web_html_path_.c_str());
    }
  }
#endif
}

// --- Low-level Thermal2 register access ---------------------------------------
bool M5Thermal2Component::read_reg8_(uint8_t reg, uint8_t *value) {
  return this->read_register(reg, value, 1) == i2c::ERROR_OK;
}

bool M5Thermal2Component::write_reg8_(uint8_t reg, uint8_t value) {
  return this->write_register(reg, &value, 1) == i2c::ERROR_OK;
}

bool M5Thermal2Component::write_reg16_(uint8_t reg, uint16_t value) {
  uint8_t buf[2] = {(uint8_t) (value & 0xFF), (uint8_t) (value >> 8)};  // little-endian
  return this->write_register(reg, buf, 2) == i2c::ERROR_OK;
}

bool M5Thermal2Component::init_device_() {
  // The unit needs >100ms after power-on; retry the device-id probe.
  uint8_t id0 = 0, id1 = 0;
  bool found = false;
  for (int i = 0; i < 16; i++) {
    if (read_reg8_(REG_DEVICE_ID0, &id0) && read_reg8_(REG_DEVICE_ID1, &id1) && id0 == DEVICE_ID0_VALUE &&
        id1 == DEVICE_ID1_VALUE) {
      found = true;
      break;
    }
    delay(16);
  }
  if (!found) {
    ESP_LOGE(TAG, "Thermal2 id mismatch (got 0x%02X 0x%02X, want 0x90 0x64)", id0, id1);
    return false;
  }

  // Enable the manual buzzer + LED (so our frequency/RGB writes take effect) and
  // auto-refresh (the unit samples continuously; we just read frames).
  function_ctrl_ = FUNCTION_BUZZER_EN | FUNCTION_LED_EN | FUNCTION_AUTO_REFRESH;
  write_reg8_(REG_FUNCTION_CTRL, function_ctrl_);
  write_reg8_(REG_REFRESH_RATE, parse_refresh_rate_(refresh_rate_));
  write_reg8_(REG_NOISE_FILTER, noise_filter_ & 0x0F);
  write_reg8_(REG_BUZZER_VOLUME, alarm_buzzer_volume_);
  // Keep the unit's own monitor area at full frame; all region math is done in
  // software so the ROI can be moved off-center (unlike the hardware ROI).
  write_reg8_(REG_TEMP_MONITOR_AREA, 15 | (11 << 4));

  set_buzzer_(0);  // silent until an alarm fires
  initialized_ = true;

  // Prime a full (both-subpage) frame here in setup() where blocking is free, so
  // the very first published stats/alarm start from real data instead of a
  // half-empty frame. Afterwards loop() reads one subpage per cycle (rolling).
  prime_frame_();
  return true;
}

// Blocking read of both subpages into a coherent frame. Only called from setup().
void M5Thermal2Component::prime_frame_() {
  uint32_t start = millis();
  bool got_sub0 = false, got_sub1 = false;
  while ((!got_sub0 || !got_sub1) && (millis() - start < 500)) {
    uint8_t ctrl[2];
    if (this->read_register(REG_REFRESH_CTRL, ctrl, 2) == i2c::ERROR_OK && (ctrl[0] & 0x01)) {
      bool subpage = ctrl[1] & 0x01;
      if (((subpage && !got_sub1) || (!subpage && !got_sub0)) && read_subpage_pixels_()) {
        store_subpage_(pixel_raw_, subpage);
        if (subpage)
          got_sub1 = true;
        else
          got_sub0 = true;
      }
    }
    delay(4);
  }
}

// Reads REG_PIXEL_DATA (0x80) — 768 bytes = 384 little-endian uint16 for the
// current subpage — into pixel_raw_. The unit auto-increments its read pointer,
// so later chunks continue sequentially without re-addressing (register indices
// past 0x80 exceed 8 bits and can only be reached by streaming).
bool M5Thermal2Component::read_subpage_pixels_() {
  uint8_t *dst = (uint8_t *) pixel_raw_;
  const int total = SUBPAGE_PIXELS * 2;  // 768 bytes
  const int chunk = 256;
  if (this->read_register(REG_PIXEL_DATA, dst, chunk) != i2c::ERROR_OK)
    return false;
  int off = chunk;
  while (off < total) {
    int n = std::min(chunk, total - off);
    if (this->read(dst + off, n) != i2c::ERROR_OK)
      return false;
    off += n;
  }
  return true;
}

// Splat one 16x24 subpage into the full 32x24 frame. The unit returns the
// checkerboard half indicated by `subpage`; two consecutive subpages fill all
// 768 pixels. Mapping per M5Stack's reference firmware.
void M5Thermal2Component::store_subpage_(const uint16_t *raw, bool subpage) {
  for (int idx = 0; idx < SUBPAGE_PIXELS; idx++) {
    int y = idx >> 4;                                                       // 0..23
    int x = ((idx & 15) << 1) + (((y & 1) != (int) subpage) ? 1 : 0);      // 0..31
    pixels_[y * THERMAL_COLS + x] = ((float) raw[idx]) / 128.0f - 64.0f;
  }
}

bool M5Thermal2Component::read_frame_() {
  // One subpage per cycle into the persistent 32x24 buffer: the opposite half
  // carries over from the previous cycle (rolling frame). This keeps loop()
  // blocking to a single ~20ms pixel read instead of waiting out a subpage flip.
  // The unit alternates subpages, so both halves refresh within two cycles.
  uint8_t ctrl[2];
  if (this->read_register(REG_REFRESH_CTRL, ctrl, 2) != i2c::ERROR_OK)
    return false;
  if ((ctrl[0] & 0x01) == 0)
    return false;  // no fresh frame yet
  bool subpage = ctrl[1] & 0x01;

  if (!read_subpage_pixels_())
    return false;
  store_subpage_(pixel_raw_, subpage);
  return true;
}

void M5Thermal2Component::compute_stats_() {
  float min_temp = pixels_[0];
  float max_temp = pixels_[0];
  float sum_temp = 0;
  int valid_count = 0;

  // Filter out invalid/extreme readings (typical usable range for this sensor).
  for (int i = 0; i < THERMAL_PIXELS; i++) {
    float temp = pixels_[i];
    if (temp > -40.0 && temp < 300.0) {
      valid_pixels_[valid_count++] = temp;
      if (temp < min_temp)
        min_temp = temp;
      if (temp > max_temp)
        max_temp = temp;
      sum_temp += temp;
    }
  }

  if (valid_count == 0) {
    ESP_LOGW(TAG, "No valid thermal readings - all pixels out of range");
    return;
  }

  min_temp_ = min_temp;
  max_temp_ = max_temp;
  avg_temp_ = sum_temp / valid_count;
  std::sort(valid_pixels_, valid_pixels_ + valid_count);
  median_temp_ = valid_pixels_[valid_count / 2];

  if (temp_min_sensor_)
    temp_min_sensor_->publish_state(min_temp_);
  if (temp_max_sensor_)
    temp_max_sensor_->publish_state(max_temp_);
  if (temp_avg_sensor_)
    temp_avg_sensor_->publish_state(avg_temp_);
  if (median_sensor_)
    median_sensor_->publish_state(median_temp_);

  ESP_LOGD(TAG, "Thermal (%d px) - Min: %.1f°C, Max: %.1f°C, Avg: %.1f°C, Median: %.1f°C", valid_count, min_temp_,
           max_temp_, avg_temp_, median_temp_);
}

// --- Button / alarm / LED -----------------------------------------------------
void M5Thermal2Component::handle_button_() {
  uint8_t btn = 0;
  if (!read_reg8_(REG_BUTTON, &btn))
    return;
  if (button_sensor_ != nullptr)
    button_sensor_->publish_state(btn & BUTTON_IS_PRESSED);

  if (btn & BUTTON_WAS_CLICKED) {
    // Toggle ROI mode. Keep the switch entity in sync so the change reflects in
    // the UI too (publish_state, not a command, so no recursion into the unit).
    bool new_state = !roi_config_.enabled;
    roi_config_.enabled = new_state;
    if (roi_enabled_control_ != nullptr)
      roi_enabled_control_->publish_state(new_state);
    ESP_LOGD(TAG, "Button click: ROI mode %s", new_state ? "ON" : "OFF");
  }

  // Ack latched flags by writing the button byte back (per the M5 protocol).
  if (btn & ~BUTTON_IS_PRESSED)
    write_reg8_(REG_BUTTON, btn);
}

float M5Thermal2Component::get_alarm_source_temp_() const {
  bool use_roi = (alarm_region_ == ALARM_REGION_ROI) || (alarm_region_ == ALARM_REGION_ACTIVE && roi_config_.enabled);
  if (use_roi && roi_pixel_count_ <= 0)
    use_roi = false;  // fall back to whole-frame if ROI stats are unavailable
  switch (alarm_source_) {
    case ALARM_SOURCE_MIN:
      return use_roi ? roi_min_temp_ : min_temp_;
    case ALARM_SOURCE_MAX:
      return use_roi ? roi_max_temp_ : max_temp_;
    case ALARM_SOURCE_MEDIAN:
      return use_roi ? roi_median_temp_ : median_temp_;
    case ALARM_SOURCE_AVERAGE:
    default:
      return use_roi ? roi_avg_temp_ : avg_temp_;
  }
}

void M5Thermal2Component::evaluate_alarm_() {
  float t = get_alarm_source_temp_();
  if (std::isnan(t))
    return;

  // Hysteresis keeps the alarm from chattering when the value hovers at a bound.
  if (!alarm_high_active_ && t >= alarm_high_threshold_)
    alarm_high_active_ = true;
  else if (alarm_high_active_ && t <= alarm_high_threshold_ - alarm_hysteresis_)
    alarm_high_active_ = false;

  if (!alarm_low_active_ && t <= alarm_low_threshold_)
    alarm_low_active_ = true;
  else if (alarm_low_active_ && t >= alarm_low_threshold_ + alarm_hysteresis_)
    alarm_low_active_ = false;

  bool active = alarm_high_active_ || alarm_low_active_;
  if (active == alarm_active_)
    return;

  alarm_active_ = active;
  if (alarm_active_sensor_ != nullptr)
    alarm_active_sensor_->publish_state(alarm_active_);

  if (alarm_active_) {
    // Start a blink cycle immediately.
    blink_on_ = true;
    last_blink_time_ = millis();
    set_led_(alarm_led_r_, alarm_led_g_, alarm_led_b_);
    set_buzzer_(buzzer_enabled_ ? alarm_buzzer_frequency_ : 0);
  } else {
    // Clear: silence the buzzer and restore the status LED right away.
    blink_on_ = false;
    set_buzzer_(0);
    refresh_status_led_();
  }
  ESP_LOGD(TAG, "Alarm %s (source=%.1f°C, high=%s, low=%s)", alarm_active_ ? "ACTIVE" : "clear", t,
           alarm_high_active_ ? "yes" : "no", alarm_low_active_ ? "yes" : "no");
}

// Toggles the manual buzzer + red LED at beep_interval while the alarm is
// active. Muting (buzzer_enabled_ off) drops the tone but keeps the red flash.
void M5Thermal2Component::drive_alarm_output_() {
  uint32_t now = millis();
  if (now - last_blink_time_ < alarm_beep_interval_)
    return;
  last_blink_time_ = now;
  blink_on_ = !blink_on_;
  if (blink_on_) {
    set_led_(alarm_led_r_, alarm_led_g_, alarm_led_b_);
    set_buzzer_(buzzer_enabled_ ? alarm_buzzer_frequency_ : 0);
  } else {
    set_led_(0, 0, 0);
    set_buzzer_(0);
  }
}

void M5Thermal2Component::set_led_(uint8_t r, uint8_t g, uint8_t b) {
  if (last_led_r_ == r && last_led_g_ == g && last_led_b_ == b)
    return;
  uint8_t rgb[3] = {r, g, b};
  if (this->write_register(REG_LED_R, rgb, 3) == i2c::ERROR_OK) {
    last_led_r_ = r;
    last_led_g_ = g;
    last_led_b_ = b;
  }
}

void M5Thermal2Component::set_buzzer_(uint16_t freq) {
  if (last_buzzer_freq_ == (int32_t) freq)
    return;
  if (write_reg16_(REG_BUZZER_FREQ, freq))
    last_buzzer_freq_ = freq;
}

// Green = healthy full-frame, blue = ROI mode. Suppressed when status_led off.
// The alarm owns the LED (red flash) while active.
void M5Thermal2Component::refresh_status_led_() {
  if (alarm_active_)
    return;
  if (!status_led_enabled_) {
    set_led_(0, 0, 0);
    return;
  }
  if (roi_config_.enabled)
    set_led_(0, 0, 8);  // blue = ROI mode
  else
    set_led_(0, 8, 0);  // green = healthy
}

// ROI calculation helper - converts 1-based user coordinates to 0-based array bounds
void M5Thermal2Component::calculate_roi_bounds_(int center_row, int center_col, int size, int &min_row, int &max_row,
                                              int &min_col, int &max_col) const {
  // Convert 1-based user coordinates to 0-based array indices
  int center_row_idx = center_row - 1;  // Convert 1-24 to 0-23
  int center_col_idx = center_col - 1;  // Convert 1-32 to 0-31

  // Calculate ROI bounds (size = n means (2n+1)x(2n+1) square)
  min_row = std::max(0, center_row_idx - size);
  max_row = std::min(23, center_row_idx + size);  // M5Thermal2 has 24 rows (0-23)
  min_col = std::max(0, center_col_idx - size);
  max_col = std::min(31, center_col_idx + size);  // M5Thermal2 has 32 columns (0-31)
}

// Process ROI temperatures from the main pixels_ array
void M5Thermal2Component::process_roi_temperatures_() {
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
      int pixel_idx = row * 32 + col;  // M5Thermal2 is 32 columns wide
      float temp = pixels_[pixel_idx];

      // Filter out invalid/extreme readings (same range as main processing)
      if (temp > -40.0 && temp < 300.0) {
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

// Configuration helper functions. The Thermal2 refresh-rate register (0x0B)
// uses the same 0..7 encoding as the raw MLX90640 (0.5Hz..64Hz).
uint8_t M5Thermal2Component::parse_refresh_rate_(const std::string &rate_str) {
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

// Thermal interpolation functions for smooth upscaling (copied from grow_env_monitor)
float M5Thermal2Component::get_point_(float *p, uint8_t rows, uint8_t cols, int8_t x, int8_t y) {
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

void M5Thermal2Component::set_point_(float *p, uint8_t rows, uint8_t cols, int8_t x, int8_t y, float f) {
  if ((x < 0) || (x >= cols))
    return;
  if ((y < 0) || (y >= rows))
    return;
  p[y * cols + x] = f;
}

void M5Thermal2Component::get_adjacents_2d_(float *src, float *dest, uint8_t rows, uint8_t cols, int8_t x, int8_t y) {
  // Fill dest with adjacent points in 2D grid format for bicubic interpolation
  for (int dy = -1; dy <= 2; dy++) {
    for (int dx = -1; dx <= 2; dx++) {
      dest[(dy + 1) * 4 + (dx + 1)] = get_point_(src, rows, cols, x + dx, y + dy);
    }
  }
}

float M5Thermal2Component::cubic_interpolate_(float p[], float x) {
  return p[1] + 0.5 * x *
                    (p[2] - p[0] +
                     x * (2.0 * p[0] - 5.0 * p[1] + 4.0 * p[2] - p[3] + x * (3.0 * (p[1] - p[2]) + p[3] - p[0])));
}

float M5Thermal2Component::bicubic_interpolate_(float p[], float x, float y) {
  float arr[4];
  arr[0] = cubic_interpolate_(p, x);
  arr[1] = cubic_interpolate_(p + 4, x);
  arr[2] = cubic_interpolate_(p + 8, x);
  arr[3] = cubic_interpolate_(p + 12, x);
  return cubic_interpolate_(arr, y);
}

void M5Thermal2Component::interpolate_image_(float *src, uint8_t src_rows, uint8_t src_cols, float *dest,
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
const uint16_t M5Thermal2Component::thermal_palette_rainbow_[256] PROGMEM = {
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

const uint16_t M5Thermal2Component::thermal_palette_golden_[256] PROGMEM = {
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

const uint16_t M5Thermal2Component::thermal_palette_grayscale_[256] PROGMEM = {
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

const uint16_t M5Thermal2Component::thermal_palette_ironblack_[256] PROGMEM = {
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

const uint16_t M5Thermal2Component::thermal_palette_cam_[256] PROGMEM = {
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

const uint16_t M5Thermal2Component::thermal_palette_ironbow_[256] PROGMEM = {
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

const uint16_t M5Thermal2Component::thermal_palette_arctic_[256] PROGMEM = {
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

const uint16_t M5Thermal2Component::thermal_palette_lava_[256] PROGMEM = {
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

const uint16_t M5Thermal2Component::thermal_palette_whitehot_[256] PROGMEM = {
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

const uint16_t M5Thermal2Component::thermal_palette_blackhot_[256] PROGMEM = {
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
void M5Thermal2Component::set_thermal_palette(const std::string &palette) {
  thermal_palette_ = palette;
  set_active_palette_();
}

uint16_t M5Thermal2Component::temp_to_color(float temperature, float min_temp, float max_temp) const {
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

void M5Thermal2Component::set_active_palette_() {
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

  ESP_LOGCONFIG(TAG, "M5Thermal2 color palette set to: %s", thermal_palette_.c_str());
}

// ROI overlay coordinate calculation methods (hardware-agnostic)
bool M5Thermal2Component::get_roi_overlay_bounds(int image_x, int image_y, int image_w, int image_h, int &roi_x1,
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

bool M5Thermal2Component::get_roi_crosshair_coords(int image_x, int image_y, int image_w, int image_h, int &center_x,
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
// Handler for the thermal-image HTTP endpoint. ESPHome's web_server_idf
// backend dispatches requests to registered AsyncWebHandlers; there is no
// AsyncWebServer::on() on this backend (used on 2026.x, incl. Arduino).
class M5Thermal2ThermalImageHandler : public AsyncWebHandler {
 public:
  M5Thermal2ThermalImageHandler(M5Thermal2Component *parent, const std::string &path) : parent_(parent), path_(path) {}
  bool canHandle(AsyncWebServerRequest *request) const override {
    return request->method() == HTTP_GET && request->url() == this->path_;
  }
  void handleRequest(AsyncWebServerRequest *request) override { this->parent_->handle_thermal_image_request_(request); }

 protected:
  M5Thermal2Component *parent_;
  std::string path_;
};

// Handler for the thermal-viewer HTML page: a tiny self-contained page that
// renders the JPEG endpoint with an auto-refreshing <img>, so the camera can
// be watched in a browser without Home Assistant or the ESPHome dashboard.
class M5Thermal2ThermalPageHandler : public AsyncWebHandler {
 public:
  M5Thermal2ThermalPageHandler(M5Thermal2Component *parent, const std::string &path) : parent_(parent), path_(path) {}
  bool canHandle(AsyncWebServerRequest *request) const override {
    return request->method() == HTTP_GET && request->url() == this->path_;
  }
  void handleRequest(AsyncWebServerRequest *request) override { this->parent_->handle_thermal_page_request_(request); }

 protected:
  M5Thermal2Component *parent_;
  std::string path_;
};

void M5Thermal2Component::setup_web_server_() {
  ESP_LOGCONFIG(TAG, "Setting up web server endpoint at %s", web_server_path_.c_str());

  // Check if web server base instance was provided
  if (!base_) {
    ESP_LOGW(TAG, "WebServerBase not available, thermal image endpoint not registered");
    return;
  }

  // Ensure the web server base is initialized
  base_->init();

  // Register our endpoint as an AsyncWebHandler (web_server_idf has no
  // AsyncWebServer::on()). The handler lives for the program's lifetime.
  base_->add_handler(new M5Thermal2ThermalImageHandler(this, web_server_path_));

  ESP_LOGCONFIG(TAG, "Thermal image endpoint registered successfully");

  // Optional HTML viewer page. Derive its path from the image path: swap a
  // trailing ".jpg" for ".html" (so "/thermal.jpg" -> "/thermal.html"), else
  // just append ".html".
  if (web_html_page_enabled_) {
    web_html_path_ = web_server_path_;
    const std::string jpg_ext = ".jpg";
    if (web_html_path_.size() >= jpg_ext.size() &&
        web_html_path_.compare(web_html_path_.size() - jpg_ext.size(), jpg_ext.size(), jpg_ext) == 0) {
      web_html_path_.replace(web_html_path_.size() - jpg_ext.size(), jpg_ext.size(), ".html");
    } else {
      web_html_path_ += ".html";
    }
    base_->add_handler(new M5Thermal2ThermalPageHandler(this, web_html_path_));
    ESP_LOGCONFIG(TAG, "Thermal viewer page registered at %s", web_html_path_.c_str());
  }
}

void M5Thermal2Component::handle_thermal_image_request_(AsyncWebServerRequest *request) {
  if (!initialized_) {
    ESP_LOGW(TAG, "Thermal camera not initialized, cannot serve image");
    request->send(503, "text/plain", "Thermal camera not ready");
    return;
  }

  ESP_LOGD(TAG, "Generating JPEG using JPEGENC library");
  generate_jpg_jpegenc_(request, 160, 120, web_server_quality_);
}

void M5Thermal2Component::handle_thermal_page_request_(AsyncWebServerRequest *request) {
  // Refresh the image at roughly the frame cadence; a new fetch re-renders the
  // latest frame on the device. Floor it so a tiny update_interval can't make
  // the browser hammer the endpoint.
  uint32_t refresh_ms = update_interval_ < 250 ? 250 : update_interval_;

  std::string html;
  html.reserve(1100);
  html += "<!DOCTYPE html><html><head><meta charset=\"utf-8\">";
  html += "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
  html += "<title>M5Thermal2 Thermal</title><style>";
  html += "html,body{margin:0;height:100%;background:#111;color:#ccc;font:14px system-ui,sans-serif}";
  html += ".wrap{height:100%;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:12px}";
  html += "img{max-width:95vw;max-height:85vh;image-rendering:pixelated;border-radius:8px;box-shadow:0 0 20px #0008}";
  html += ".t{opacity:.7}</style></head><body><div class=\"wrap\">";
  html += "<img id=\"c\" alt=\"thermal image\"><div class=\"t\">M5Thermal2 &middot; auto-refresh</div></div><script>";
  html += "var p=\"";
  html += web_server_path_;
  html += "\",ms=";
  html += std::to_string(refresh_ms);
  html += ",i=document.getElementById('c');";
  html += "function r(){i.src=p+'?t='+Date.now()}";
  html += "i.onload=function(){setTimeout(r,ms)};i.onerror=function(){setTimeout(r,ms*2)};r();";
  html += "</script></body></html>";

  AsyncWebServerResponse *response = request->beginResponse(200, "text/html", html);
  response->addHeader("Cache-Control", "no-cache");
  request->send(response);
}

void M5Thermal2Component::generate_jpg_jpegenc_(AsyncWebServerRequest *request, int width, int height, int quality) {
  // JPEGENC embeds a ~3 KB JPEGE_IMAGE (a 2 KB file buffer plus quant/MCU tables). This handler
  // runs on the esp_http_server task, whose stack is only ~4 KB, so a stack-allocated JPEGENC
  // overflows the task stack and resets the device on every /thermal.jpg request. Keep it on the
  // heap (unique_ptr frees it on every return path). JPEGENCODE is tiny and stays on the stack.
  std::unique_ptr<JPEGENC> jpg(new (std::nothrow) JPEGENC());
  if (!jpg) {
    ESP_LOGE(TAG, "Failed to allocate JPEG encoder");
    request->send(500, "text/plain", "Memory allocation failed");
    return;
  }
  JPEGENCODE jpe;

  // Use smaller resolution to avoid heap exhaustion
  int img_width = std::min(width, 160);  // Max 160x120 to save memory
  int img_height = std::min(height, 120);
  ESP_LOGD(TAG, "Generating %dx%d thermal JPEG (requested %dx%d)", img_width, img_height, width, height);

  // Interpolate the latest frame (24x32 -> 48x64) on demand here, rather than
  // every poll cycle, so the loop only pays for it when an image is requested.
  interpolate_image_(pixels_, 24, 32, interpolated_pixels_, 48, 64);

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

  int result = jpg->open(jpg_buffer, 16384);
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

  result = jpg->encodeBegin(&jpe, img_width, img_height, JPEGE_PIXEL_RGB565, JPEGE_SUBSAMPLE_420, jpeg_quality);
  if (result != JPEGE_SUCCESS) {
    ESP_LOGE(TAG, "Failed to begin JPEG encoding");
    free(jpg_buffer);
    request->send(500, "text/plain", "JPEG encoding failed");
    return;
  }

  result = jpg->addFrame(&jpe, (uint8_t *) image_data.data(), img_width * sizeof(uint16_t));
  if (result != JPEGE_SUCCESS) {
    ESP_LOGE(TAG, "Failed to add frame data");
    free(jpg_buffer);
    request->send(500, "text/plain", "JPEG frame failed");
    return;
  }

  int jpeg_size = jpg->close();
  ESP_LOGD(TAG, "JPEG created in memory: %d bytes", jpeg_size);

  if (jpeg_size > 0) {
    // Serve JPEG from memory. Copy into a std::string response (web_server_idf
    // dropped beginResponse_P); the copy lets us free jpg_buffer below safely.
    std::string jpg_content(reinterpret_cast<const char *>(jpg_buffer), jpeg_size);
    AsyncWebServerResponse *response = request->beginResponse(200, "image/jpeg", jpg_content);
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

void M5Thermal2Component::add_roi_overlay_to_image_(std::vector<uint16_t> &image_data, int img_width, int img_height) {
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

void M5Thermal2Component::add_temperature_text_to_image_(std::vector<uint16_t> &image_data, int img_width,
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
void M5Thermal2Component::sync_roi_state_from_controls() {
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

  // Sync alarm thresholds from number controls (their setup() restores the value
  // but does not push it into the parent, so pick it up here).
  if (alarm_high_threshold_control_ != nullptr && !std::isnan(alarm_high_threshold_control_->state)) {
    alarm_high_threshold_ = alarm_high_threshold_control_->state;
    ESP_LOGD(TAG, "Synced alarm high threshold: %.1f°C", alarm_high_threshold_);
  }
  if (alarm_low_threshold_control_ != nullptr && !std::isnan(alarm_low_threshold_control_->state)) {
    alarm_low_threshold_ = alarm_low_threshold_control_->state;
    ESP_LOGD(TAG, "Synced alarm low threshold: %.1f°C", alarm_low_threshold_);
  }

  ESP_LOGD(TAG, "ROI state sync complete - enabled=%s, center=(%d,%d), size=%d", roi_config_.enabled ? "true" : "false",
           roi_config_.center_row, roi_config_.center_col, roi_config_.size);
}

// M5Thermal2Number implementation
void M5Thermal2Number::setup() {
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

// M5Thermal2Select implementation
void M5Thermal2Select::setup() {
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

// M5Thermal2Switch implementation
void M5Thermal2Switch::setup() {
  optional<bool> initial_state = this->get_initial_state_with_restore_mode();

  if (initial_state.has_value()) {
    const char *control_name;
    if (control_type_ == WEB_OVERLAY_ENABLED) {
      control_name = "Web overlay enabled";
    } else if (control_type_ == BUZZER_ENABLED) {
      control_name = "Buzzer enabled";
    } else {
      control_name = "ROI enabled";
    }
    ESP_LOGD("M5Thermal2Switch", "Restored %s state: %s", control_name, initial_state.value() ? "ON" : "OFF");
    if (parent_ != nullptr) {
      if (control_type_ == WEB_OVERLAY_ENABLED) {
        parent_->update_web_overlay_enabled(initial_state.value());
      } else if (control_type_ == BUZZER_ENABLED) {
        parent_->update_buzzer_enabled(initial_state.value());
      } else {
        parent_->update_roi_enabled(initial_state.value());
      }
    }
    this->publish_state(initial_state.value());
  }
}

// M5Thermal2Number implementation
void M5Thermal2Number::control(float value) {
  if (parent_ == nullptr) {
    ESP_LOGE("M5Thermal2Number", "Parent component not set");
    return;
  }

  switch (control_type_) {
    case UPDATE_INTERVAL:
      parent_->set_update_interval((uint32_t) value);
      ESP_LOGD("M5Thermal2Number", "Update interval changed to %d ms", (uint32_t) value);
      break;

    case ROI_CENTER_ROW:
      parent_->update_roi_center_row((int) value);
      ESP_LOGD("M5Thermal2Number", "ROI center row changed to %d", (int) value);
      break;

    case ROI_CENTER_COL:
      parent_->update_roi_center_col((int) value);
      ESP_LOGD("M5Thermal2Number", "ROI center column changed to %d", (int) value);
      break;

    case ROI_SIZE:
      parent_->update_roi_size((int) value);
      ESP_LOGD("M5Thermal2Number", "ROI size changed to %d", (int) value);
      break;

    case ALARM_HIGH_THRESHOLD:
      parent_->update_alarm_high_threshold(value);
      ESP_LOGD("M5Thermal2Number", "Alarm high threshold changed to %.1f°C", value);
      break;

    case ALARM_LOW_THRESHOLD:
      parent_->update_alarm_low_threshold(value);
      ESP_LOGD("M5Thermal2Number", "Alarm low threshold changed to %.1f°C", value);
      break;

    default:
      ESP_LOGE("M5Thermal2Number", "Unknown control type");
      return;
  }

  // Update the UI state
  publish_state(value);

  if (this->restore_value_)
    this->pref_.save(&value);
}

// M5Thermal2Select implementation
void M5Thermal2Select::control(const std::string &value) {
  if (parent_ == nullptr) {
    ESP_LOGE("M5Thermal2Select", "Parent component not set");
    return;
  }

  parent_->set_thermal_palette(value);
  ESP_LOGD("M5Thermal2Select", "Thermal palette changed to %s", value.c_str());

  // Update the UI state
  publish_state(value);

  if (this->restore_value_) {
    auto index = this->index_of(value);
    if (index.has_value()) {
      this->pref_.save(&index.value());
    }
  }
}

// M5Thermal2Switch implementation
void M5Thermal2Switch::write_state(bool state) {
  if (parent_ == nullptr) {
    ESP_LOGE("M5Thermal2Switch", "Parent component not set");
    return;
  }

  if (control_type_ == WEB_OVERLAY_ENABLED) {
    parent_->update_web_overlay_enabled(state);
    ESP_LOGD("M5Thermal2Switch", "Web overlay enabled changed to %s", state ? "true" : "false");
  } else if (control_type_ == BUZZER_ENABLED) {
    parent_->update_buzzer_enabled(state);
    ESP_LOGD("M5Thermal2Switch", "Buzzer enabled changed to %s", state ? "true" : "false");
  } else {
    parent_->update_roi_enabled(state);
    ESP_LOGD("M5Thermal2Switch", "ROI enabled changed to %s", state ? "true" : "false");
  }

  // Update the UI state
  publish_state(state);
}

}  // namespace m5stack_thermal2
}  // namespace esphome
