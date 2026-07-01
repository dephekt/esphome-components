#include "m5stack_thermal2.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cmath>

namespace esphome {
namespace m5stack_thermal2 {

static const char *const TAG = "m5stack_thermal2";

// Runs once per setup(), after the palette/ROI sync (see ThermalCameraBase::setup()).
void M5Thermal2Component::on_setup_() {
  // Establish the initial LED for the current (post-restore) ROI mode.
  refresh_status_led_();
}

// Runs once per update_interval tick, before read_frame_() is attempted.
void M5Thermal2Component::on_update_tick_(uint32_t now) { handle_button_(); }

// Runs after compute_stats_()/process_roi_temperatures_(), only when
// read_frame_() reported a ready (fully-primed) frame.
void M5Thermal2Component::on_frame_() { evaluate_alarm_(); }

// Runs every loop() call, independent of the update_interval cadence, so the
// alarm blinks smoothly and UI-driven ROI-mode changes reflect promptly.
void M5Thermal2Component::on_loop_(uint32_t now) {
  if (alarm_active_) {
    drive_alarm_output_();
  } else if (sound_test_active_) {
    // Hold the one-shot test beep + red flash until one beep interval elapses.
    if (now - sound_test_start_ >= alarm_beep_interval_) {
      sound_test_active_ = false;
      set_buzzer_(0);
      refresh_status_led_();
    }
  } else {
    // Re-assert buzzer-off every idle cycle (set_buzzer_ is cached, so this is a
    // no-op unless a prior clear write NACKed — self-heals a stuck buzzer).
    set_buzzer_(0);
    refresh_status_led_();
  }
}

void M5Thermal2Component::trigger_sound_test() {
  if (alarm_active_)
    return;  // a real alarm is already sounding
  ESP_LOGD(TAG, "Sound test: one alarm cycle");
  sound_test_active_ = true;
  sound_test_start_ = millis();
  if (status_led_enabled_)
    set_led_(alarm_led_r_, alarm_led_g_, alarm_led_b_);
  set_buzzer_(alarm_buzzer_frequency_);  // sounds regardless of the mute switch
}

void M5Thermal2Component::dump_device_config_() {
  ESP_LOGCONFIG(TAG, "  Refresh Rate: %s", refresh_rate_.c_str());
  ESP_LOGCONFIG(TAG, "  Noise Filter: %u", noise_filter_);
  ESP_LOGCONFIG(TAG, "  Status LED: %s", status_led_enabled_ ? "on" : "off");
  ESP_LOGCONFIG(TAG, "  Alarm: high=%.1f°C low=%.1f°C hysteresis=%.1f°C", alarm_high_threshold_, alarm_low_threshold_,
                alarm_hysteresis_);
  ESP_LOGCONFIG(TAG, "  Alarm buzzer: %uHz volume=%u interval=%ums", alarm_buzzer_frequency_, alarm_buzzer_volume_,
                alarm_beep_interval_);
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
  subpages_seen_ |= subpage ? 0x02 : 0x01;
  if (subpages_seen_ == 0x03)
    frame_primed_ = true;  // both halves captured at least once
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
  // Base's loop() only runs stats/on_frame_() when read_frame_() returns
  // true, so fold in the frame_primed_ check here (equivalent to the old
  // caller-side `read_frame_() && frame_primed_`).
  return frame_primed_;
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
    bool new_state = !roi_config_.enabled;  // benign same-task read
    update_roi_enabled(new_state);           // guarded write (frame_mutex_)
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
  if (!alarm_enabled_)
    return;  // armed only when an `alarm:` block is configured

  float t = get_alarm_source_temp_();
  if (std::isnan(t))
    return;

  // High latch with hysteresis.
  if (!alarm_high_active_ && t >= alarm_high_threshold_)
    alarm_high_active_ = true;
  else if (alarm_high_active_ && t <= alarm_high_threshold_ - alarm_hysteresis_)
    alarm_high_active_ = false;

  // Low latch — only when the thresholds are sanely ordered. Guarding against
  // low >= high stops an overlapping-band config (e.g. low=40, high=30 via the
  // runtime controls) from latching both sides so the alarm can never clear.
  if (alarm_low_threshold_ < alarm_high_threshold_) {
    if (!alarm_low_active_ && t <= alarm_low_threshold_)
      alarm_low_active_ = true;
    else if (alarm_low_active_ && t >= alarm_low_threshold_ + alarm_hysteresis_)
      alarm_low_active_ = false;
  } else {
    alarm_low_active_ = false;
  }

  bool active = alarm_high_active_ || alarm_low_active_;
  if (active == alarm_active_)
    return;

  alarm_active_ = active;
  if (alarm_active_sensor_ != nullptr)
    alarm_active_sensor_->publish_state(alarm_active_);

  if (alarm_active_) {
    // A real alarm takes over the buzzer/LED, so cleanly preempt any in-progress
    // sound test — otherwise its leftover sound_test_active_ would make the loop
    // idle branch and the alarm-clear path fight over the outputs mid-test.
    sound_test_active_ = false;
    // Start a blink cycle immediately.
    blink_on_ = true;
    last_blink_time_ = millis();
    if (status_led_enabled_)  // honor the status_led=false knob for the flash too
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
// active. Muting drops the tone but keeps the flash; status_led=false drops the
// flash but keeps the tone.
void M5Thermal2Component::drive_alarm_output_() {
  uint32_t now = millis();
  if (now - last_blink_time_ < alarm_beep_interval_)
    return;
  last_blink_time_ = now;
  blink_on_ = !blink_on_;
  if (status_led_enabled_) {
    if (blink_on_)
      set_led_(alarm_led_r_, alarm_led_g_, alarm_led_b_);
    else
      set_led_(0, 0, 0);
  }
  set_buzzer_((blink_on_ && buzzer_enabled_) ? alarm_buzzer_frequency_ : 0);
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

// State synchronization - sync internal roi_config_/update_interval_ (base),
// then the alarm-threshold controls (device-specific).
void M5Thermal2Component::sync_roi_state_from_controls() {
  ThermalCameraBase::sync_roi_state_from_controls();

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
}

// Fall-through for device-specific numeric controls (alarm thresholds).
void M5Thermal2Component::on_extra_number_control(int type, float value) {
  switch (type) {
    case ALARM_HIGH_THRESHOLD:
      update_alarm_high_threshold(value);
      ESP_LOGD(TAG, "Alarm high threshold changed to %.1f°C", value);
      break;

    case ALARM_LOW_THRESHOLD:
      update_alarm_low_threshold(value);
      ESP_LOGD(TAG, "Alarm low threshold changed to %.1f°C", value);
      break;

    default:
      ESP_LOGE(TAG, "Unknown control type");
      break;
  }
}

// Fall-through for device-specific switch controls (buzzer enable).
void M5Thermal2Component::on_extra_switch_control(int type, bool state) {
  if (type == BUZZER_ENABLED) {
    update_buzzer_enabled(state);
    ESP_LOGD(TAG, "Buzzer enabled changed to %s", state ? "true" : "false");
  } else {
    ESP_LOGE(TAG, "Unknown control type");
  }
}

}  // namespace m5stack_thermal2
}  // namespace esphome
