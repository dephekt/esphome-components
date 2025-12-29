#include "scd4x_stats.h"
#include "esphome/core/log.h"
#include "esphome/components/time/real_time_clock.h"
#include <cmath>

namespace esphome {
namespace scd4x_stats {

static const char *TAG = "scd4x_stats";

void SCD4xStats::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SCD4x Statistics...");

  // Initialize daily stats
  daily_max_co2_ = 0.0f;
  daily_min_temp_ = 100.0f;
  daily_max_temp_ = 0.0f;

  // Initialize moving averages
  for (int i = 0; i < MOVING_AVG_SAMPLES; i++) {
    co2_values_[i] = 0.0f;
    temp_values_[i] = 0.0f;
  }
  moving_avg_index_ = 0;

  ESP_LOGCONFIG(TAG, "SCD4x Statistics setup complete");
}

void SCD4xStats::loop() {
  uint32_t now = millis();

  // Check for midnight reset
  if (is_midnight_()) {
    reset_daily_stats_();
  }

  // Update moving averages more frequently
  if (now - last_moving_avg_update_ > MOVING_AVG_INTERVAL) {
    update_moving_averages_();
    last_moving_avg_update_ = now;
  }

  // Update other sensors less frequently
  if (now - last_update_time_ > UPDATE_INTERVAL) {
    update_sensors_();
    last_update_time_ = now;
  }
}

void SCD4xStats::dump_config() {
  ESP_LOGCONFIG(TAG, "SCD4x Statistics:");
  ESP_LOGCONFIG(TAG, "  VPD Sensor: %s", YESNO(vpd_sensor_ != nullptr));
  ESP_LOGCONFIG(TAG, "  Daily Max CO2 Sensor: %s", YESNO(daily_max_co2_sensor_ != nullptr));
  ESP_LOGCONFIG(TAG, "  Daily Min Temperature Sensor: %s", YESNO(daily_min_temp_sensor_ != nullptr));
  ESP_LOGCONFIG(TAG, "  Daily Max Temperature Sensor: %s", YESNO(daily_max_temp_sensor_ != nullptr));
  ESP_LOGCONFIG(TAG, "  CO2 Moving Average Sensor: %s", YESNO(co2_moving_avg_sensor_ != nullptr));
  ESP_LOGCONFIG(TAG, "  Temperature Moving Average Sensor: %s", YESNO(temp_moving_avg_sensor_ != nullptr));
}

void SCD4xStats::update_sensors_() {
  update_vpd_();
  update_daily_stats_();
}

void SCD4xStats::update_vpd_() {
  if (vpd_sensor_ == nullptr || temperature_sensor_ == nullptr || humidity_sensor_ == nullptr)
    return;

  if (temperature_sensor_->has_state() && humidity_sensor_->has_state()) {
    float temperature = temperature_sensor_->state;
    float humidity = humidity_sensor_->state;

    float vpd = calculate_vpd_(temperature, humidity);
    if (!isnan(vpd)) {
      vpd_sensor_->publish_state(vpd);
    }
  }
}

void SCD4xStats::update_daily_stats_() {
  // Update daily max CO2
  if (daily_max_co2_sensor_ != nullptr && co2_sensor_ != nullptr && co2_sensor_->has_state()) {
    float co2 = co2_sensor_->state;
    if (co2 > daily_max_co2_) {
      daily_max_co2_ = co2;
    }
    daily_max_co2_sensor_->publish_state(daily_max_co2_);
  }

  // Update daily min/max temperature
  if (temperature_sensor_ != nullptr && temperature_sensor_->has_state()) {
    float temp = temperature_sensor_->state;

    if (temp < daily_min_temp_) {
      daily_min_temp_ = temp;
    }
    if (temp > daily_max_temp_) {
      daily_max_temp_ = temp;
    }

    if (daily_min_temp_sensor_ != nullptr) {
      daily_min_temp_sensor_->publish_state(daily_min_temp_);
    }
    if (daily_max_temp_sensor_ != nullptr) {
      daily_max_temp_sensor_->publish_state(daily_max_temp_);
    }
  }
}

void SCD4xStats::update_moving_averages_() {
  // Update CO2 moving average
  if (co2_moving_avg_sensor_ != nullptr && co2_sensor_ != nullptr && co2_sensor_->has_state()) {
    co2_values_[moving_avg_index_] = co2_sensor_->state;

    float sum = 0;
    for (int i = 0; i < MOVING_AVG_SAMPLES; i++) {
      sum += co2_values_[i];
    }
    co2_moving_avg_sensor_->publish_state(sum / MOVING_AVG_SAMPLES);
  }

  // Update temperature moving average
  if (temp_moving_avg_sensor_ != nullptr && temperature_sensor_ != nullptr && temperature_sensor_->has_state()) {
    temp_values_[moving_avg_index_] = temperature_sensor_->state;

    float sum = 0;
    for (int i = 0; i < MOVING_AVG_SAMPLES; i++) {
      sum += temp_values_[i];
    }
    temp_moving_avg_sensor_->publish_state(sum / MOVING_AVG_SAMPLES);
  }

  // Advance moving average index
  moving_avg_index_ = (moving_avg_index_ + 1) % MOVING_AVG_SAMPLES;
}

void SCD4xStats::reset_daily_stats_() {
  ESP_LOGD(TAG, "Resetting daily statistics");

  daily_max_co2_ = 0.0f;
  daily_min_temp_ = 100.0f;
  daily_max_temp_ = 0.0f;

  // Mark current day as reset day
  if (time_component_ != nullptr) {
    auto now = time_component_->now();
    if (now.is_valid()) {
      last_reset_day_ = now.day_of_year;
    }
  }
}

float SCD4xStats::calculate_vpd_(float temperature, float humidity) {
  if (isnan(temperature) || isnan(humidity)) {
    return NAN;
  }

  // Calculate VPD using standard formula
  // SVP = 0.6108 * exp((17.27 * T) / (T + 237.3))
  // VPD = SVP * (1 - RH/100)
  float svp = 0.6108f * exp((17.27f * temperature) / (temperature + 237.3f));
  float vpd = svp * (1.0f - humidity / 100.0f);

  return vpd;
}

bool SCD4xStats::is_midnight_() {
  if (time_component_ == nullptr)
    return false;

  auto now = time_component_->now();
  if (!now.is_valid())
    return false;

  // Check if it's midnight (00:00) and we haven't reset today
  if (now.hour == 0 && now.minute == 0 && now.day_of_year != last_reset_day_) {
    return true;
  }

  return false;
}

}  // namespace scd4x_stats
}  // namespace esphome