#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/time/real_time_clock.h"

namespace esphome {
namespace scd4x_stats {

class SCD4xStats : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_co2_sensor(sensor::Sensor *co2_sensor) { co2_sensor_ = co2_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }
  void set_time_component(time::RealTimeClock *time_component) { time_component_ = time_component; }

  void set_vpd_sensor(sensor::Sensor *vpd_sensor) { vpd_sensor_ = vpd_sensor; }
  void set_daily_max_co2_sensor(sensor::Sensor *daily_max_co2_sensor) { daily_max_co2_sensor_ = daily_max_co2_sensor; }
  void set_daily_min_temp_sensor(sensor::Sensor *daily_min_temp_sensor) {
    daily_min_temp_sensor_ = daily_min_temp_sensor;
  }
  void set_daily_max_temp_sensor(sensor::Sensor *daily_max_temp_sensor) {
    daily_max_temp_sensor_ = daily_max_temp_sensor;
  }
  void set_co2_moving_avg_sensor(sensor::Sensor *co2_moving_avg_sensor) {
    co2_moving_avg_sensor_ = co2_moving_avg_sensor;
  }
  void set_temp_moving_avg_sensor(sensor::Sensor *temp_moving_avg_sensor) {
    temp_moving_avg_sensor_ = temp_moving_avg_sensor;
  }

 protected:
  void update_sensors_();
  void update_vpd_();
  void update_daily_stats_();
  void update_moving_averages_();
  void reset_daily_stats_();
  float calculate_vpd_(float temperature, float humidity);
  bool is_midnight_();

  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  time::RealTimeClock *time_component_{nullptr};

  sensor::Sensor *vpd_sensor_{nullptr};
  sensor::Sensor *daily_max_co2_sensor_{nullptr};
  sensor::Sensor *daily_min_temp_sensor_{nullptr};
  sensor::Sensor *daily_max_temp_sensor_{nullptr};
  sensor::Sensor *co2_moving_avg_sensor_{nullptr};
  sensor::Sensor *temp_moving_avg_sensor_{nullptr};

  // Daily statistics tracking
  float daily_max_co2_{0.0f};
  float daily_min_temp_{100.0f};
  float daily_max_temp_{0.0f};

  // Moving averages (1 minute window = 12 samples at 5s intervals)
  static const int MOVING_AVG_SAMPLES = 12;
  float co2_values_[MOVING_AVG_SAMPLES] = {0};
  float temp_values_[MOVING_AVG_SAMPLES] = {0};
  int moving_avg_index_{0};

  // Update timing
  uint32_t last_update_time_{0};
  uint32_t last_moving_avg_update_{0};
  static const uint32_t UPDATE_INTERVAL = 30000;     // 30 seconds
  static const uint32_t MOVING_AVG_INTERVAL = 5000;  // 5 seconds

  int last_reset_day_{-1};
};

}  // namespace scd4x_stats
}  // namespace esphome