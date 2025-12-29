#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include <M5Unified.h>
#include "esphome/components/mlx90640/mlx90640.h"

namespace esphome {
namespace grow_env_monitor {

class GrowEnvMonitor : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_display_brightness(int brightness) { display_brightness_ = brightness; }
  void set_display_rotation(int rotation) { display_rotation_ = rotation; }

  void set_co2_sensor(sensor::Sensor *co2_sensor) { co2_sensor_ = co2_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }
  void set_thermal_min_sensor(sensor::Sensor *thermal_min_sensor) { thermal_min_sensor_ = thermal_min_sensor; }
  void set_thermal_max_sensor(sensor::Sensor *thermal_max_sensor) { thermal_max_sensor_ = thermal_max_sensor; }
  void set_thermal_avg_sensor(sensor::Sensor *thermal_avg_sensor) { thermal_avg_sensor_ = thermal_avg_sensor; }
  void set_roi_min_sensor(sensor::Sensor *roi_min_sensor) { roi_min_sensor_ = roi_min_sensor; }
  void set_roi_max_sensor(sensor::Sensor *roi_max_sensor) { roi_max_sensor_ = roi_max_sensor; }
  void set_roi_avg_sensor(sensor::Sensor *roi_avg_sensor) { roi_avg_sensor_ = roi_avg_sensor; }
  void set_light_sensor(binary_sensor::BinarySensor *light_sensor) { light_sensor_ = light_sensor; }
  void set_mlx90640_component(mlx90640::MLX90640Component *mlx90640) { mlx90640_component_ = mlx90640; }

  // Alert binary sensor setters
  void set_temp_high_alert_sensor(binary_sensor::BinarySensor *sensor) { temp_high_alert_sensor_ = sensor; }
  void set_temp_low_alert_sensor(binary_sensor::BinarySensor *sensor) { temp_low_alert_sensor_ = sensor; }
  void set_humidity_high_alert_sensor(binary_sensor::BinarySensor *sensor) { humidity_high_alert_sensor_ = sensor; }
  void set_humidity_low_alert_sensor(binary_sensor::BinarySensor *sensor) { humidity_low_alert_sensor_ = sensor; }

 protected:
  void draw_screen_();
  void draw_sensor_data_();
  void draw_thermal_data_();
  void draw_thermal_image_();
  void draw_roi_overlay_(int image_x, int image_y, int image_w, int image_h);
  void draw_alerts_();

  // Selective redraw functions
  void draw_co2_value_();
  void draw_temperature_value_();
  void draw_humidity_value_();
  void draw_vpd_value_();
  void draw_light_status_();
  void draw_thermal_values_();
  void check_and_update_sensor_values_();
  float calculate_vpd_(float temperature, float humidity);
  uint16_t get_status_color_(float co2, float temp, float humidity);

  bool initialized_{false};
  int display_brightness_{70};
  int display_rotation_{3};

  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *thermal_min_sensor_{nullptr};
  sensor::Sensor *thermal_max_sensor_{nullptr};
  sensor::Sensor *thermal_avg_sensor_{nullptr};
  sensor::Sensor *roi_min_sensor_{nullptr};
  sensor::Sensor *roi_max_sensor_{nullptr};
  sensor::Sensor *roi_avg_sensor_{nullptr};
  binary_sensor::BinarySensor *light_sensor_{nullptr};
  mlx90640::MLX90640Component *mlx90640_component_{nullptr};

  // Alert binary sensors
  binary_sensor::BinarySensor *temp_high_alert_sensor_{nullptr};
  binary_sensor::BinarySensor *temp_low_alert_sensor_{nullptr};
  binary_sensor::BinarySensor *humidity_high_alert_sensor_{nullptr};
  binary_sensor::BinarySensor *humidity_low_alert_sensor_{nullptr};

  // Previous values for selective redrawing
  float prev_co2_{NAN};
  float prev_temperature_{NAN};
  float prev_humidity_{NAN};
  float prev_thermal_min_{NAN};
  float prev_thermal_max_{NAN};
  float prev_thermal_avg_{NAN};
  bool prev_light_on_{false};

  // Update tracking
  uint32_t last_update_time_{0};
  static const uint32_t UPDATE_INTERVAL = 10000;  // 10 seconds for environmental data

  // Display layout constants
  static const int SCREEN_WIDTH = 320;
  static const int SCREEN_HEIGHT = 240;
  static const int MARGIN = 10;
  static const int LINE_HEIGHT = 25;

  // Color constants
  static const uint16_t COLOR_BACKGROUND = 0x0000;  // Black
  static const uint16_t COLOR_TEXT = 0xFFFF;        // White
  static const uint16_t COLOR_GOOD = 0x07E0;        // Green
  static const uint16_t COLOR_WARNING = 0xFFE0;     // Yellow
  static const uint16_t COLOR_ALERT = 0xF800;       // Red
  static const uint16_t COLOR_HEADER = 0x07FF;      // Cyan
};

}  // namespace grow_env_monitor
}  // namespace esphome
