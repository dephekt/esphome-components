#include "scd4x_alerts.h"
#include "esphome/core/log.h"

namespace esphome {
namespace scd4x_alerts {

static const char *TAG = "scd4x_alerts";

void SCD4xAlerts::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SCD4x Alerts...");
  ESP_LOGCONFIG(TAG, "SCD4x Alerts setup complete");
}

void SCD4xAlerts::loop() {
  uint32_t now = millis();

  if (now - last_update_time_ > UPDATE_INTERVAL) {
    update_alerts_();
    last_update_time_ = now;
  }
}

void SCD4xAlerts::dump_config() {
  ESP_LOGCONFIG(TAG, "SCD4x Alerts:");
  ESP_LOGCONFIG(TAG, "  CO2 High Threshold: %.0f ppm", co2_high_threshold_);
  ESP_LOGCONFIG(TAG, "  CO2 Low Threshold: %.0f ppm", co2_low_threshold_);
  ESP_LOGCONFIG(TAG, "  Temperature High Threshold: %.1f°C", temp_high_threshold_);
  ESP_LOGCONFIG(TAG, "  Temperature Low Threshold: %.1f°C", temp_low_threshold_);
  ESP_LOGCONFIG(TAG, "  Humidity High Threshold: %.1f%%", humidity_high_threshold_);
  ESP_LOGCONFIG(TAG, "  Humidity Low Threshold: %.1f%%", humidity_low_threshold_);
  ESP_LOGCONFIG(TAG, "  VPD High Threshold: %.2f kPa", vpd_high_threshold_);
  ESP_LOGCONFIG(TAG, "  VPD Low Threshold: %.2f kPa", vpd_low_threshold_);

  ESP_LOGCONFIG(TAG, "  Alert Sensors:");
  ESP_LOGCONFIG(TAG, "    CO2 High Alert: %s", YESNO(co2_high_alert_sensor_ != nullptr));
  ESP_LOGCONFIG(TAG, "    CO2 Low Alert: %s", YESNO(co2_low_alert_sensor_ != nullptr));
  ESP_LOGCONFIG(TAG, "    Temperature High Alert: %s", YESNO(temp_high_alert_sensor_ != nullptr));
  ESP_LOGCONFIG(TAG, "    Temperature Low Alert: %s", YESNO(temp_low_alert_sensor_ != nullptr));
  ESP_LOGCONFIG(TAG, "    Humidity High Alert: %s", YESNO(humidity_high_alert_sensor_ != nullptr));
  ESP_LOGCONFIG(TAG, "    Humidity Low Alert: %s", YESNO(humidity_low_alert_sensor_ != nullptr));
  ESP_LOGCONFIG(TAG, "    VPD High Alert: %s", YESNO(vpd_high_alert_sensor_ != nullptr));
  ESP_LOGCONFIG(TAG, "    VPD Low Alert: %s", YESNO(vpd_low_alert_sensor_ != nullptr));
}

void SCD4xAlerts::update_alerts_() {
  // Sync thresholds from number entities if they exist
  if (co2_high_threshold_number_ != nullptr && co2_high_threshold_number_->has_state()) {
    co2_high_threshold_ = co2_high_threshold_number_->state;
  }
  if (co2_low_threshold_number_ != nullptr && co2_low_threshold_number_->has_state()) {
    co2_low_threshold_ = co2_low_threshold_number_->state;
  }
  if (temp_high_threshold_number_ != nullptr && temp_high_threshold_number_->has_state()) {
    temp_high_threshold_ = temp_high_threshold_number_->state;
  }
  if (temp_low_threshold_number_ != nullptr && temp_low_threshold_number_->has_state()) {
    temp_low_threshold_ = temp_low_threshold_number_->state;
  }
  if (humidity_high_threshold_number_ != nullptr && humidity_high_threshold_number_->has_state()) {
    humidity_high_threshold_ = humidity_high_threshold_number_->state;
  }
  if (humidity_low_threshold_number_ != nullptr && humidity_low_threshold_number_->has_state()) {
    humidity_low_threshold_ = humidity_low_threshold_number_->state;
  }
  if (vpd_high_threshold_number_ != nullptr && vpd_high_threshold_number_->has_state()) {
    vpd_high_threshold_ = vpd_high_threshold_number_->state;
  }
  if (vpd_low_threshold_number_ != nullptr && vpd_low_threshold_number_->has_state()) {
    vpd_low_threshold_ = vpd_low_threshold_number_->state;
  }

  update_co2_alerts_();
  update_temperature_alerts_();
  update_humidity_alerts_();
  update_vpd_alerts_();
}

void SCD4xAlerts::update_co2_alerts_() {
  if (co2_sensor_ == nullptr || !co2_sensor_->has_state())
    return;

  float co2 = co2_sensor_->state;

  // CO2 High Alert
  if (co2_high_alert_sensor_ != nullptr) {
    bool condition = co2 > co2_high_threshold_;
    bool alert_state = check_alert_with_delay_(condition, co2_high_last_state_, co2_high_last_change_, CO2_ALERT_DELAY);
    co2_high_alert_sensor_->publish_state(alert_state);
  }

  // CO2 Low Alert
  if (co2_low_alert_sensor_ != nullptr) {
    bool condition = co2 < co2_low_threshold_;
    bool alert_state = check_alert_with_delay_(condition, co2_low_last_state_, co2_low_last_change_, CO2_ALERT_DELAY);
    co2_low_alert_sensor_->publish_state(alert_state);
  }
}

void SCD4xAlerts::update_temperature_alerts_() {
  if (temperature_sensor_ == nullptr || !temperature_sensor_->has_state())
    return;

  float temperature = temperature_sensor_->state;

  // Temperature High Alert
  if (temp_high_alert_sensor_ != nullptr) {
    bool condition = temperature > temp_high_threshold_;
    bool alert_state =
        check_alert_with_delay_(condition, temp_high_last_state_, temp_high_last_change_, TEMP_ALERT_DELAY);
    temp_high_alert_sensor_->publish_state(alert_state);
  }

  // Temperature Low Alert
  if (temp_low_alert_sensor_ != nullptr) {
    bool condition = temperature < temp_low_threshold_;
    bool alert_state =
        check_alert_with_delay_(condition, temp_low_last_state_, temp_low_last_change_, TEMP_ALERT_DELAY);
    temp_low_alert_sensor_->publish_state(alert_state);
  }
}

void SCD4xAlerts::update_humidity_alerts_() {
  if (humidity_sensor_ == nullptr || !humidity_sensor_->has_state())
    return;

  float humidity = humidity_sensor_->state;

  // Humidity High Alert
  if (humidity_high_alert_sensor_ != nullptr) {
    bool condition = humidity > humidity_high_threshold_;
    bool alert_state =
        check_alert_with_delay_(condition, humidity_high_last_state_, humidity_high_last_change_, HUMIDITY_ALERT_DELAY);
    humidity_high_alert_sensor_->publish_state(alert_state);
  }

  // Humidity Low Alert
  if (humidity_low_alert_sensor_ != nullptr) {
    bool condition = humidity < humidity_low_threshold_;
    bool alert_state =
        check_alert_with_delay_(condition, humidity_low_last_state_, humidity_low_last_change_, HUMIDITY_ALERT_DELAY);
    humidity_low_alert_sensor_->publish_state(alert_state);
  }
}

void SCD4xAlerts::update_vpd_alerts_() {
  if (vpd_sensor_ == nullptr || !vpd_sensor_->has_state())
    return;

  float vpd = vpd_sensor_->state;

  // VPD High Alert
  if (vpd_high_alert_sensor_ != nullptr) {
    bool condition = vpd > vpd_high_threshold_;
    bool alert_state = check_alert_with_delay_(condition, vpd_high_last_state_, vpd_high_last_change_, VPD_ALERT_DELAY);
    vpd_high_alert_sensor_->publish_state(alert_state);
  }

  // VPD Low Alert
  if (vpd_low_alert_sensor_ != nullptr) {
    bool condition = vpd < vpd_low_threshold_;
    bool alert_state = check_alert_with_delay_(condition, vpd_low_last_state_, vpd_low_last_change_, VPD_ALERT_DELAY);
    vpd_low_alert_sensor_->publish_state(alert_state);
  }
}

bool SCD4xAlerts::check_alert_with_delay_(bool condition, bool &last_state, uint32_t &last_change_time,
                                          uint32_t delay_ms) {
  uint32_t now = millis();

  if (condition != last_state) {
    last_state = condition;
    last_change_time = now;
    return false;  // Don't trigger immediately on state change
  }

  // If condition has been stable for delay_ms, return the condition
  if (now - last_change_time > delay_ms) {
    return condition;
  }

  return false;  // Still within delay period
}

}  // namespace scd4x_alerts
}  // namespace esphome