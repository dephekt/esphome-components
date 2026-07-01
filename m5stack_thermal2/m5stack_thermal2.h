#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/thermal_camera_core/thermal_camera_core.h"

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

enum AlarmSource { ALARM_SOURCE_AVERAGE, ALARM_SOURCE_MEDIAN, ALARM_SOURCE_MIN, ALARM_SOURCE_MAX };
enum AlarmRegion { ALARM_REGION_ACTIVE, ALARM_REGION_FRAME, ALARM_REGION_ROI };

class M5Thermal2Component : public thermal_camera_core::ThermalCameraBase {
 public:
  // Configuration setters
  void set_refresh_rate(const std::string &rate) { refresh_rate_ = rate; }
  void set_noise_filter(uint8_t level) { noise_filter_ = level; }

  // Alarm configuration (static, from YAML)
  void set_status_led_enabled(bool enabled) { status_led_enabled_ = enabled; }
  void set_alarm_enabled(bool enabled) { alarm_enabled_ = enabled; }
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
  void update_buzzer_enabled(bool enabled) {
    buzzer_enabled_ = enabled;
    // Apply immediately during an audible beep so mute/unmute isn't delayed up
    // to a full beep interval.
    if (alarm_active_ && blink_on_)
      set_buzzer_(enabled ? alarm_buzzer_frequency_ : 0);
  }
  bool is_buzzer_enabled() const { return buzzer_enabled_; }
  void update_alarm_high_threshold(float t) { alarm_high_threshold_ = t; }
  void update_alarm_low_threshold(float t) { alarm_low_threshold_ = t; }

  // Sound one alarm cycle as a one-shot test (buzzer + red flash for one beep),
  // regardless of the mute switch. No-op while a real alarm is already active.
  void trigger_sound_test();

  // Sensor setters
  void set_alarm_active_sensor(binary_sensor::BinarySensor *sensor) { alarm_active_sensor_ = sensor; }
  void set_button_sensor(binary_sensor::BinarySensor *sensor) { button_sensor_ = sensor; }

  // Auto-generated control entity setters (device-specific controls)
  void set_buzzer_enabled_control(switch_::Switch *control) { buzzer_enabled_control_ = control; }
  void set_alarm_high_threshold_control(number::Number *control) { alarm_high_threshold_control_ = control; }
  void set_alarm_low_threshold_control(number::Number *control) { alarm_low_threshold_control_ = control; }

  bool is_alarm_active() const { return alarm_active_; }

  // State synchronization - extends the base ROI/update_interval sync with
  // the alarm-threshold controls.
  void sync_roi_state_from_controls() override;

 protected:
  // Virtual seams from ThermalCameraBase
  bool read_frame_() override;
  bool init_device_() override;
  const char *display_name() const override { return "M5Stack Thermal2"; }
  void dump_device_config_() override;
  void on_setup_() override;
  void on_update_tick_(uint32_t now) override;
  void on_frame_() override;
  void on_loop_(uint32_t now) override;
  void on_extra_number_control(int type, float value) override;
  void on_extra_switch_control(int type, bool value) override;

  // Low-level Thermal2 register access
  bool read_reg8_(uint8_t reg, uint8_t *value);
  bool write_reg8_(uint8_t reg, uint8_t value);
  bool write_reg16_(uint8_t reg, uint16_t value);

  void prime_frame_();
  bool read_subpage_pixels_();
  void store_subpage_(const uint16_t *raw, bool subpage);

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

  // Configuration
  std::string refresh_rate_{"16Hz"};
  uint8_t noise_filter_{8};

  // Alarm configuration
  bool alarm_enabled_{false};  // armed only when an `alarm:` block is configured
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

  // One-shot sound-test state
  bool sound_test_active_{false};
  uint32_t sound_test_start_{0};

  // Cached hardware output state (avoid redundant I2C writes)
  uint8_t function_ctrl_{0};
  int16_t last_led_r_{-1};
  int16_t last_led_g_{-1};
  int16_t last_led_b_{-1};
  int32_t last_buzzer_freq_{-1};

  // Hardware state
  bool frame_primed_{false};    // both subpages captured at least once
  uint8_t subpages_seen_{0};    // bit0 = subpage 0, bit1 = subpage 1

  // Raw subpage read buffer (class member to prevent stack overflow)
  uint16_t pixel_raw_[SUBPAGE_PIXELS]{};

  // Sensors (optional - for publishing to ESPHome)
  binary_sensor::BinarySensor *alarm_active_sensor_{nullptr};
  binary_sensor::BinarySensor *button_sensor_{nullptr};

  // Auto-generated control entities (optional, device-specific)
  switch_::Switch *buzzer_enabled_control_{nullptr};
  number::Number *alarm_high_threshold_control_{nullptr};
  number::Number *alarm_low_threshold_control_{nullptr};
};

// Device-specific control types, dispatched to via
// ThermalCameraBase::on_extra_number_control()/on_extra_switch_control().
enum M5Thermal2ControlType {
  BUZZER_ENABLED = thermal_camera_core::THERMAL_CONTROL_TYPE_EXTRA_START,
  ALARM_HIGH_THRESHOLD,
  ALARM_LOW_THRESHOLD,
};

// M5Thermal2Button - momentary button that sounds one alarm cycle as a test
class M5Thermal2Button : public button::Button {
 public:
  void set_m5stack_thermal2_parent(M5Thermal2Component *parent) { parent_ = parent; }

 protected:
  void press_action() override {
    if (parent_ != nullptr)
      parent_->trigger_sound_test();
  }

 private:
  M5Thermal2Component *parent_{nullptr};
};

}  // namespace m5stack_thermal2
}  // namespace esphome
